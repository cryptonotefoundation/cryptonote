// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers, The Monero developers
// Copyright (c) 2018, Ryo Currency Project
// Copyright (c) 2016-2020, The Karbo developers
//
// This file is part of Karbo.
//
// Karbo is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Karbo is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Karbo.  If not, see <http://www.gnu.org/licenses/>.

#include "Blockchain.h"

#include <algorithm>
#include <numeric>
#include <cstdio>
#include <cmath>
#include <boost/foreach.hpp>
#include "Common/Math.h"
#include "Common/int-util.h"
#include "Common/ShuffleGenerator.h"
#include "Common/StdInputStream.h"
#include "Common/StdOutputStream.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"
#include "Serialization/BinarySerializationTools.h"
#include "CryptoNoteTools.h"
#include "TransactionExtra.h"
#include "parallel_hashmap/phmap_dump.h"

using namespace Logging;
using namespace Common;

namespace {

std::string appendPath(const std::string& path, const std::string& fileName) {
  std::string result = path;
  if (!result.empty()) {
    result += '/';
  }

  result += fileName;
  return result;
}

}

namespace std {
bool operator<(const Crypto::Hash& hash1, const Crypto::Hash& hash2) {
  return memcmp(&hash1, &hash2, Crypto::HASH_SIZE) < 0;
}

bool operator<(const Crypto::KeyImage& keyImage1, const Crypto::KeyImage& keyImage2) {
  return memcmp(&keyImage1, &keyImage2, 32) < 0;
}
}

#define CURRENT_BLOCKCACHE_STORAGE_ARCHIVE_VER 3
#define CURRENT_BLOCKCHAININDICES_STORAGE_ARCHIVE_VER 1

namespace CryptoNote {
class BlockCacheSerializer;
class BlockchainIndicesSerializer;
}

namespace CryptoNote {

template<typename K, typename V, typename Hash>
bool serialize(google::sparse_hash_map<K, V, Hash>& value, Common::StringView name, CryptoNote::ISerializer& serializer) {
  return serializeMap(value, name, serializer, [&value](size_t size) { value.resize(size); });
}

template<typename K, typename Hash>
bool serialize(google::sparse_hash_set<K, Hash>& value, Common::StringView name, CryptoNote::ISerializer& serializer) {
  size_t size = value.size();
  if (!serializer.beginArray(size, name)) {
    return false;
  }

  if (serializer.type() == ISerializer::OUTPUT) {
    for (auto& key : value) {
      serializer(const_cast<K&>(key), "");
    }
  } else {
    value.resize(size);
    while (size--) {
      K key;
      serializer(key, "");
      value.insert(key);
    }
  }

  serializer.endArray();
  return true;
}

// custom serialization to speedup cache loading
bool serialize(std::vector<std::pair<Blockchain::TransactionIndex, uint16_t>>& value, Common::StringView name, CryptoNote::ISerializer& s) {
  const size_t elementSize = sizeof(std::pair<Blockchain::TransactionIndex, uint16_t>);
  size_t size = value.size() * elementSize;

  if (!s.beginArray(size, name)) {
    return false;
  }

  if (s.type() == CryptoNote::ISerializer::INPUT) {
    if (size % elementSize != 0) {
      throw std::runtime_error("Invalid vector size");
    }
    value.resize(size / elementSize);
  }

  if (size) {
    s.binary(value.data(), size, "");
  }

  s.endArray();
  return true;
}

void serialize(Blockchain::TransactionIndex& value, ISerializer& s) {
  s(value.block, "block");
  s(value.transaction, "tx");
}

class BlockCacheSerializer {

public:
  BlockCacheSerializer(Blockchain& bs, const Crypto::Hash lastBlockHash, ILogger& logger) :
    m_bs(bs), m_lastBlockHash(lastBlockHash), m_loaded(false), logger(logger, "BlockCacheSerializer") {
  }

  void load(const std::string& filename) {
    try {
      std::ifstream stdStream(filename, std::ios::binary);
      if (!stdStream) {
        return;
      }

      StdInputStream stream(stdStream);
      BinaryInputStreamSerializer s(stream);
      CryptoNote::serialize(*this, s);
    } catch (std::exception& e) {
      logger(WARNING) << "loading failed: " << e.what();
    }
  }

  bool save(const std::string& filename) {
    try {
      std::ofstream file(filename, std::ios::binary);
      if (!file) {
        return false;
      }

      StdOutputStream stream(file);
      BinaryOutputStreamSerializer s(stream);
      CryptoNote::serialize(*this, s);
    } catch (std::exception&) {
      return false;
    }

    return true;
  }

  void serialize(ISerializer& s) {
    auto start = std::chrono::steady_clock::now();

    uint8_t version = CURRENT_BLOCKCACHE_STORAGE_ARCHIVE_VER;
    s(version, "version");

    // ignore old versions, do rebuild
    if (version < CURRENT_BLOCKCACHE_STORAGE_ARCHIVE_VER)
      return;

    std::string operation;
    if (s.type() == ISerializer::INPUT) {
      operation = "- loading ";
      Crypto::Hash blockHash;
      s(blockHash, "last_block");

      if (blockHash != m_lastBlockHash) {
        return;
      }

    } else {
      operation = "- saving ";
      s(m_lastBlockHash, "last_block");
    }

    logger(INFO) << operation << "block index...";
    s(m_bs.m_blockIndex, "block_index");

    logger(INFO) << operation << "transaction map...";
    //s(m_bs.m_transactionMap, "transactions");
    if (s.type() == ISerializer::INPUT) {
      phmap::BinaryInputArchive ar_in(appendPath(m_bs.m_config_folder, "transactionsmap.dat").c_str());
      m_bs.m_transactionMap.load(ar_in);
    }
    else {
      phmap::BinaryOutputArchive ar_out(appendPath(m_bs.m_config_folder, "transactionsmap.dat").c_str());
      m_bs.m_transactionMap.dump(ar_out);
    }

    logger(INFO) << operation << "spent keys...";
    //s(m_bs.m_spent_key_images, "spent_keys");
    if (s.type() == ISerializer::INPUT) {
      phmap::BinaryInputArchive ar_in(appendPath(m_bs.m_config_folder, "spentkeys.dat").c_str());
      m_bs.m_spent_key_images.load(ar_in);
    }
    else {
      phmap::BinaryOutputArchive ar_out(appendPath(m_bs.m_config_folder, "spentkeys.dat").c_str());
      m_bs.m_spent_key_images.dump(ar_out);
    }

    logger(INFO) << operation << "outputs...";
    s(m_bs.m_outputs, "outputs");

    logger(INFO) << operation << "multi-signature outputs...";
    s(m_bs.m_multisignatureOutputs, "multisig_outputs");

    auto dur = std::chrono::steady_clock::now() - start;

    logger(INFO) << "Serialization time: " << std::chrono::duration_cast<std::chrono::milliseconds>(dur).count() << "ms";

    m_loaded = true;
  }

  bool loaded() const {
    return m_loaded;
  }

private:

  LoggerRef logger;
  bool m_loaded;
  Blockchain& m_bs;
  Crypto::Hash m_lastBlockHash;
};

class BlockchainIndicesSerializer {

public:
  BlockchainIndicesSerializer(Blockchain& bs, const Crypto::Hash lastBlockHash, ILogger& logger) :
    m_bs(bs), m_lastBlockHash(lastBlockHash), m_loaded(false), logger(logger, "BlockchainIndicesSerializer") {
  }

  void serialize(ISerializer& s) {

    uint8_t version = CURRENT_BLOCKCHAININDICES_STORAGE_ARCHIVE_VER;

    KV_MEMBER(version);

    // ignore old versions, do rebuild
    if (version != CURRENT_BLOCKCHAININDICES_STORAGE_ARCHIVE_VER)
      return;

    std::string operation;

    if (s.type() == ISerializer::INPUT) {
      operation = "- loading ";

      Crypto::Hash blockHash;
      s(blockHash, "blockHash");

      if (blockHash != m_lastBlockHash) {
        return;
      }

    } else {
      operation = "- saving ";
      s(m_lastBlockHash, "blockHash");
    }

    logger(INFO) << operation << "paymentID index...";
    s(m_bs.m_paymentIdIndex, "paymentIdIndex");

    logger(INFO) << operation << "timestamp index...";
    s(m_bs.m_timestampIndex, "timestampIndex");

    logger(INFO) << operation << "generated transactions index...";
    s(m_bs.m_generatedTransactionsIndex, "generatedTransactionsIndex");

    m_loaded = true;
  }

  template<class Archive> void serialize(Archive& ar, unsigned int version) {

    // ignore old versions, do rebuild
    if (version < CURRENT_BLOCKCHAININDICES_STORAGE_ARCHIVE_VER)
      return;

    std::string operation;
    if (Archive::is_loading::value) {
      operation = "- loading ";
      Crypto::Hash blockHash;
      ar & blockHash;

      if (blockHash != m_lastBlockHash) {
        return;
      }

    } else {
      operation = "- saving ";
      ar & m_lastBlockHash;
    }

    logger(INFO) << operation << "paymentID index...";
    ar & m_bs.m_paymentIdIndex;

    logger(INFO) << operation << "timestamp index...";
    ar & m_bs.m_timestampIndex;

    logger(INFO) << operation << "generated transactions index...";
    ar & m_bs.m_generatedTransactionsIndex;

    m_loaded = true;
  }

  bool loaded() const {
    return m_loaded;
  }

private:

  LoggerRef logger;
  bool m_loaded;
  Blockchain& m_bs;
  Crypto::Hash m_lastBlockHash;
};


Blockchain::Blockchain(const Currency& currency, tx_memory_pool& tx_pool, ILogger& logger, bool blockchainIndexesEnabled) :
logger(logger, "Blockchain"),
m_currency(currency),
m_tx_pool(tx_pool),
m_current_block_cumul_sz_limit(0),
m_upgradeDetectorV2(currency, m_blocks, BLOCK_MAJOR_VERSION_2, logger),
m_upgradeDetectorV3(currency, m_blocks, BLOCK_MAJOR_VERSION_3, logger),
m_upgradeDetectorV4(currency, m_blocks, BLOCK_MAJOR_VERSION_4, logger),
m_upgradeDetectorV5(currency, m_blocks, BLOCK_MAJOR_VERSION_5, logger),
m_checkpoints(logger),
m_paymentIdIndex(blockchainIndexesEnabled),
m_timestampIndex(blockchainIndexesEnabled),
m_generatedTransactionsIndex(blockchainIndexesEnabled),
m_orphanBlocksIndex(blockchainIndexesEnabled),
m_blockchainIndexesEnabled(blockchainIndexesEnabled) {
}

bool Blockchain::addObserver(IBlockchainStorageObserver* observer) {
  return m_observerManager.add(observer);
}

bool Blockchain::removeObserver(IBlockchainStorageObserver* observer) {
  return m_observerManager.remove(observer);
}

bool Blockchain::checkTransactionInputs(const CryptoNote::Transaction& tx, BlockInfo& maxUsedBlock) {
  return checkTransactionInputs(tx, maxUsedBlock.height, maxUsedBlock.id);
}

bool Blockchain::checkTransactionInputs(const CryptoNote::Transaction& tx, BlockInfo& maxUsedBlock, BlockInfo& lastFailed) {

  BlockInfo tail;

  //not the best implementation at this time, sorry :(
  //check is ring_signature already checked ?
  if (maxUsedBlock.empty()) {
    //not checked, lets try to check
    if (!lastFailed.empty() && getCurrentBlockchainHeight() > lastFailed.height && getBlockIdByHeight(lastFailed.height) == lastFailed.id) {
      return false; //we already sure that this tx is broken for this height
    }

    if (!checkTransactionInputs(tx, maxUsedBlock.height, maxUsedBlock.id, &tail)) {
      lastFailed = tail;
      return false;
    }
  } else {
    if (maxUsedBlock.height >= getCurrentBlockchainHeight()) {
      return false;
    }

    if (getBlockIdByHeight(maxUsedBlock.height) != maxUsedBlock.id) {
      //if we already failed on this height and id, skip actual ring signature check
      if (lastFailed.id == getBlockIdByHeight(lastFailed.height)) {
        return false;
      }
    }

    //check ring signature again, it is possible (with very small chance) that this transaction become again valid
    if (!checkTransactionInputs(tx, maxUsedBlock.height, maxUsedBlock.id, &tail)) {
      lastFailed = tail;
      return false;
    }
  }

  return true;
}

bool Blockchain::haveSpentKeyImages(const CryptoNote::Transaction& tx) {
  return this->haveTransactionKeyImagesAsSpent(tx);
}

/**
* \pre m_blockchain_lock is locked
*/
bool Blockchain::checkTransactionSize(size_t blobSize) {
  if (blobSize > getCurrentCumulativeBlocksizeLimit() - m_currency.minerTxBlobReservedSize()) {
    logger(ERROR) << "transaction is too big " << blobSize << ", maximum allowed size is " <<
      (getCurrentCumulativeBlocksizeLimit() - m_currency.minerTxBlobReservedSize());
    return false;
  }

  return true;
}

bool Blockchain::haveTransaction(const Crypto::Hash &id) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return m_transactionMap.find(id) != m_transactionMap.end();
}

bool Blockchain::have_tx_keyimg_as_spent(const Crypto::KeyImage &key_im) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return  checkIfSpent(key_im);
}

bool Blockchain::checkIfSpent(const Crypto::KeyImage& keyImage, uint32_t blockIndex) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  
  auto it = m_spent_key_images.find(keyImage);
  if (it == m_spent_key_images.end()) {
    return false;
  }

  return it->second <= blockIndex;
}

bool Blockchain::checkIfSpent(const Crypto::KeyImage& keyImage) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (m_spent_key_images.count(keyImage) != 0) {
    return true;
  }

  return false;
}

uint32_t Blockchain::getCurrentBlockchainHeight() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return static_cast<uint32_t>(m_blocks.size());
}

bool Blockchain::init(const std::string& config_folder, bool load_existing) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (!config_folder.empty() && !Tools::create_directories_if_necessary(config_folder)) {
    logger(ERROR, BRIGHT_RED) << "Failed to create data directory: " << m_config_folder;
    return false;
  }

  m_config_folder = config_folder;

  if (!m_blocks.open(appendPath(config_folder, m_currency.blocksFileName()), appendPath(config_folder, m_currency.blockIndexesFileName()), 1024)) {
    return false;
  }

  if (load_existing && !m_blocks.empty()) {
    logger(INFO, BRIGHT_WHITE) << "Loading blockchain...";
    BlockCacheSerializer loader(*this, get_block_hash(m_blocks.back().bl), logger.getLogger());
    loader.load(appendPath(config_folder, m_currency.blocksCacheFileName()));

    if (!loader.loaded()) {
      logger(WARNING, BRIGHT_YELLOW) << "No actual blockchain cache found, rebuilding internal structures...";
      rebuildCache();
    }

    if (m_blockchainIndexesEnabled) {
      loadBlockchainIndices();
    }
  } else {
    m_blocks.clear();
  }

  if (m_blocks.empty()) {
    logger(INFO, BRIGHT_WHITE)
      << "Blockchain not loaded, generating genesis block.";
    block_verification_context bvc = boost::value_initialized<block_verification_context>();
    pushBlock(m_currency.genesisBlock(), get_block_hash(m_currency.genesisBlock()), bvc);
    if (bvc.m_verification_failed) {
      logger(ERROR, BRIGHT_RED) << "Failed to add genesis block to blockchain";
      return false;
    }
  } else {
    Crypto::Hash firstBlockHash = get_block_hash(m_blocks[0].bl);
    if (!(firstBlockHash == m_currency.genesisBlockHash())) {
      logger(ERROR, BRIGHT_RED) << "Failed to init: genesis block mismatch. "
        "Probably you set --testnet flag with data "
        "dir with non-test blockchain or another "
        "network.";
      return false;
    }
  }

  uint32_t lastValidCheckpointHeight = 0;
  if (!checkCheckpoints(lastValidCheckpointHeight)) {
    logger(WARNING, BRIGHT_YELLOW) << "Invalid checkpoint found. Rollback blockchain to height=" << lastValidCheckpointHeight;
    rollbackBlockchainTo(lastValidCheckpointHeight);
  }

  if (!m_upgradeDetectorV2.init() || !m_upgradeDetectorV3.init() || !m_upgradeDetectorV4.init() || !m_upgradeDetectorV5.init()/* || !m_upgradeDetectorV6.init()*/) {
    logger(ERROR, BRIGHT_RED) << "Failed to initialize upgrade detector. Trying self healing procedure.";
    //return false;
  }

  bool reinitUpgradeDetectors = false;
  if (!checkUpgradeHeight(m_upgradeDetectorV2)) {
    uint32_t upgradeHeight = m_upgradeDetectorV2.upgradeHeight();
    assert(upgradeHeight != UpgradeDetectorBase::UNDEF_HEIGHT);
    logger(WARNING, BRIGHT_YELLOW) << "Invalid block version at " << upgradeHeight + 1 << ": real=" << static_cast<int>(m_blocks[upgradeHeight + 1].bl.majorVersion) <<
      " expected=" << static_cast<int>(m_upgradeDetectorV2.targetVersion()) << ". Rollback blockchain to height=" << upgradeHeight;
    rollbackBlockchainTo(upgradeHeight);
    reinitUpgradeDetectors = true;
  } else if (!checkUpgradeHeight(m_upgradeDetectorV3)) {
    uint32_t upgradeHeight = m_upgradeDetectorV3.upgradeHeight();
    logger(WARNING, BRIGHT_YELLOW) << "Invalid block version at " << upgradeHeight + 1 << ": real=" << static_cast<int>(m_blocks[upgradeHeight + 1].bl.majorVersion) <<
      " expected=" << static_cast<int>(m_upgradeDetectorV3.targetVersion()) << ". Rollback blockchain to height=" << upgradeHeight;
    rollbackBlockchainTo(upgradeHeight);
    reinitUpgradeDetectors = true;
  } else if (!checkUpgradeHeight(m_upgradeDetectorV4)) {
    uint32_t upgradeHeight = m_upgradeDetectorV4.upgradeHeight();
    logger(WARNING, BRIGHT_YELLOW) << "Invalid block version at " << upgradeHeight + 1 << ": real=" << static_cast<int>(m_blocks[upgradeHeight + 1].bl.majorVersion) <<
      " expected=" << static_cast<int>(m_upgradeDetectorV4.targetVersion()) << ". Rollback blockchain to height=" << upgradeHeight;
    rollbackBlockchainTo(upgradeHeight);
    reinitUpgradeDetectors = true;
	} else if (!checkUpgradeHeight(m_upgradeDetectorV5)) {
    uint32_t upgradeHeight = m_upgradeDetectorV5.upgradeHeight();
    logger(WARNING, BRIGHT_YELLOW) << "Invalid block version at " << upgradeHeight + 1 << ": real=" << static_cast<int>(m_blocks[upgradeHeight + 1].bl.majorVersion) <<
      " expected=" << static_cast<int>(m_upgradeDetectorV5.targetVersion()) << ". Rollback blockchain to height=" << upgradeHeight;
    rollbackBlockchainTo(upgradeHeight);
    reinitUpgradeDetectors = true;
  }

  if (reinitUpgradeDetectors && (!m_upgradeDetectorV2.init() || !m_upgradeDetectorV3.init() || !m_upgradeDetectorV4.init() || !m_upgradeDetectorV5.init())) {
    logger(ERROR, BRIGHT_RED) << "Failed to initialize upgrade detector";
    return false;
  }

  update_next_cumulative_size_limit();

  uint64_t timestamp_diff = time(NULL) - m_blocks.back().bl.timestamp;
  if (!m_blocks.back().bl.timestamp) {
    timestamp_diff = time(NULL) - 1341378000;
  }

  logger(INFO, BRIGHT_GREEN)
    << "Blockchain initialized. last block: " << m_blocks.size() - 1 << ", "
    << Common::timeIntervalToString(timestamp_diff)
    << " time ago, current difficulty: " << getDifficultyForNextBlock();
  return true;
}

void Blockchain::rebuildCache() {
  std::chrono::steady_clock::time_point timePoint = std::chrono::steady_clock::now();
  m_blockIndex.clear();
  m_transactionMap.clear();
  m_spent_key_images.clear();
  m_outputs.clear();
  m_multisignatureOutputs.clear();
  for (uint32_t b = 0; b < m_blocks.size(); ++b) {
    if (b % 1000 == 0) {
      logger(INFO, BRIGHT_WHITE) << "Height " << b << " of " << m_blocks.size();
    }
    const BlockEntry& block = m_blocks[b];
    Crypto::Hash blockHash = get_block_hash(block.bl);
    m_blockIndex.push(blockHash);
    for (uint16_t t = 0; t < block.transactions.size(); ++t) {
      const TransactionEntry& transaction = block.transactions[t];
      Crypto::Hash transactionHash = getObjectHash(transaction.tx);
      TransactionIndex transactionIndex = { b, t };
      m_transactionMap.insert(std::make_pair(transactionHash, transactionIndex));

      // process inputs
      for (auto& i : transaction.tx.inputs) {
        if (i.type() == typeid(KeyInput)) {
          m_spent_key_images.insert(std::make_pair(::boost::get<KeyInput>(i).keyImage, b));
        } else if (i.type() == typeid(MultisignatureInput)) {
          auto out = ::boost::get<MultisignatureInput>(i);
          m_multisignatureOutputs[out.amount][out.outputIndex].isUsed = true;
        }
      }

      // process outputs
      for (uint16_t o = 0; o < transaction.tx.outputs.size(); ++o) {
        const auto& out = transaction.tx.outputs[o];
        if (out.target.type() == typeid(KeyOutput)) {
          m_outputs[out.amount].push_back(std::make_pair<>(transactionIndex, o));
        } else if (out.target.type() == typeid(MultisignatureOutput)) {
          MultisignatureOutputUsage usage = { transactionIndex, o, false };
          m_multisignatureOutputs[out.amount].push_back(usage);
        }
      }
    }
  }

  std::chrono::duration<double> duration = std::chrono::steady_clock::now() - timePoint;
  logger(INFO, BRIGHT_WHITE) << "Rebuilding internal structures took: " << duration.count();
}

bool Blockchain::storeCache() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  logger(INFO, BRIGHT_WHITE) << "Saving blockchain at height " << m_blocks.size() - 1 << "...";
  BlockCacheSerializer ser(*this, getTailId(), logger.getLogger());
  if (!ser.save(appendPath(m_config_folder, m_currency.blocksCacheFileName()))) {
    logger(ERROR, BRIGHT_RED) << "Failed to save blockchain cache";
    return false;
  }

  return true;
}

bool Blockchain::deinit() {
  storeCache();
  if (m_blockchainIndexesEnabled) {
    storeBlockchainIndices();
  }
  assert(m_messageQueueList.empty());
  return true;
}

bool Blockchain::resetAndSetGenesisBlock(const Block& b) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  m_blocks.clear();
  m_blockIndex.clear();
  m_transactionMap.clear();

  m_spent_key_images.clear();
  m_alternative_chains.clear();
  m_outputs.clear();

  m_paymentIdIndex.clear();
  m_timestampIndex.clear();
  m_generatedTransactionsIndex.clear();
  m_orphanBlocksIndex.clear();

  block_verification_context bvc = boost::value_initialized<block_verification_context>();
  addNewBlock(b, bvc);
  return bvc.m_added_to_main_chain && !bvc.m_verification_failed;
}

Crypto::Hash Blockchain::getTailId(uint32_t& height) {
  assert(!m_blocks.empty());
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  height = getCurrentBlockchainHeight() - 1;
  return getTailId();
}

Crypto::Hash Blockchain::getTailId() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return m_blocks.empty() ? NULL_HASH : m_blockIndex.getTailId();
}

std::vector<Crypto::Hash> Blockchain::buildSparseChain() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  assert(m_blockIndex.size() != 0);
  return doBuildSparseChain(m_blockIndex.getTailId());
}

std::vector<Crypto::Hash> Blockchain::buildSparseChain(const Crypto::Hash& startBlockId) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  assert(haveBlock(startBlockId));
  return doBuildSparseChain(startBlockId);
}

std::vector<Crypto::Hash> Blockchain::doBuildSparseChain(const Crypto::Hash& startBlockId) const {
  assert(m_blockIndex.size() != 0);

  std::vector<Crypto::Hash> sparseChain;

  if (m_blockIndex.hasBlock(startBlockId)) {
    sparseChain = m_blockIndex.buildSparseChain(startBlockId);
  } else {
    assert(m_alternative_chains.count(startBlockId) > 0);

    std::vector<Crypto::Hash> alternativeChain;
    Crypto::Hash blockchainAncestor;
    for (auto it = m_alternative_chains.find(startBlockId); it != m_alternative_chains.end(); it = m_alternative_chains.find(blockchainAncestor)) {
      alternativeChain.emplace_back(it->first);
      blockchainAncestor = it->second.bl.previousBlockHash;
    }

    for (size_t i = 1; i <= alternativeChain.size(); i *= 2) {
      sparseChain.emplace_back(alternativeChain[i - 1]);
    }

    assert(!sparseChain.empty());
    assert(m_blockIndex.hasBlock(blockchainAncestor));
    std::vector<Crypto::Hash> sparseMainChain = m_blockIndex.buildSparseChain(blockchainAncestor);
    sparseChain.reserve(sparseChain.size() + sparseMainChain.size());
    std::copy(sparseMainChain.begin(), sparseMainChain.end(), std::back_inserter(sparseChain));
  }

  return sparseChain;
}

Crypto::Hash Blockchain::getBlockIdByHeight(uint32_t height) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  assert(height < m_blockIndex.size());
  return m_blockIndex.getBlockId(height);
}

bool Blockchain::getBlockByHash(const Crypto::Hash& blockHash, Block& b) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  uint32_t height = 0;

  if (m_blockIndex.getBlockHeight(blockHash, height)) {
    b = m_blocks[height].bl;
    return true;
  }

  logger(WARNING) << blockHash;

  auto blockByHashIterator = m_alternative_chains.find(blockHash);
  if (blockByHashIterator != m_alternative_chains.end()) {
    b = blockByHashIterator->second.bl;
    return true;
  }

  return false;
}

bool Blockchain::getBlockHeight(const Crypto::Hash& blockId, uint32_t& blockHeight) {
  std::lock_guard<decltype(m_blockchain_lock)> lock(m_blockchain_lock);
  return m_blockIndex.getBlockHeight(blockId, blockHeight);
}

bool Blockchain::getTransactionHeight(const Crypto::Hash &txId, uint32_t& blockHeight) {
  std::lock_guard<decltype(m_blockchain_lock)> bcLock(m_blockchain_lock);

  auto it = m_transactionMap.find(txId);
  if (it != m_transactionMap.end()) {
    blockHeight = it->second.block;
    return true;
  }

  return false;
}

difficulty_type Blockchain::getDifficultyForNextBlock() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  std::vector<uint64_t> timestamps;
  std::vector<difficulty_type> cumulative_difficulties;
  uint8_t BlockMajorVersion = getBlockMajorVersionForHeight(static_cast<uint32_t>(m_blocks.size()));
  size_t offset;
  offset = m_blocks.size() - std::min<size_t>(m_blocks.size(), static_cast<size_t>(m_currency.difficultyBlocksCountByBlockVersion(BlockMajorVersion)));

  if (offset == 0) {
    ++offset;
  }
  for (; offset < m_blocks.size(); offset++) {
    timestamps.push_back(m_blocks[offset].bl.timestamp);
    cumulative_difficulties.push_back(m_blocks[offset].cumulative_difficulty);
  }
  return m_currency.nextDifficulty(static_cast<uint32_t>(m_blocks.size()), BlockMajorVersion, timestamps, cumulative_difficulties);
}

difficulty_type Blockchain::getAvgDifficulty(uint32_t height, size_t window) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  height = std::min<uint32_t>(height, (uint32_t)m_blocks.size() - 1);
  if (height <= 1)
    return 1;

  if (window == height) {
    return m_blocks[height].cumulative_difficulty / height;
  }

  size_t offset;
  offset = height - std::min<uint32_t>(height, std::min<uint32_t>(static_cast<uint32_t>(m_blocks.size() - 1), static_cast<uint32_t>(window)));
  if (offset == 0) {
    ++offset;
  }
  difficulty_type cumulDiffForPeriod = m_blocks[height].cumulative_difficulty - m_blocks[offset].cumulative_difficulty;
  return cumulDiffForPeriod / std::min<uint32_t>(static_cast<uint32_t>(m_blocks.size() - 1), static_cast<uint32_t>(window));
}

difficulty_type Blockchain::getAvgDifficulty(uint32_t height) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (height <= 1)
    return 1;
  return m_blocks[std::min<difficulty_type>(height, m_blocks.size())].cumulative_difficulty / std::min<difficulty_type>(height, m_blocks.size());
}

uint64_t Blockchain::getBlockTimestamp(uint32_t height) {
  assert(height < m_blocks.size());
  return m_blocks[height].bl.timestamp;
}

uint64_t Blockchain::getMinimalFee(uint32_t height) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (height == 0 || m_blocks.size() <= 1) {
    return 0;
  }
  if (height > static_cast<uint32_t>(m_blocks.size()) - 1) {
    height = static_cast<uint32_t>(m_blocks.size()) - 1;
  }
  if (height < 3) {
    height = 3;
  }
  uint32_t window = std::min(height, std::min<uint32_t>(static_cast<uint32_t>(m_blocks.size()), static_cast<uint32_t>(m_currency.expectedNumberOfBlocksPerDay())));
  if (window == 0) {
    ++window;
  }
  size_t offset = height - window;
  if (offset == 0) {
    ++offset;
  }

  /* Perhaps, in case of POW change, difficulties for calculations here
   * should be reset and used starting from the fork height.
   */

  // calculate average difficulty for ~last month
  uint64_t avgCurrentDifficulty = getAvgDifficulty(height, window * 7 * 4);
  // reference trailing average difficulty
  uint64_t avgReferenceDifficulty = m_blocks[height].cumulative_difficulty / height;
  // calculate current base reward
  uint64_t currentReward = m_currency.calculateReward(m_blocks[height].already_generated_coins);
  // reference trailing average reward
  uint64_t avgReferenceReward = m_blocks[height].already_generated_coins / height;

  return m_currency.getMinimalFee(avgCurrentDifficulty, currentReward, avgReferenceDifficulty, avgReferenceReward, height);
}

uint64_t Blockchain::getCoinsInCirculation() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (m_blocks.empty()) {
    return 0;
  } else {
    return m_blocks.back().already_generated_coins;
  }
}

uint64_t Blockchain::getCoinsInCirculation(uint32_t height) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (m_blocks.empty()) {
    return 0;
  }
  else {
    return m_blocks[height].already_generated_coins;
  }
}

uint8_t Blockchain::getBlockMajorVersionForHeight(uint32_t height) const {
  if (height > m_upgradeDetectorV5.upgradeHeight()) {
    return m_upgradeDetectorV5.targetVersion();
  } else if (height > m_upgradeDetectorV4.upgradeHeight()) {
    return m_upgradeDetectorV4.targetVersion();
  } else if (height > m_upgradeDetectorV3.upgradeHeight()) {
    return m_upgradeDetectorV3.targetVersion();
  } else if (height > m_upgradeDetectorV2.upgradeHeight()) {
    return m_upgradeDetectorV2.targetVersion();
  } else {
    return BLOCK_MAJOR_VERSION_1;
  }
}

bool Blockchain::rollback_blockchain_switching(std::list<Block> &original_chain, size_t rollback_height) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  // remove failed subchain
  for (size_t i = m_blocks.size() - 1; i >= rollback_height; i--) {
    popBlock();
  }

  // return back original chain
  for (auto &bl : original_chain) {
    block_verification_context bvc =
      boost::value_initialized<block_verification_context>();
    bool r = pushBlock(bl, get_block_hash(bl), bvc);
    if (!(r && bvc.m_added_to_main_chain)) {
      logger(ERROR, BRIGHT_RED) << "PANIC!!! failed to add (again) block while "
        "chain switching during the rollback!";
      return false;
    }
  }

  logger(INFO, BRIGHT_WHITE) << "Rollback success.";
  return true;
}

bool Blockchain::switch_to_alternative_blockchain(std::list<blocks_ext_by_hash::iterator>& alt_chain, bool discard_disconnected_chain) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  if (!(alt_chain.size())) {
    logger(ERROR, BRIGHT_RED) << "switch_to_alternative_blockchain: empty chain passed";
    return false;
  }

  size_t split_height = alt_chain.front()->second.height;

  if (!(m_blocks.size() > split_height)) {
    logger(ERROR, BRIGHT_RED) << "switch_to_alternative_blockchain: blockchain size is lower than split height";
    return false;
  }

  // Poisson check, courtesy of ryo-project
  // https://github.com/ryo-currency/ryo-writeups/blob/master/poisson-writeup.md
  // For longer reorgs, check if the timestamps are probable - if they aren't the diff algo has failed
  // This check is meant to detect an offline bypass of timestamp < time() + ftl check
  // It doesn't need to be very strict as it synergises with the median check
  if (alt_chain.size() >= CryptoNote::parameters::POISSON_CHECK_TRIGGER)
  {
    uint64_t alt_chain_size = alt_chain.size();
    uint64_t high_timestamp = alt_chain.back()->second.bl.timestamp;
    Crypto::Hash low_block = alt_chain.front()->second.bl.previousBlockHash;

    //Make sure that the high_timestamp is really highest
    for (const blocks_ext_by_hash::iterator &it : alt_chain)
    {
      if (high_timestamp < it->second.bl.timestamp)
        high_timestamp = it->second.bl.timestamp;
    }

    uint64_t block_ftl = CryptoNote::parameters::CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V1;
    // This would fail later anyway
    if (high_timestamp > get_adjusted_time() + block_ftl)
    {
      logger(ERROR, BRIGHT_RED) << "Attempting to move to an alternate chain, but it failed FTL check! Timestamp: " << high_timestamp << ", limit: " << get_adjusted_time() + block_ftl;
      return false;
    }

    logger(WARNING) << "Poisson check triggered by reorg size of " << alt_chain_size;

    uint64_t failed_checks = 0, i = 1;
    for (; i <= CryptoNote::parameters::POISSON_CHECK_DEPTH; i++)
    {
      // This means we reached the genesis block
      if (low_block == NULL_HASH)
        break;

      Block blk;
      getBlockByHash(low_block, blk);

      uint64_t low_timestamp = blk.timestamp;
      low_block = blk.previousBlockHash;

      if (low_timestamp >= high_timestamp)
      {
        logger(INFO) << "Skipping check at depth " << i << " due to tampered timestamp on main chain.";
        failed_checks++;
        continue;
      }

      double lam = double(high_timestamp - low_timestamp) / double(CryptoNote::parameters::DIFFICULTY_TARGET);
      if (calc_poisson_ln(lam, alt_chain_size + i) < CryptoNote::parameters::POISSON_LOG_P_REJECT)
      {
        logger(INFO) << "Poisson check at depth " << i << " failed! delta_t: " << (high_timestamp - low_timestamp) << " size: " << alt_chain_size + i;
        failed_checks++;
      }
    }

    i--; //Convert to number of checks
    logger(INFO) << "Poisson check result " << failed_checks << " fails out of " << i;

    if (failed_checks > i / 2)
    {
      logger(ERROR, BRIGHT_RED) << "Attempting to move to an alternate chain, but it failed Poisson check! " << failed_checks << " fails out of " << i << " alt_chain_size: " << alt_chain_size;
      return false;
    }
  }

  // Compare transactions in proposed alt chain vs current main chain and reject if some transaction is missing in the alt chain
  std::vector<Crypto::Hash> mainChainTxHashes, altChainTxHashes;
  for (size_t i = m_blocks.size() - 1; i >= split_height; i--) {
    Block b = m_blocks[i].bl;
    std::copy(b.transactionHashes.begin(), b.transactionHashes.end(), std::inserter(mainChainTxHashes, mainChainTxHashes.end()));
  }
  for (auto alt_ch_iter = alt_chain.begin(); alt_ch_iter != alt_chain.end(); alt_ch_iter++) {
    auto ch_ent = *alt_ch_iter;
    Block b = ch_ent->second.bl;
    std::copy(b.transactionHashes.begin(), b.transactionHashes.end(), std::inserter(altChainTxHashes, altChainTxHashes.end()));
  }
  for (auto main_ch_it = mainChainTxHashes.begin(); main_ch_it != mainChainTxHashes.end(); main_ch_it++) {
    auto tx_hash = *main_ch_it;
    if (std::find(altChainTxHashes.begin(), altChainTxHashes.end(), tx_hash) == altChainTxHashes.end()) {
      logger(ERROR, BRIGHT_RED) << "Attempting to switch to an alternate chain, but it lacks transaction " << Common::podToHex(tx_hash) << " from main chain, rejected";
      mainChainTxHashes.clear();
      mainChainTxHashes.shrink_to_fit();
      altChainTxHashes.clear();
      altChainTxHashes.shrink_to_fit();
      return false;
    }
  }

  //disconnecting old chain
  std::list<Block> disconnected_chain;
  for (size_t i = m_blocks.size() - 1; i >= split_height; i--) {
    Block b = m_blocks[i].bl;
    popBlock();
    //if (!(r)) { logger(ERROR, BRIGHT_RED) << "failed to remove block on chain switching"; return false; }
    disconnected_chain.push_front(b);
  }

  //connecting new alternative chain
  for (auto alt_ch_iter = alt_chain.begin(); alt_ch_iter != alt_chain.end(); alt_ch_iter++) {
    auto ch_ent = *alt_ch_iter;
    block_verification_context bvc = boost::value_initialized<block_verification_context>();
    bool r = pushBlock(ch_ent->second.bl, get_block_hash(ch_ent->second.bl), bvc);
    if (!r || !bvc.m_added_to_main_chain) {
      logger(INFO, BRIGHT_WHITE) << "Failed to switch to alternative blockchain";
      rollback_blockchain_switching(disconnected_chain, split_height);
      //add_block_as_invalid(ch_ent->second, get_block_hash(ch_ent->second.bl));
      logger(INFO, BRIGHT_WHITE) << "The block was inserted as invalid while connecting new alternative chain,  block_id: " << get_block_hash(ch_ent->second.bl);
      m_orphanBlocksIndex.remove(ch_ent->second.bl);
      m_alternative_chains.erase(ch_ent);

      for (auto alt_ch_to_orph_iter = ++alt_ch_iter; alt_ch_to_orph_iter != alt_chain.end(); alt_ch_to_orph_iter++) {
        //block_verification_context bvc = boost::value_initialized<block_verification_context>();
        //add_block_as_invalid((*alt_ch_iter)->second, (*alt_ch_iter)->first);
        m_orphanBlocksIndex.remove((*alt_ch_to_orph_iter)->second.bl);
        m_alternative_chains.erase(*alt_ch_to_orph_iter);
      }

      return false;
    }
  }

  if (!discard_disconnected_chain) {
    //pushing old chain as alternative chain
    for (auto& old_ch_ent : disconnected_chain) {
      block_verification_context bvc = boost::value_initialized<block_verification_context>();
      bool r = handle_alternative_block(old_ch_ent, get_block_hash(old_ch_ent), bvc, false);
      if (!r) {
        logger(WARNING, BRIGHT_YELLOW) << ("Failed to push ex-main chain blocks to alternative chain ");
        break;
      }
    }
  }

  std::vector<Crypto::Hash> blocksFromCommonRoot;
  blocksFromCommonRoot.reserve(alt_chain.size() + 1);
  blocksFromCommonRoot.push_back(alt_chain.front()->second.bl.previousBlockHash);

  //removing all_chain entries from alternative chain
  for (auto ch_ent : alt_chain) {
    blocksFromCommonRoot.push_back(get_block_hash(ch_ent->second.bl));
    m_orphanBlocksIndex.remove(ch_ent->second.bl);
    m_alternative_chains.erase(ch_ent);
  }

  sendMessage(BlockchainMessage(ChainSwitchMessage(std::move(blocksFromCommonRoot))));

  logger(INFO, BRIGHT_GREEN) << "REORGANIZE SUCCESS! on height: " << split_height << ", new blockchain size: " << m_blocks.size();
  return true;
}

//------------------------------------------------------------------
// This function calculates the difficulty target for the block being added to an alternate chain.
difficulty_type Blockchain::get_next_difficulty_for_alternative_chain(const std::list<blocks_ext_by_hash::iterator>& alt_chain, BlockEntry& bei) {
  std::vector<uint64_t> timestamps;
  std::vector<difficulty_type> cumulative_difficulties;
  uint8_t BlockMajorVersion = getBlockMajorVersionForHeight(static_cast<uint32_t>(m_blocks.size()));

  // if the alt chain isn't long enough to calculate the difficulty target
  // based on its blocks alone, need to get more blocks from the main chain
  if (alt_chain.size() < m_currency.difficultyBlocksCountByBlockVersion(BlockMajorVersion)) {
    std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
    size_t main_chain_stop_offset = alt_chain.size() ? alt_chain.front()->second.height : bei.height;
    size_t main_chain_count = m_currency.difficultyBlocksCountByBlockVersion(BlockMajorVersion) - std::min(m_currency.difficultyBlocksCountByBlockVersion(BlockMajorVersion), alt_chain.size());
    main_chain_count = std::min(main_chain_count, main_chain_stop_offset);
    size_t main_chain_start_offset = main_chain_stop_offset - main_chain_count;

    if (!main_chain_start_offset)
      ++main_chain_start_offset; //skip genesis block
    
    // get difficulties and timestamps from relevant main chain blocks
    for (; main_chain_start_offset < main_chain_stop_offset; ++main_chain_start_offset) {
      timestamps.push_back(m_blocks[main_chain_start_offset].bl.timestamp);
      cumulative_difficulties.push_back(m_blocks[main_chain_start_offset].cumulative_difficulty);
    }

    // make sure we haven't accidentally grabbed too many blocks... ???
    if (!((alt_chain.size() + timestamps.size()) <= m_currency.difficultyBlocksCountByBlockVersion(BlockMajorVersion))) {
      logger(ERROR, BRIGHT_RED) << "Internal error, alt_chain.size()[" << alt_chain.size() << "] + timestamps.size()[" << timestamps.size() <<
        "] NOT <= m_currency.difficultyBlocksCount()[" << m_currency.difficultyBlocksCountByBlockVersion(BlockMajorVersion) << ']'; return false;
    }
    for (auto it : alt_chain) {
      timestamps.push_back(it->second.bl.timestamp);
      cumulative_difficulties.push_back(it->second.cumulative_difficulty);
    }
  // if the alt chain is long enough for the difficulty calc, grab difficulties
  // and timestamps from it alone
  } else {
    timestamps.resize(std::min(alt_chain.size(), m_currency.difficultyBlocksCountByBlockVersion(BlockMajorVersion)));
    cumulative_difficulties.resize(std::min(alt_chain.size(), m_currency.difficultyBlocksCountByBlockVersion(BlockMajorVersion)));
    size_t count = 0;
    size_t max_i = timestamps.size() - 1;
    // get difficulties and timestamps from most recent blocks in alt chain
    BOOST_REVERSE_FOREACH(auto it, alt_chain) {
      timestamps[max_i - count] = it->second.bl.timestamp;
      cumulative_difficulties[max_i - count] = it->second.cumulative_difficulty;
      count++;
      if (count >= m_currency.difficultyBlocksCountByBlockVersion(BlockMajorVersion)) {
        break;
      }
    }
  }

  return m_currency.nextDifficulty(static_cast<uint32_t>(m_blocks.size()), BlockMajorVersion, timestamps, cumulative_difficulties);
}

bool Blockchain::prevalidate_miner_transaction(const Block& b, uint32_t height) {

  if (!(b.baseTransaction.inputs.size() == 1)) {
    logger(ERROR, BRIGHT_RED)
      << "Coinbase transaction in the block has no inputs";
    return false;
  }

  if (!(b.baseTransaction.signatures.empty())) {
    logger(ERROR, BRIGHT_RED)
      << "Coinbase transaction in the block shouldn't have signatures";
    return false;
  }

  if (!(b.baseTransaction.inputs[0].type() == typeid(BaseInput))) {
    logger(ERROR, BRIGHT_RED)
      << "Coinbase transaction in the block has the wrong type";
    return false;
  }

  if (boost::get<BaseInput>(b.baseTransaction.inputs[0]).blockIndex != height) {
    logger(INFO, BRIGHT_RED) << "The miner transaction in block has invalid height: " <<
      boost::get<BaseInput>(b.baseTransaction.inputs[0]).blockIndex << ", expected: " << height;
    return false;
  }

  if (!(b.baseTransaction.unlockTime == height + m_currency.minedMoneyUnlockWindow())) {
    logger(ERROR, BRIGHT_RED)
      << "Coinbase transaction has wrong unlock time="
      << b.baseTransaction.unlockTime << ", expected "
      << (height + m_currency.minedMoneyUnlockWindow());
    return false;
  }

  if (!check_outs_overflow(b.baseTransaction)) {
    logger(ERROR, BRIGHT_RED) << "The miner transaction has money overflow in block " << get_block_hash(b);
    return false;
  }

  uint64_t extraSize = (uint64_t)b.baseTransaction.extra.size();
  if (height > CryptoNote::parameters::UPGRADE_HEIGHT_V4_2 && extraSize > CryptoNote::parameters::MAX_EXTRA_SIZE) {
    logger(ERROR, BRIGHT_RED)
      << "The miner transaction extra is too large in block "
      << get_block_hash(b) << ". Allowed: "
      << CryptoNote::parameters::MAX_EXTRA_SIZE
      << ", actual: " << extraSize << ".";
    return false;
  }

  return true;
}

bool Blockchain::validate_miner_transaction(const Block& b, uint32_t height, size_t cumulativeBlockSize,
  uint64_t alreadyGeneratedCoins, uint64_t fee, uint64_t& reward, int64_t& emissionChange) {

  uint64_t minerReward = 0;
  for (auto& o : b.baseTransaction.outputs) {
    minerReward += o.amount;
  }

  std::vector<size_t> lastBlocksSizes;
  get_last_n_blocks_sizes(lastBlocksSizes, m_currency.rewardBlocksWindow());
  size_t blocksSizeMedian = Common::medianValue(lastBlocksSizes);

  auto blockMajorVersion = getBlockMajorVersionForHeight(height);
  if (!m_currency.getBlockReward(blockMajorVersion, blocksSizeMedian, cumulativeBlockSize, alreadyGeneratedCoins, fee, reward, emissionChange)) {
    logger(INFO, BRIGHT_WHITE) << "block size " << cumulativeBlockSize << " is bigger than allowed for this blockchain";
    return false;
  }

  if (minerReward > reward) {
    logger(ERROR, BRIGHT_RED) << "Coinbase transaction spend too much money: " << m_currency.formatAmount(minerReward) <<
      ", block reward is " << m_currency.formatAmount(reward);
    return false;
  } else if (minerReward < reward) {
    logger(ERROR, BRIGHT_RED) << "Coinbase transaction doesn't use full amount of block reward: spent " <<
      m_currency.formatAmount(minerReward) << ", block reward is " << m_currency.formatAmount(reward);
    return false;
  }

  return true;
}

bool Blockchain::getBackwardBlocksSize(size_t from_height, std::vector<size_t>& sz, size_t count) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (!(from_height < m_blocks.size())) {
    logger(ERROR, BRIGHT_RED)
      << "Internal error: get_backward_blocks_sizes called with from_height="
      << from_height << ", blockchain height = " << m_blocks.size();
    return false;
  }
  size_t start_offset = (from_height + 1) - std::min((from_height + 1), count);
  for (size_t i = start_offset; i != from_height + 1; i++) {
    sz.push_back(m_blocks[i].block_cumulative_size);
  }

  return true;
}

bool Blockchain::get_last_n_blocks_sizes(std::vector<size_t>& sz, size_t count) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (!m_blocks.size()) {
    return true;
  }

  return getBackwardBlocksSize(m_blocks.size() - 1, sz, count);
}

uint64_t Blockchain::getCurrentCumulativeBlocksizeLimit() {
  return m_current_block_cumul_sz_limit;
}

bool Blockchain::complete_timestamps_vector(uint8_t blockMajorVersion, uint64_t start_top_height, std::vector<uint64_t>& timestamps) {
  if (timestamps.size() >= m_currency.timestampCheckWindow(blockMajorVersion))
    return true;

  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  size_t need_elements = m_currency.timestampCheckWindow(blockMajorVersion) - timestamps.size();
  if (!(start_top_height < m_blocks.size())) { logger(ERROR, BRIGHT_RED) << "internal error: passed start_height = " << start_top_height << " not less then m_blocks.size()=" << m_blocks.size(); return false; }
  size_t stop_offset = start_top_height > need_elements ? start_top_height - need_elements : 0;
  do {
    timestamps.push_back(m_blocks[start_top_height].bl.timestamp);
    if (start_top_height == 0)
      break;
    --start_top_height;
  } while (start_top_height != stop_offset);
  return true;
}

bool Blockchain::handle_alternative_block(const Block& b, const Crypto::Hash& id, block_verification_context& bvc, bool sendNewAlternativeBlockMessage) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  auto block_height = get_block_height(b);
  if (block_height == 0) {
    logger(ERROR, BRIGHT_RED) <<
      "Block with id: " << Common::podToHex(id) << " (as alternative) have wrong miner transaction";
    bvc.m_verification_failed = true;
    return false;
  }

  // get fresh checkpoints from DNS - the best we have right now
#ifndef __ANDROID__
  m_checkpoints.load_checkpoints_from_dns();
#endif

  if (!m_checkpoints.is_alternative_block_allowed(getCurrentBlockchainHeight(), block_height)) {
    logger(TRACE) << "Block with id: " << id << std::endl <<
      " can't be accepted for alternative chain, block height: " << block_height << std::endl <<
      " blockchain height: " << getCurrentBlockchainHeight();
    bvc.m_verification_failed = true;
    return false;
  }

  if (!checkBlockVersion(b, id)) {
    bvc.m_verification_failed = true;
    return false;
  }

  //if (!checkParentBlockSize(b, id)) {
  //  bvc.m_verification_failed = true;
  //  return false;
  //}

  size_t cumulativeSize;
  if (!getBlockCumulativeSize(b, cumulativeSize)) {
    logger(TRACE) << "Block with id: " << id << " has at least one unknown transaction. Cumulative size is calculated imprecisely";
  }

  if (!checkCumulativeBlockSize(id, cumulativeSize, block_height)) {
    bvc.m_verification_failed = true;
    return false;
  }

  //block is not related with head of main chain
  //first of all - look in alternative chains container
  uint32_t mainPrevHeight = 0;
  const bool mainPrev = m_blockIndex.getBlockHeight(b.previousBlockHash, mainPrevHeight);
  const auto it_prev = m_alternative_chains.find(b.previousBlockHash);

  if (it_prev != m_alternative_chains.end() || mainPrev) {
    //we have new block in alternative chain

    //build alternative subchain, front -> mainchain, back -> alternative head
    blocks_ext_by_hash::iterator alt_it = it_prev; //m_alternative_chains.find()
    std::list<blocks_ext_by_hash::iterator> alt_chain;
    std::vector<uint64_t> timestamps;
    while (alt_it != m_alternative_chains.end()) {
      alt_chain.push_front(alt_it);
      timestamps.push_back(alt_it->second.bl.timestamp);
      alt_it = m_alternative_chains.find(alt_it->second.bl.previousBlockHash);
    }

    // if block to be added connects to known blocks that aren't part of the
    // main chain -- that is, if we're adding on to an alternate chain
    if (alt_chain.size()) {
      // make sure alt chain doesn't somehow start past the end of the main chain
      if (!(m_blocks.size() > alt_chain.front()->second.height)) { logger(ERROR, BRIGHT_RED) << "main blockchain wrong height"; return false; }
      // make sure block connects correctly to the main chain
	  Crypto::Hash h = NULL_HASH;
      get_block_hash(m_blocks[alt_chain.front()->second.height - 1].bl, h);
      if (!(h == alt_chain.front()->second.bl.previousBlockHash)) { logger(ERROR, BRIGHT_RED) << "alternative chain have wrong connection to main chain"; return false; }
      complete_timestamps_vector(b.majorVersion, alt_chain.front()->second.height - 1, timestamps);
    } else {
      // if block parent is not part of main chain or an alternate chain, we ignore it
      if (!(mainPrev)) { logger(ERROR, BRIGHT_RED) << "internal error: broken imperative condition it_main_prev != m_blocks_index.end()"; return false; }
	  complete_timestamps_vector(b.majorVersion, mainPrevHeight, timestamps);
    }

    // check timestamp correct - verify that the block's timestamp is within the acceptable range
    // (not earlier than the median of the last X blocks)
    if (!check_block_timestamp(timestamps, b)) {
      logger(INFO, BRIGHT_RED) <<
        "Block with id: " << id
        << ENDL << " for alternative chain, have invalid timestamp: " << b.timestamp;
      //add_block_as_invalid(b, id);//do not add blocks to invalid storage before proof of work check was passed
      bvc.m_verification_failed = true;
      return false;
    }

    BlockEntry bei = boost::value_initialized<BlockEntry>();
    bei.bl = b;
    bei.height = static_cast<uint32_t>(alt_chain.size() ? it_prev->second.height + 1 : mainPrevHeight + 1);

    bool is_a_checkpoint;
    if (!m_checkpoints.check_block(bei.height, id, is_a_checkpoint)) {
      logger(ERROR, BRIGHT_RED) <<
        "CHECKPOINT VALIDATION FAILED";
      bvc.m_verification_failed = true;
      return false;
    }

    // Disable merged mining
    if (bei.bl.majorVersion >= CryptoNote::BLOCK_MAJOR_VERSION_5) {
      TransactionExtraMergeMiningTag mmTag;
      if (getMergeMiningTagFromExtra(bei.bl.baseTransaction.extra, mmTag)) {
        logger(ERROR, BRIGHT_RED) << "Merge mining tag was found in extra of miner transaction";
        return false;
      }
    }

    // Check the block's hash against the difficulty target for its alt chain
    difficulty_type current_diff = get_next_difficulty_for_alternative_chain(alt_chain, bei);
    if (!(current_diff)) { logger(ERROR, BRIGHT_RED) << "!!!!!!! DIFFICULTY OVERHEAD !!!!!!!"; return false; }
    Crypto::Hash proof_of_work = NULL_HASH;
    // Always check PoW for alternative blocks
    if (!m_currency.checkProofOfWork(m_cn_context, bei.bl, current_diff, proof_of_work)) {
      logger(INFO, BRIGHT_RED) <<
        "Block with id: " << id
        << ENDL << " for alternative chain, have not enough proof of work: " << proof_of_work
        << ENDL << " expected difficulty: " << current_diff;
      bvc.m_verification_failed = true;
      return false;
    }

    if (!prevalidate_miner_transaction(b, bei.height)) {
      logger(INFO, BRIGHT_RED) <<
        "Block with id: " << Common::podToHex(id) << " (as alternative) have wrong miner transaction.";
      bvc.m_verification_failed = true;
      return false;
    }

    bei.cumulative_difficulty = alt_chain.size() ? it_prev->second.cumulative_difficulty : m_blocks[mainPrevHeight].cumulative_difficulty;
    bei.cumulative_difficulty += current_diff;

#ifdef _DEBUG
    auto i_dres = m_alternative_chains.find(id);
    if (!(i_dres == m_alternative_chains.end())) { logger(ERROR, BRIGHT_RED) << "insertion of new alternative block returned as it already exist"; return false; }
#endif

    auto i_res = m_alternative_chains.insert(blocks_ext_by_hash::value_type(id, bei));
    if (!(i_res.second)) { logger(ERROR, BRIGHT_RED) << "insertion of new alternative block returned as it already exist"; return false; }

    m_orphanBlocksIndex.add(bei.bl);

    alt_chain.push_back(i_res.first);

    if (is_a_checkpoint) {
      //do reorganize!
      logger(INFO, BRIGHT_GREEN) <<
        "###### REORGANIZE on height: " << alt_chain.front()->second.height << " of " << m_blocks.size() - 1 <<
        ", checkpoint is found in alternative chain on height " << bei.height;
      bool r = switch_to_alternative_blockchain(alt_chain, true);
      if (r) {
        bvc.m_added_to_main_chain = true;
        bvc.m_switched_to_alt_chain = true;
      } else {
        bvc.m_verification_failed = true;
      }
      return r;
    } else if (m_blocks.back().cumulative_difficulty < bei.cumulative_difficulty) //check if difficulty bigger then in main chain
    {
      //do reorganize!
      logger(INFO, BRIGHT_GREEN) <<
        "###### REORGANIZE on height: " << alt_chain.front()->second.height << " of " << m_blocks.size() - 1 << " with cum_difficulty " << m_blocks.back().cumulative_difficulty
        << ENDL << " alternative blockchain size: " << alt_chain.size() << " with cum_difficulty " << bei.cumulative_difficulty;
      bool r = switch_to_alternative_blockchain(alt_chain, false);
      if (r) {
        bvc.m_added_to_main_chain = true;
        bvc.m_switched_to_alt_chain = true;
      } else {
        bvc.m_verification_failed = true;
      }
      return r;
    } else {
      logger(INFO, BRIGHT_BLUE) <<
        "----- BLOCK ADDED AS ALTERNATIVE ON HEIGHT " << bei.height
        << ENDL << "id:\t" << id
        << ENDL << "PoW:\t" << proof_of_work
        << ENDL << "difficulty:\t" << current_diff;
      if (sendNewAlternativeBlockMessage) {
        sendMessage(BlockchainMessage(NewAlternativeBlockMessage(id)));
      }
      return true;
    }
  } else {
    //block orphaned
    bvc.m_marked_as_orphaned = true;
    logger(INFO, BRIGHT_RED) <<
      "Block recognized as orphaned and rejected, id = " << id;
  }

  return true;
}

bool Blockchain::getBlocks(uint32_t start_offset, uint32_t count, std::list<Block>& blocks, std::list<Transaction>& txs) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (start_offset >= m_blocks.size())
    return false;
  for (size_t i = start_offset; i < start_offset + count && i < m_blocks.size(); i++) {
    blocks.push_back(m_blocks[i].bl);
    std::list<Crypto::Hash> missed_ids;
    getTransactions(m_blocks[i].bl.transactionHashes, txs, missed_ids);
    if (!(!missed_ids.size())) { logger(ERROR, BRIGHT_RED) << "have missed transactions in own block in main blockchain"; return false; }
  }

  return true;
}

bool Blockchain::getBlocks(uint32_t start_offset, uint32_t count, std::list<Block>& blocks) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (start_offset >= m_blocks.size()) {
    return false;
  }

  for (uint32_t i = start_offset; i < start_offset + count && i < m_blocks.size(); i++) {
    blocks.push_back(m_blocks[i].bl);
  }

  return true;
}

bool Blockchain::getTransactionsWithOutputGlobalIndexes(const std::vector<Crypto::Hash>& txs_ids, std::list<Crypto::Hash>& missed_txs, std::vector<std::pair<Transaction, std::vector<uint32_t>>>& txs) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  for (const auto& tx_id : txs_ids) {
    auto it = m_transactionMap.find(tx_id);
    if (it == m_transactionMap.end()) {
      missed_txs.push_back(tx_id);
    }
    else {
      const TransactionEntry& tx = transactionByIndex(it->second);
      if (!(tx.m_global_output_indexes.size())) { logger(ERROR, BRIGHT_RED) << "internal error: global indexes for transaction " << tx_id << " is empty"; return false; }
      txs.push_back(std::make_pair(tx.tx, tx.m_global_output_indexes));
    }
  }

  return true;
}

bool Blockchain::handleGetObjects(NOTIFY_REQUEST_GET_OBJECTS::request& arg, NOTIFY_RESPONSE_GET_OBJECTS::request& rsp) { //Deprecated. Should be removed with CryptoNoteProtocolHandler.
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  rsp.current_blockchain_height = getCurrentBlockchainHeight();
  std::list<Block> blocks;
  getBlocks(arg.blocks, blocks, rsp.missed_ids);
  for (const auto& bl : blocks) {
    std::list<Crypto::Hash> missed_tx_id;
    std::list<Transaction> txs;
    getTransactions(bl.transactionHashes, txs, missed_tx_id);
    if (!(!missed_tx_id.size())) {
      logger(ERROR, BRIGHT_RED) << "Internal error: have missed missed_tx_id.size()=" << missed_tx_id.size() << ENDL << "for block id = " << get_block_hash(bl);
      rsp.missed_ids.insert(rsp.missed_ids.end(), missed_tx_id.begin(), missed_tx_id.end());
      return false;
    } //WTF???
    rsp.blocks.push_back(block_complete_entry());
    block_complete_entry& e = rsp.blocks.back();
    //pack block
    e.block = asString(toBinaryArray(bl));
    //pack transactions
    for (Transaction& tx : txs) {
      e.txs.push_back(asString(toBinaryArray(tx)));
    }
  }

  //get another transactions, if need
  std::list<Transaction> txs;
  getTransactions(arg.txs, txs, rsp.missed_ids);
  //pack aside transactions
  for (const auto& tx : txs) {
    rsp.txs.push_back(asString(toBinaryArray(tx)));
  }

  return true;
}

bool Blockchain::getAlternativeBlocks(std::list<Block>& blocks) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  for (auto& alt_bl : m_alternative_chains) {
    blocks.push_back(alt_bl.second.bl);
  }

  return true;
}

uint32_t Blockchain::getAlternativeBlocksCount() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return static_cast<uint32_t>(m_alternative_chains.size());
}

bool Blockchain::add_out_to_get_random_outs(std::vector<std::pair<TransactionIndex, uint16_t>>& amount_outs, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount& result_outs, uint64_t amount, size_t i) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  const Transaction& tx = transactionByIndex(amount_outs[i].first).tx;
  if (!(tx.outputs.size() > amount_outs[i].second)) {
    logger(ERROR, BRIGHT_RED) << "internal error: in global outs index, transaction out index="
      << amount_outs[i].second << " more than transaction outputs = " << tx.outputs.size() << ", for tx id = " << getObjectHash(tx); return false;
  }
  if (!(tx.outputs[amount_outs[i].second].target.type() == typeid(KeyOutput))) { logger(ERROR, BRIGHT_RED) << "unknown tx out type"; return false; }

  //check if transaction is unlocked
  if (!is_tx_spendtime_unlocked(tx.unlockTime))
    return false;

  COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry& oen = *result_outs.outs.insert(result_outs.outs.end(), COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry());
  oen.global_amount_index = static_cast<uint32_t>(i);
  oen.out_key = boost::get<KeyOutput>(tx.outputs[amount_outs[i].second].target).key;
  return true;
}

size_t Blockchain::find_end_of_allowed_index(const std::vector<std::pair<TransactionIndex, uint16_t>>& amount_outs) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (amount_outs.empty()) {
    return 0;
  }

  size_t i = amount_outs.size();
  do {
    --i;
    if (amount_outs[i].first.block + (amount_outs[i].first.block < m_currency.minedMoneyUnlockWindow()) <= getCurrentBlockchainHeight()) {
      return i + 1;
    }
  } while (i != 0);

  return 0;
}

bool Blockchain::getRandomOutsByAmount(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request& req, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response& res) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  for (uint64_t amount : req.amounts) {
    COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount& result_outs = *res.outs.insert(res.outs.end(), COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount());
    result_outs.amount = amount;
    auto it = m_outputs.find(amount);
    if (it == m_outputs.end()) {
      logger(ERROR, BRIGHT_RED) <<
        "COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS: not outs for amount " << amount << ", wallet should use some real outs when it lookup for some mix, so, at least one out for this amount should exist";
      continue;//actually this is strange situation, wallet should use some real outs when it lookup for some mix, so, at least one out for this amount should exist
    }

    std::vector<std::pair<TransactionIndex, uint16_t>>& amount_outs = it->second;
    //it is not good idea to use top fresh outs, because it increases possibility of transaction canceling on split
    //lets find upper bound of not fresh outs
    size_t up_index_limit = find_end_of_allowed_index(amount_outs);
    if (!(up_index_limit <= amount_outs.size())) { logger(ERROR, BRIGHT_RED) << "internal error: find_end_of_allowed_index returned wrong index=" << up_index_limit << ", with amount_outs.size = " << amount_outs.size(); return false; }

	if(amount_outs.size() > req.outs_count)
    {
      std::set<size_t> used;
      size_t try_count = 0;
      for(uint64_t j = 0; j != req.outs_count && try_count < up_index_limit;)
      {
	    // triangular distribution over [a,b) with a=0, mode c=b=up_index_limit
        uint64_t r = Random::randomValue<size_t>() % ((uint64_t)1 << 53);
        double frac = std::sqrt((double)r / ((uint64_t)1 << 53));
        size_t i = (size_t)(frac*up_index_limit);
        if(used.count(i))
          continue;
        bool added = add_out_to_get_random_outs(amount_outs, result_outs, amount, i);
        used.insert(i);
        if(added)
          ++j;
        ++try_count;
      }
    }else
    {
      for(size_t i = 0; i != up_index_limit; i++)
        add_out_to_get_random_outs(amount_outs, result_outs, amount, i);
    }
  }
  return true;
}

uint32_t Blockchain::findBlockchainSupplement(const std::vector<Crypto::Hash>& qblock_ids) {
  assert(!qblock_ids.empty());
  assert(qblock_ids.back() == m_blockIndex.getBlockId(0));

  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t blockIndex;
  // assert above guarantees that method returns true
  m_blockIndex.findSupplement(qblock_ids, blockIndex);
  return blockIndex;
}

uint64_t Blockchain::blockDifficulty(size_t i) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (!(i < m_blocks.size())) { logger(ERROR, BRIGHT_RED) << "wrong block index i = " << i << " at Blockchain::block_difficulty()"; return false; }
  if (i == 0)
    return m_blocks[i].cumulative_difficulty;

  return m_blocks[i].cumulative_difficulty - m_blocks[i - 1].cumulative_difficulty;
}

uint64_t Blockchain::blockCumulativeDifficulty(size_t i) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (!(i < m_blocks.size())) { logger(ERROR, BRIGHT_RED) << "wrong block index i = " << i << " at Blockchain::block_difficulty()"; return false; }

  return m_blocks[i].cumulative_difficulty;
}

bool Blockchain::getblockEntry(size_t i, uint64_t& block_cumulative_size, difficulty_type& difficulty, uint64_t& already_generated_coins, uint64_t& reward, uint64_t& transactions_count, uint64_t& timestamp) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (!(i < m_blocks.size())) { logger(ERROR, BRIGHT_RED) << "wrong block index i = " << i << " at Blockchain::get_block_entry()"; return false; }

  block_cumulative_size = m_blocks[i].block_cumulative_size;
  difficulty = m_blocks[i].cumulative_difficulty - m_blocks[i - 1].cumulative_difficulty;
  already_generated_coins = m_blocks[i].already_generated_coins;
  reward = m_blocks[i].already_generated_coins - m_blocks[i - 1].already_generated_coins;
  timestamp = m_blocks[i].bl.timestamp;
  transactions_count = m_blocks[i].bl.transactionHashes.size();

  return true;
}

void Blockchain::print_blockchain(uint64_t start_index, uint64_t end_index) {
  std::stringstream ss;
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (start_index >= m_blocks.size()) {
    logger(INFO, BRIGHT_WHITE) <<
      "Wrong starter index set: " << start_index << ", expected max index " << m_blocks.size() - 1;
    return;
  }

  for (size_t i = start_index; i != m_blocks.size() && i != end_index; i++) {
    ss << "height " << i << ", timestamp " << m_blocks[i].bl.timestamp << ", cumul_dif " << m_blocks[i].cumulative_difficulty << ", cumul_size " << m_blocks[i].block_cumulative_size
      << "\nid\t\t" << get_block_hash(m_blocks[i].bl)
      << "\ndifficulty\t\t" << blockDifficulty(i) << ", nonce " << m_blocks[i].bl.nonce << ", tx_count " << m_blocks[i].bl.transactionHashes.size() << ENDL;
  }
  logger(DEBUGGING) <<
    "Current blockchain:" << ENDL << ss.str();
  logger(INFO, BRIGHT_WHITE) <<
    "Blockchain printed with log level 1";
}

void Blockchain::print_blockchain_index() {
  std::stringstream ss;
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  std::vector<Crypto::Hash> blockIds = m_blockIndex.getBlockIds(0, std::numeric_limits<uint32_t>::max());
  logger(INFO, BRIGHT_WHITE) << "Current blockchain index:";

  size_t height = 0;
  for (auto i = blockIds.begin(); i != blockIds.end(); ++i, ++height) {
    logger(INFO, BRIGHT_WHITE) << "id\t\t" << *i << " height" << height;
  }

}

void Blockchain::print_blockchain_outs(const std::string& file) {
  std::stringstream ss;
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  for (const outputs_container::value_type& v : m_outputs) {
    const std::vector<std::pair<TransactionIndex, uint16_t>>& vals = v.second;
    if (!vals.empty()) {
      ss << "amount: " << v.first << ENDL;
      for (size_t i = 0; i != vals.size(); i++) {
        ss << "\t" << getObjectHash(transactionByIndex(vals[i].first).tx) << ": " << vals[i].second << ENDL;
      }
    }
  }

  if (Common::saveStringToFile(file, ss.str())) {
    logger(INFO, BRIGHT_WHITE) <<
      "Current outputs index writen to file: " << file;
  } else {
    logger(WARNING, BRIGHT_YELLOW) <<
      "Failed to write current outputs index to file: " << file;
  }
}

std::vector<Crypto::Hash> Blockchain::findBlockchainSupplement(const std::vector<Crypto::Hash>& remoteBlockIds, size_t maxCount,
  uint32_t& totalBlockCount, uint32_t& startBlockIndex) {

  assert(!remoteBlockIds.empty());
  assert(remoteBlockIds.back() == m_blockIndex.getBlockId(0));

  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  totalBlockCount = getCurrentBlockchainHeight();
  startBlockIndex = findBlockchainSupplement(remoteBlockIds);

  return m_blockIndex.getBlockIds(startBlockIndex, static_cast<uint32_t>(maxCount));
}

bool Blockchain::haveBlock(const Crypto::Hash& id) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (m_blockIndex.hasBlock(id))
    return true;

  if (m_alternative_chains.count(id))
    return true;

  return false;
}

size_t Blockchain::getTotalTransactions() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return m_transactionMap.size();
}

bool Blockchain::getTransactionOutputGlobalIndexes(const Crypto::Hash& tx_id, std::vector<uint32_t>& indexs) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  auto it = m_transactionMap.find(tx_id);
  if (it == m_transactionMap.end()) {
    logger(WARNING, YELLOW) << "warning: get_tx_outputs_gindexs failed to find transaction with id = " << tx_id;
    return false;
  }

  const TransactionEntry& tx = transactionByIndex(it->second);
  if (!(tx.m_global_output_indexes.size())) { logger(ERROR, BRIGHT_RED) << "internal error: global indexes for transaction " << tx_id << " is empty"; return false; }
  indexs.resize(tx.m_global_output_indexes.size());
  for (size_t i = 0; i < tx.m_global_output_indexes.size(); ++i) {
    indexs[i] = tx.m_global_output_indexes[i];
  }

  return true;
}

bool Blockchain::get_out_by_msig_gindex(uint64_t amount, uint64_t gindex, MultisignatureOutput& out) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  auto it = m_multisignatureOutputs.find(amount);
  if (it == m_multisignatureOutputs.end()) {
    return false;
  }

  if (it->second.size() <= gindex) {
    return false;
  }

  auto msigUsage = it->second[gindex];
  auto& targetOut = transactionByIndex(msigUsage.transactionIndex).tx.outputs[msigUsage.outputIndex].target;
  if (targetOut.type() != typeid(MultisignatureOutput)) {
    return false;
  }

  out = boost::get<MultisignatureOutput>(targetOut);
  return true;
}



bool Blockchain::checkTransactionInputs(const Transaction& tx, uint32_t& max_used_block_height, Crypto::Hash& max_used_block_id, BlockInfo* tail) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  if (tail)
    tail->id = getTailId(tail->height);

  bool res = checkTransactionInputs(tx, &max_used_block_height);
  if (!res) return false;
  if (!(max_used_block_height < m_blocks.size())) { logger(ERROR, BRIGHT_RED) << "internal error: max used block index=" << max_used_block_height << " is not less then blockchain size = " << m_blocks.size(); return false; }
  get_block_hash(m_blocks[max_used_block_height].bl, max_used_block_id);
  return true;
}

bool Blockchain::haveTransactionKeyImagesAsSpent(const Transaction &tx) {
  for (const auto& in : tx.inputs) {
    if (in.type() == typeid(KeyInput)) {
      if (have_tx_keyimg_as_spent(boost::get<KeyInput>(in).keyImage)) {
        return true;
      }
    }
  }

  return false;
}

bool Blockchain::checkTransactionInputs(const Transaction& tx, uint32_t* pmax_used_block_height) {
  Crypto::Hash tx_prefix_hash = getObjectHash(*static_cast<const TransactionPrefix*>(&tx));
  return checkTransactionInputs(tx, tx_prefix_hash, pmax_used_block_height);
}

bool Blockchain::checkTransactionInputs(const Transaction& tx, const Crypto::Hash& tx_prefix_hash, uint32_t* pmax_used_block_height) {
  size_t inputIndex = 0;
  if (pmax_used_block_height) {
    *pmax_used_block_height = 0;
  }

  Crypto::Hash transactionHash = getObjectHash(tx);
  for (const auto& txin : tx.inputs) {
    assert(inputIndex < tx.signatures.size());
    if (txin.type() == typeid(KeyInput)) {

      const KeyInput& in_to_key = boost::get<KeyInput>(txin);
      if (!(!in_to_key.outputIndexes.empty())) { logger(ERROR, BRIGHT_RED) << "empty in_to_key.outputIndexes in transaction with id " << getObjectHash(tx); return false; }

      if (have_tx_keyimg_as_spent(in_to_key.keyImage)) {
        logger(DEBUGGING) <<
          "Key image already spent in blockchain: " << Common::podToHex(in_to_key.keyImage);
        return false;
      }

      if (!isInCheckpointZone(getCurrentBlockchainHeight())) {
        if (!check_tx_input(in_to_key, tx_prefix_hash, tx.signatures[inputIndex], pmax_used_block_height)) {
          logger(INFO, BRIGHT_WHITE) <<
            "Failed to check input in transaction " << transactionHash;
          return false;
        }
      }

      ++inputIndex;
    } else if (txin.type() == typeid(MultisignatureInput)) {
      if (!isInCheckpointZone(getCurrentBlockchainHeight())) {
        if (!validateInput(::boost::get<MultisignatureInput>(txin), transactionHash, tx_prefix_hash, tx.signatures[inputIndex])) {
          return false;
        }
      }
      ++inputIndex;
    } else {
      logger(INFO, BRIGHT_WHITE) <<
        "Transaction << " << transactionHash << " contains input of unsupported type.";
      return false;
    }
  }

  return true;
}

bool Blockchain::is_tx_spendtime_unlocked(uint64_t unlock_time) {
  if (unlock_time < m_currency.maxBlockHeight()) {
    //interpret as block index
    if (getCurrentBlockchainHeight() - 1 + m_currency.lockedTxAllowedDeltaBlocks() >= unlock_time)
      return true;
    else
      return false;
  } else {
    //interpret as time

    // compare with last block timestamp + delta seconds
    const uint64_t lastBlockTimestamp = getBlockTimestamp(getCurrentBlockchainHeight() - 1);
    if (lastBlockTimestamp + m_currency.lockedTxAllowedDeltaSeconds() >= unlock_time)
      return true;
    else
      return false;
  }

  return false;
}

bool Blockchain::is_tx_spendtime_unlocked(uint64_t unlock_time, uint32_t height) {
  if (unlock_time < m_currency.maxBlockHeight()) {
    //interpret as block index
    if (height - 1 + m_currency.lockedTxAllowedDeltaBlocks() >= unlock_time)
      return true;
  }
  
  return false;
}

bool Blockchain::check_tx_input(const KeyInput& txin, const Crypto::Hash& tx_prefix_hash, const std::vector<Crypto::Signature>& sig, uint32_t* pmax_related_block_height) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  struct outputs_visitor {
    std::vector<const Crypto::PublicKey *>& m_results_collector;
    Blockchain& m_bch;
    LoggerRef logger;
    outputs_visitor(std::vector<const Crypto::PublicKey *>& results_collector, Blockchain& bch, ILogger& logger) :m_results_collector(results_collector), m_bch(bch), logger(logger, "outputs_visitor") {
    }

    bool handle_output(const Transaction& tx, const TransactionOutput& out, size_t transactionOutputIndex) {
      //check tx unlock time
      if (!m_bch.is_tx_spendtime_unlocked(tx.unlockTime)) {
        logger(INFO, BRIGHT_WHITE) <<
          "One of outputs for one of inputs have wrong tx.unlockTime = " << tx.unlockTime;
        return false;
      }

      if (out.target.type() != typeid(KeyOutput)) {
        logger(INFO, BRIGHT_WHITE) <<
          "Output have wrong type id, which=" << out.target.which();
        return false;
      }

      m_results_collector.push_back(&boost::get<KeyOutput>(out.target).key);
      return true;
    }
  };

  // additional key_image check, fix discovered by Monero Lab and suggested by "fluffypony" (bitcointalk.org)
  if (!(scalarmultKey(txin.keyImage, Crypto::EllipticCurveScalar2KeyImage(Crypto::L)) == Crypto::EllipticCurveScalar2KeyImage(Crypto::I))) {
    logger(ERROR) << "Transaction uses key image not in the valid domain";
    return false;
  }

  //check ring signature
  std::vector<const Crypto::PublicKey *> output_keys;
  outputs_visitor vi(output_keys, *this, logger.getLogger());
  if (!scanOutputKeysForIndexes(txin, vi, pmax_related_block_height)) {
    logger(INFO, BRIGHT_WHITE) <<
      "Failed to get output keys for tx with amount = " << m_currency.formatAmount(txin.amount) <<
      " and count indexes " << txin.outputIndexes.size();
    return false;
  }

  if (txin.outputIndexes.size() != output_keys.size()) {
    logger(INFO, BRIGHT_WHITE) <<
      "Output keys for tx with amount = " << txin.amount << " and count indexes " << txin.outputIndexes.size() << " returned wrong keys count " << output_keys.size();
    return false;
  }

  if (!(sig.size() == output_keys.size())) { logger(ERROR, BRIGHT_RED) << "internal error: tx signatures count=" << sig.size() << " mismatch with outputs keys count for inputs=" << output_keys.size(); return false; }
  if (isInCheckpointZone(getCurrentBlockchainHeight())) {
    return true;
  }

  bool check_tx_ring_signature = Crypto::check_ring_signature(tx_prefix_hash, txin.keyImage, output_keys, sig.data());
  if (!check_tx_ring_signature) {
    logger(ERROR) << "Failed to check ring signature for keyImage: " << txin.keyImage;
  }
  return check_tx_ring_signature;
}

uint64_t Blockchain::get_adjusted_time() {
  //TODO: add collecting median time
  return time(NULL);
}

bool Blockchain::check_block_timestamp_main(const Block& b) {
  if (b.timestamp > get_adjusted_time() + m_currency.blockFutureTimeLimit(b.majorVersion)) {
    logger(INFO, BRIGHT_WHITE) <<
      "Timestamp of block with id: " << get_block_hash(b) << ", " << b.timestamp << ", bigger than adjusted time + 28 min.";
    return false;
  }

  std::vector<uint64_t> timestamps;
  size_t offset = m_blocks.size() <= m_currency.timestampCheckWindow(b.majorVersion) ? 0 : m_blocks.size() - m_currency.timestampCheckWindow(b.majorVersion);
  for (; offset != m_blocks.size(); ++offset) {
    timestamps.push_back(m_blocks[offset].bl.timestamp);
  }

  return check_block_timestamp(std::move(timestamps), b);
}

//------------------------------------------------------------------
// This function takes the timestamps from the most recent <n> blocks,
// where n = BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW. If there are not that many
// blocks in the blockchain, the timestap is assumed to be valid. If there
// are, this function returns:
//   true if the block's timestamp is not less than the median timestamp
//       of the selected blocks
//   false otherwise
bool Blockchain::check_block_timestamp(std::vector<uint64_t> timestamps, const Block& b) {
  if (timestamps.size() < m_currency.timestampCheckWindow(b.majorVersion)) {
    return true;
  }

  uint64_t median_ts = Common::medianValue(timestamps);

  if (b.timestamp < median_ts) {
    logger(INFO, BRIGHT_WHITE) <<
      "Timestamp of block with id: " << get_block_hash(b) << ", " << b.timestamp <<
      ", less than median of last " << m_currency.timestampCheckWindow(b.majorVersion) << " blocks, " << median_ts;
    return false;
  }

  return true;
}

bool Blockchain::checkBlockVersion(const Block& b, const Crypto::Hash& blockHash) {
  uint32_t height = get_block_height(b);
  const uint8_t expectedBlockVersion = getBlockMajorVersionForHeight(height);
  if (b.majorVersion != expectedBlockVersion) {
    logger(TRACE) << "Block " << blockHash << " has wrong major version: " << static_cast<int>(b.majorVersion) <<
      ", at height " << height << " expected version is " << static_cast<int>(expectedBlockVersion);
    return false;
  }

  return true;
}

bool Blockchain::checkParentBlockSize(const Block& b, const Crypto::Hash& blockHash) {
  if (b.majorVersion == BLOCK_MAJOR_VERSION_2 || b.majorVersion == BLOCK_MAJOR_VERSION_3) {
    auto serializer = makeParentBlockSerializer(b, false, false);
    size_t parentBlockSize;
    if (!getObjectBinarySize(serializer, parentBlockSize)) {
      logger(ERROR, BRIGHT_RED) <<
        "Block " << blockHash << ": failed to determine parent block size";
      return false;
    }

    if (parentBlockSize > 2 * 1024) {
      logger(INFO, BRIGHT_WHITE) <<
        "Block " << blockHash << " contains too big parent block: " << parentBlockSize <<
        " bytes, expected no more than " << 2 * 1024 << " bytes";
      return false;
    }
  }

  return true;
}

bool Blockchain::checkCumulativeBlockSize(const Crypto::Hash& blockId, size_t cumulativeBlockSize, uint64_t height) {
  size_t maxBlockCumulativeSize = m_currency.maxBlockCumulativeSize(height);
  if (cumulativeBlockSize > maxBlockCumulativeSize) {
    logger(INFO, BRIGHT_WHITE) <<
      "Block " << blockId << " is too big: " << cumulativeBlockSize << " bytes, " <<
      "expected no more than " << maxBlockCumulativeSize << " bytes";
    return false;
  }

  return true;
}

// Returns true, if cumulativeSize is calculated precisely, else returns false.
bool Blockchain::getBlockCumulativeSize(const Block& block, size_t& cumulativeSize) {
  std::vector<Transaction> blockTxs;
  std::vector<Crypto::Hash> missedTxs;
  getTransactions(block.transactionHashes, blockTxs, missedTxs, true);

  cumulativeSize = getObjectBinarySize(block.baseTransaction);
  for (const Transaction& tx : blockTxs) {
    cumulativeSize += getObjectBinarySize(tx);
  }

  return missedTxs.empty();
}

// Precondition: m_blockchain_lock is locked.
bool Blockchain::update_next_cumulative_size_limit() {
  uint8_t nextBlockMajorVersion = getBlockMajorVersionForHeight(static_cast<uint32_t>(m_blocks.size()));
  size_t nextBlockGrantedFullRewardZone = m_currency.blockGrantedFullRewardZoneByBlockVersion(nextBlockMajorVersion);

  std::vector<size_t> sz;
  get_last_n_blocks_sizes(sz, m_currency.rewardBlocksWindow());

  uint64_t median = Common::medianValue(sz);
  if (median <= nextBlockGrantedFullRewardZone) {
    median = nextBlockGrantedFullRewardZone;
  }

  m_current_block_cumul_sz_limit = median * 2;
  return true;
}

bool Blockchain::addNewBlock(const Block& bl, block_verification_context& bvc) {
  Crypto::Hash id;
  if (!get_block_hash(bl, id)) {
    logger(ERROR, BRIGHT_RED) <<
      "Failed to get block hash, possible block has invalid format";
    bvc.m_verification_failed = true;
    return false;
  }

  bool add_result;

  { //to avoid deadlock lets lock tx_pool for whole add/reorganize process
    std::lock_guard<decltype(m_tx_pool)> poolLock(m_tx_pool);
    std::lock_guard<decltype(m_blockchain_lock)> bcLock(m_blockchain_lock);

    if (haveBlock(id)) {
      logger(TRACE) << "block with id = " << id << " already exists";
      bvc.m_already_exists = true;
      return false;
    }

    //check that block refers to chain tail
    if (!(bl.previousBlockHash == getTailId())) {
      //chain switching or wrong block
      logger(DEBUGGING) << "handling alternative block " << Common::podToHex(id)
                        << " at height " << boost::get<BaseInput>(bl.baseTransaction.inputs.front()).blockIndex 
                        << " as it doesn't refer to chain tail " << Common::podToHex(getTailId())
                        << ", its prev. block hash: " << Common::podToHex(bl.previousBlockHash);
      bvc.m_added_to_main_chain = false;
      add_result = handle_alternative_block(bl, id, bvc);
    } else {
      add_result = pushBlock(bl, id, bvc);
      if (add_result) {
        sendMessage(BlockchainMessage(NewBlockMessage(id)));
      }
    }
  }

  if (add_result && bvc.m_added_to_main_chain) {
    m_observerManager.notify(&IBlockchainStorageObserver::blockchainUpdated);
  }

  return add_result;
}

const Blockchain::TransactionEntry& Blockchain::transactionByIndex(TransactionIndex index) {
  return m_blocks[index.block].transactions[index.transaction];
}

bool Blockchain::pushBlock(const Block& blockData, const Crypto::Hash& id, block_verification_context& bvc) {
  std::vector<Transaction> transactions;
  if (!loadTransactions(blockData, transactions)) {
    bvc.m_verification_failed = true;
    return false;
  }

  if (!pushBlock(blockData, transactions, id, bvc)) {
    saveTransactions(transactions);
    return false;
  }

  return true;
}

bool Blockchain::pushBlock(const Block& blockData, const std::vector<Transaction>& transactions, const Crypto::Hash& blockHash, block_verification_context& bvc) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  auto blockProcessingStart = std::chrono::steady_clock::now();

  if (m_blockIndex.hasBlock(blockHash)) {
    logger(ERROR, BRIGHT_RED) <<
      "Block " << blockHash << " already exists in blockchain.";
    bvc.m_verification_failed = true;
    return false;
  }

  if (!checkBlockVersion(blockData, blockHash)) {
    bvc.m_verification_failed = true;
    return false;
  }

  //if (!checkParentBlockSize(blockData, blockHash)) {
  //  bvc.m_verification_failed = true;
  //  return false;
  //}

  // Disable merged mining
  if (blockData.majorVersion >= CryptoNote::BLOCK_MAJOR_VERSION_5) {
    TransactionExtraMergeMiningTag mmTag;
    if (getMergeMiningTagFromExtra(blockData.baseTransaction.extra, mmTag)) {
      logger(ERROR, BRIGHT_RED) << "Merge mining tag was found in extra of miner transaction";
      return false;
    }
  }

  if (blockData.previousBlockHash != getTailId()) {
    logger(INFO, BRIGHT_WHITE) <<
      "Block " << blockHash << " has wrong previousBlockHash: " << blockData.previousBlockHash << ", expected: " << getTailId();
    bvc.m_verification_failed = true;
    return false;
  }

  // make sure block timestamp is not less than the median timestamp
  // of a set number of the most recent blocks.
  if (!check_block_timestamp_main(blockData)) {
    logger(INFO, BRIGHT_WHITE) <<
      "Block " << blockHash << " has invalid timestamp: " << blockData.timestamp;
    bvc.m_verification_failed = true;
    return false;
  }

  auto targetTimeStart = std::chrono::steady_clock::now();
  difficulty_type currentDifficulty = getDifficultyForNextBlock();
  auto target_calculating_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - targetTimeStart).count();

  if (!(currentDifficulty)) {
    logger(ERROR, BRIGHT_RED) << "!!!!!!!!! difficulty overhead !!!!!!!!!";
    return false;
  }

  auto longhashTimeStart = std::chrono::steady_clock::now();
  Crypto::Hash proof_of_work = NULL_HASH;
  if (m_checkpoints.is_in_checkpoint_zone(getCurrentBlockchainHeight())) {
    if (!m_checkpoints.check_block(getCurrentBlockchainHeight(), blockHash)) {
      logger(ERROR, BRIGHT_RED) <<
        "CHECKPOINT VALIDATION FAILED";
      bvc.m_verification_failed = true;
      return false;
    }
  } else {
    if (!m_currency.checkProofOfWork(m_cn_context, blockData, currentDifficulty, proof_of_work)) {
      logger(INFO, BRIGHT_WHITE) <<
        "Block " << blockHash << ", has too weak proof of work: " << proof_of_work << ", expected difficulty: " << currentDifficulty;
      bvc.m_verification_failed = true;
      return false;
    }
  }

  auto longhash_calculating_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - longhashTimeStart).count();

  if (!prevalidate_miner_transaction(blockData, static_cast<uint32_t>(m_blocks.size()))) {
    logger(INFO, BRIGHT_WHITE) <<
      "Block " << blockHash << " failed to pass prevalidation";
    bvc.m_verification_failed = true;
    return false;
  }

  Crypto::Hash minerTransactionHash = getObjectHash(blockData.baseTransaction);

  BlockEntry block;
  block.bl = blockData;
  block.transactions.resize(1);
  block.transactions[0].tx = blockData.baseTransaction;
  TransactionIndex transactionIndex = { static_cast<uint32_t>(m_blocks.size()), static_cast<uint16_t>(0) };
  pushTransaction(block, minerTransactionHash, transactionIndex);

  size_t coinbase_blob_size = getObjectBinarySize(blockData.baseTransaction);
  size_t cumulative_block_size = coinbase_blob_size;
  uint64_t fee_summary = 0;
  for (size_t i = 0; i < transactions.size(); ++i) {
    const Crypto::Hash& tx_id = blockData.transactionHashes[i];
    block.transactions.resize(block.transactions.size() + 1);
    size_t blob_size = 0;
    uint64_t fee = 0;
    block.transactions.back().tx = transactions[i];

    blob_size = toBinaryArray(block.transactions.back().tx).size();
    fee = getInputAmount(block.transactions.back().tx) - getOutputAmount(block.transactions.back().tx);
    if (!checkTransactionInputs(block.transactions.back().tx)) {
      logger(INFO, BRIGHT_WHITE) <<
        "Block " << blockHash << " has at least one transaction with wrong inputs: " << tx_id;
      bvc.m_verification_failed = true;

      block.transactions.pop_back();
      popTransactions(block, minerTransactionHash);
      return false;
    }

    ++transactionIndex.transaction;
    pushTransaction(block, tx_id, transactionIndex);

    cumulative_block_size += blob_size;
    fee_summary += fee;
  }

  if (!checkCumulativeBlockSize(blockHash, cumulative_block_size, m_blocks.size())) {
    bvc.m_verification_failed = true;
    return false;
  }

  int64_t emissionChange = 0;
  uint64_t reward = 0;
  uint64_t already_generated_coins = m_blocks.empty() ? 0 : m_blocks.back().already_generated_coins;
  if (!validate_miner_transaction(blockData, static_cast<uint32_t>(m_blocks.size()), cumulative_block_size, already_generated_coins, fee_summary, reward, emissionChange)) {
    logger(INFO, BRIGHT_WHITE) << "Block " << blockHash << " has invalid miner transaction";
    bvc.m_verification_failed = true;
    popTransactions(block, minerTransactionHash);
    return false;
  }

  block.height = static_cast<uint32_t>(m_blocks.size());
  block.block_cumulative_size = cumulative_block_size;
  block.cumulative_difficulty = currentDifficulty;
  block.already_generated_coins = already_generated_coins + emissionChange;
  if (m_blocks.size() > 0) {
    block.cumulative_difficulty += m_blocks.back().cumulative_difficulty;
  }

  pushBlock(block, blockHash);

  auto block_processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - blockProcessingStart).count();

  if (block.height % 1000 == 0) {
    logger(INFO) << "Blockchain loaded to height: " << block.height;
  }

  logger(DEBUGGING) <<
    "+++++ BLOCK SUCCESSFULLY ADDED" << ENDL << "id:\t" << blockHash
    << ENDL << "PoW:\t" << proof_of_work
    << ENDL << "HEIGHT " << block.height << ", difficulty:\t" << currentDifficulty
    << ENDL << "block reward: " << m_currency.formatAmount(reward) << ", fee = " << m_currency.formatAmount(fee_summary)
    << ", coinbase_blob_size: " << coinbase_blob_size << ", cumulative size: " << cumulative_block_size
    << ", " << block_processing_time << "(" << target_calculating_time << "/" << longhash_calculating_time << ")ms";

  bvc.m_added_to_main_chain = true;

  m_upgradeDetectorV2.blockPushed();
  m_upgradeDetectorV3.blockPushed();
  m_upgradeDetectorV4.blockPushed();
  m_upgradeDetectorV5.blockPushed();

  update_next_cumulative_size_limit();

  return true;
}

bool Blockchain::pushBlock(BlockEntry& block, const Crypto::Hash& blockHash) {
  m_blocks.push_back(block);
  m_blockIndex.push(blockHash);
  m_timestampIndex.add(block.bl.timestamp, blockHash);
  m_generatedTransactionsIndex.add(block.bl);

  assert(m_blockIndex.size() == m_blocks.size());

  return true;
}

void Blockchain::popBlock() {
  if (m_blocks.empty()) {
    logger(ERROR, BRIGHT_RED) <<
      "Attempt to pop block from empty blockchain.";
    return;
  }

  std::vector<Transaction> transactions(m_blocks.back().transactions.size() - 1);
  for (size_t i = 0; i < m_blocks.back().transactions.size() - 1; ++i) {
    transactions[i] = m_blocks.back().transactions[1 + i].tx;
  }

  saveTransactions(transactions);
  removeLastBlock();

  m_upgradeDetectorV2.blockPopped();
  m_upgradeDetectorV3.blockPopped();
  m_upgradeDetectorV4.blockPopped();
  m_upgradeDetectorV5.blockPopped();
}

bool Blockchain::pushTransaction(BlockEntry& block, const Crypto::Hash& transactionHash, TransactionIndex transactionIndex) {
  auto result = m_transactionMap.insert(std::make_pair(transactionHash, transactionIndex));
  if (!result.second) {
    logger(ERROR, BRIGHT_RED) <<
      "Duplicate transaction was pushed to blockchain.";
    return false;
  }

  TransactionEntry& transaction = block.transactions[transactionIndex.transaction];

  if (!checkMultisignatureInputsDiff(transaction.tx)) {
    logger(ERROR, BRIGHT_RED) <<
      "Double spending transaction was pushed to blockchain.";
    m_transactionMap.erase(transactionHash);
    return false;
  }

  for (size_t i = 0; i < transaction.tx.inputs.size(); ++i) {
    if (transaction.tx.inputs[i].type() == typeid(KeyInput)) {
      auto result = m_spent_key_images.insert(std::make_pair(::boost::get<KeyInput>(transaction.tx.inputs[i]).keyImage, block.height));
      if (!result.second) {
        logger(ERROR, BRIGHT_RED) <<
          "Double spending transaction was pushed to blockchain.";
        for (size_t j = 0; j < i; ++j) {
          m_spent_key_images.erase(::boost::get<KeyInput>(transaction.tx.inputs[i - 1 - j]).keyImage);
        }
        m_transactionMap.erase(transactionHash);
        return false;
      }
    }
  }

  for (const auto& inv : transaction.tx.inputs) {
    if (inv.type() == typeid(MultisignatureInput)) {
      const MultisignatureInput& in = ::boost::get<MultisignatureInput>(inv);
      auto& amountOutputs = m_multisignatureOutputs[in.amount];
      amountOutputs[in.outputIndex].isUsed = true;
    }
  }

  transaction.m_global_output_indexes.resize(transaction.tx.outputs.size());
  for (uint16_t output = 0; output < transaction.tx.outputs.size(); ++output) {
    if (transaction.tx.outputs[output].target.type() == typeid(KeyOutput)) {
      auto& amountOutputs = m_outputs[transaction.tx.outputs[output].amount];
      transaction.m_global_output_indexes[output] = static_cast<uint32_t>(amountOutputs.size());
      amountOutputs.push_back(std::make_pair<>(transactionIndex, output));
    } else if (transaction.tx.outputs[output].target.type() == typeid(MultisignatureOutput)) {
      auto& amountOutputs = m_multisignatureOutputs[transaction.tx.outputs[output].amount];
      transaction.m_global_output_indexes[output] = static_cast<uint32_t>(amountOutputs.size());
      MultisignatureOutputUsage outputUsage = { transactionIndex, output, false };
      amountOutputs.push_back(outputUsage);
    }
  }

  m_paymentIdIndex.add(transaction.tx);

  return true;
}

void Blockchain::popTransaction(const Transaction& transaction, const Crypto::Hash& transactionHash) {
  TransactionIndex transactionIndex = m_transactionMap.at(transactionHash);
  for (size_t outputIndex = 0; outputIndex < transaction.outputs.size(); ++outputIndex) {
    const TransactionOutput& output = transaction.outputs[transaction.outputs.size() - 1 - outputIndex];
    if (output.target.type() == typeid(KeyOutput)) {
      auto amountOutputs = m_outputs.find(output.amount);
      if (amountOutputs == m_outputs.end()) {
        logger(ERROR, BRIGHT_RED) <<
          "Blockchain consistency broken - cannot find specific amount in outputs map.";
        continue;
      }

      if (amountOutputs->second.empty()) {
        logger(ERROR, BRIGHT_RED) <<
          "Blockchain consistency broken - output array for specific amount is empty.";
        continue;
      }

      if (amountOutputs->second.back().first.block != transactionIndex.block || amountOutputs->second.back().first.transaction != transactionIndex.transaction) {
        logger(ERROR, BRIGHT_RED) <<
          "Blockchain consistency broken - invalid transaction index.";
        continue;
      }

      if (amountOutputs->second.back().second != transaction.outputs.size() - 1 - outputIndex) {
        logger(ERROR, BRIGHT_RED) <<
          "Blockchain consistency broken - invalid output index.";
        continue;
      }

      amountOutputs->second.pop_back();
      if (amountOutputs->second.empty()) {
        m_outputs.erase(amountOutputs);
      }
    } else if (output.target.type() == typeid(MultisignatureOutput)) {
      auto amountOutputs = m_multisignatureOutputs.find(output.amount);
      if (amountOutputs == m_multisignatureOutputs.end()) {
        logger(ERROR, BRIGHT_RED) <<
          "Blockchain consistency broken - cannot find specific amount in outputs map.";
        continue;
      }

      if (amountOutputs->second.empty()) {
        logger(ERROR, BRIGHT_RED) <<
          "Blockchain consistency broken - output array for specific amount is empty.";
        continue;
      }

      if (amountOutputs->second.back().isUsed) {
        logger(ERROR, BRIGHT_RED) <<
          "Blockchain consistency broken - attempting to remove used output.";
        continue;
      }

      if (amountOutputs->second.back().transactionIndex.block != transactionIndex.block || amountOutputs->second.back().transactionIndex.transaction != transactionIndex.transaction) {
        logger(ERROR, BRIGHT_RED) <<
          "Blockchain consistency broken - invalid transaction index.";
        continue;
      }

      if (amountOutputs->second.back().outputIndex != transaction.outputs.size() - 1 - outputIndex) {
        logger(ERROR, BRIGHT_RED) <<
          "Blockchain consistency broken - invalid output index.";
        continue;
      }

      amountOutputs->second.pop_back();
      if (amountOutputs->second.empty()) {
        m_multisignatureOutputs.erase(amountOutputs);
      }
    }
  }

  for (auto& input : transaction.inputs) {
    if (input.type() == typeid(KeyInput)) {
      size_t count = m_spent_key_images.erase(::boost::get<KeyInput>(input).keyImage);
      if (count != 1) {
        logger(ERROR, BRIGHT_RED) <<
          "Blockchain consistency broken - cannot find spent key.";
      }
    } else if (input.type() == typeid(MultisignatureInput)) {
      const MultisignatureInput& in = ::boost::get<MultisignatureInput>(input);
      auto& amountOutputs = m_multisignatureOutputs[in.amount];
      if (!amountOutputs[in.outputIndex].isUsed) {
        logger(ERROR, BRIGHT_RED) <<
          "Blockchain consistency broken - multisignature output not marked as used.";
      }

      amountOutputs[in.outputIndex].isUsed = false;
    }
  }

  m_paymentIdIndex.remove(transaction);

  size_t count = m_transactionMap.erase(transactionHash);
  if (count != 1) {
    logger(ERROR, BRIGHT_RED) <<
      "Blockchain consistency broken - cannot find transaction by hash.";
  }
}

void Blockchain::popTransactions(const BlockEntry& block, const Crypto::Hash& minerTransactionHash) {
  for (size_t i = 0; i < block.transactions.size() - 1; ++i) {
    popTransaction(block.transactions[block.transactions.size() - 1 - i].tx, block.bl.transactionHashes[block.transactions.size() - 2 - i]);
  }

  popTransaction(block.bl.baseTransaction, minerTransactionHash);
}

bool Blockchain::validateInput(const MultisignatureInput& input, const Crypto::Hash& transactionHash, const Crypto::Hash& transactionPrefixHash, const std::vector<Crypto::Signature>& transactionSignatures) {
  assert(input.signatureCount == transactionSignatures.size());
  MultisignatureOutputsContainer::const_iterator amountOutputs = m_multisignatureOutputs.find(input.amount);
  if (amountOutputs == m_multisignatureOutputs.end()) {
    logger(DEBUGGING) <<
      "Transaction << " << transactionHash << " contains multisignature input with invalid amount.";
    return false;
  }

  if (input.outputIndex >= amountOutputs->second.size()) {
    logger(DEBUGGING) <<
      "Transaction << " << transactionHash << " contains multisignature input with invalid outputIndex.";
    return false;
  }

  const MultisignatureOutputUsage& outputIndex = amountOutputs->second[input.outputIndex];
  if (outputIndex.isUsed) {
    logger(DEBUGGING) <<
      "Transaction << " << transactionHash << " contains double spending multisignature input.";
    return false;
  }

  const Transaction& outputTransaction = m_blocks[outputIndex.transactionIndex.block].transactions[outputIndex.transactionIndex.transaction].tx;
  if (!is_tx_spendtime_unlocked(outputTransaction.unlockTime)) {
    logger(DEBUGGING) <<
      "Transaction << " << transactionHash << " contains multisignature input which points to a locked transaction.";
    return false;
  }

  assert(outputTransaction.outputs[outputIndex.outputIndex].amount == input.amount);
  assert(outputTransaction.outputs[outputIndex.outputIndex].target.type() == typeid(MultisignatureOutput));
  const MultisignatureOutput& output = ::boost::get<MultisignatureOutput>(outputTransaction.outputs[outputIndex.outputIndex].target);
  if (input.signatureCount != output.requiredSignatureCount) {
    logger(DEBUGGING) <<
      "Transaction << " << transactionHash << " contains multisignature input with invalid signature count.";
    return false;
  }

  size_t inputSignatureIndex = 0;
  size_t outputKeyIndex = 0;
  while (inputSignatureIndex < input.signatureCount) {
    if (outputKeyIndex == output.keys.size()) {
      logger(DEBUGGING) <<
        "Transaction << " << transactionHash << " contains multisignature input with invalid signatures.";
      return false;
    }

    if (Crypto::check_signature(transactionPrefixHash, output.keys[outputKeyIndex], transactionSignatures[inputSignatureIndex])) {
      ++inputSignatureIndex;
    }

    ++outputKeyIndex;
  }

  return true;
}

bool Blockchain::checkCheckpoints(uint32_t& lastValidCheckpointHeight) {
  std::vector<uint32_t> checkpointHeights = m_checkpoints.getCheckpointHeights();
  for (const auto& checkpointHeight : checkpointHeights) {
    if (m_blocks.size() <= checkpointHeight) {
      return true;
    }

    if(m_checkpoints.check_block(checkpointHeight, getBlockIdByHeight(checkpointHeight))) {
      lastValidCheckpointHeight = checkpointHeight;
    } else {
      return false;
    }
  }

  return true;
}

void Blockchain::rollbackBlockchainTo(uint32_t height) {
  while (height + 1 < m_blocks.size()) {
    removeLastBlock();
  }
}

void Blockchain::removeLastBlock() {
  if (m_blocks.empty()) {
    logger(ERROR, BRIGHT_RED) <<
      "Attempt to pop block from empty blockchain.";
    return;
  }

  logger(DEBUGGING) << "Removing last block with height " << m_blocks.back().height;
  popTransactions(m_blocks.back(), getObjectHash(m_blocks.back().bl.baseTransaction));

  Crypto::Hash blockHash = getBlockIdByHeight(m_blocks.back().height);
  m_timestampIndex.remove(m_blocks.back().bl.timestamp, blockHash);
  m_generatedTransactionsIndex.remove(m_blocks.back().bl);

  m_blocks.pop_back();
  m_blockIndex.pop();

  assert(m_blockIndex.size() == m_blocks.size());
}

bool Blockchain::checkUpgradeHeight(const UpgradeDetector& upgradeDetector) {
  uint32_t upgradeHeight = upgradeDetector.upgradeHeight();
  if (upgradeHeight != UpgradeDetectorBase::UNDEF_HEIGHT && upgradeHeight + 1 < m_blocks.size()) {
    logger(INFO) << "Checking block version at " << upgradeHeight + 1;
    if (m_blocks[upgradeHeight + 1].bl.majorVersion != upgradeDetector.targetVersion()) {
      return false;
    }
  }

  return true;
}

bool Blockchain::getLowerBound(uint64_t timestamp, uint64_t startOffset, uint32_t& height) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  assert(startOffset < m_blocks.size());

  auto bound = std::lower_bound(m_blocks.begin() + startOffset, m_blocks.end(), timestamp - m_currency.blockFutureTimeLimit(),
    [](const BlockEntry& b, uint64_t timestamp) { return b.bl.timestamp < timestamp; });

  if (bound == m_blocks.end()) {
    return false;
  }

  height = static_cast<uint32_t>(std::distance(m_blocks.begin(), bound));
  return true;
}

std::vector<Crypto::Hash> Blockchain::getBlockIds(uint32_t startHeight, uint32_t maxCount) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return m_blockIndex.getBlockIds(startHeight, maxCount);
}

bool Blockchain::getBlockContainingTransaction(const Crypto::Hash& txId, Crypto::Hash& blockId, uint32_t& blockHeight) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  auto it = m_transactionMap.find(txId);
  if (it == m_transactionMap.end()) {
    return false;
  } else {
    blockHeight = m_blocks[it->second.block].height;
    blockId = getBlockIdByHeight(blockHeight);
    return true;
  }
}

bool Blockchain::getAlreadyGeneratedCoins(const Crypto::Hash& hash, uint64_t& generatedCoins) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  // try to find block in main chain
  uint32_t height = 0;
  if (m_blockIndex.getBlockHeight(hash, height)) {
    generatedCoins = m_blocks[height].already_generated_coins;
    return true;
  }

  // try to find block in alternative chain
  auto blockByHashIterator = m_alternative_chains.find(hash);
  if (blockByHashIterator != m_alternative_chains.end()) {
    generatedCoins = blockByHashIterator->second.already_generated_coins;
    return true;
  }

  logger(DEBUGGING) << "Can't find block with hash " << hash << " to get already generated coins.";
  return false;
}

bool Blockchain::getBlockSize(const Crypto::Hash& hash, size_t& size) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  // try to find block in main chain
  uint32_t height = 0;
  if (m_blockIndex.getBlockHeight(hash, height)) {
    size = m_blocks[height].block_cumulative_size;
    return true;
  }

  // try to find block in alternative chain
  auto blockByHashIterator = m_alternative_chains.find(hash);
  if (blockByHashIterator != m_alternative_chains.end()) {
    size = blockByHashIterator->second.block_cumulative_size;
    return true;
  }

  logger(DEBUGGING) << "Can't find block with hash " << hash << " to get block size.";
  return false;
}

bool Blockchain::getMultisigOutputReference(const MultisignatureInput& txInMultisig, std::pair<Crypto::Hash, size_t>& outputReference) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  MultisignatureOutputsContainer::const_iterator amountIter = m_multisignatureOutputs.find(txInMultisig.amount);
  if (amountIter == m_multisignatureOutputs.end()) {
    logger(DEBUGGING) << "Transaction contains multisignature input with invalid amount.";
    return false;
  }
  if (amountIter->second.size() <= txInMultisig.outputIndex) {
    logger(DEBUGGING) << "Transaction contains multisignature input with invalid outputIndex.";
    return false;
  }
  const MultisignatureOutputUsage& outputIndex = amountIter->second[txInMultisig.outputIndex];
  const Transaction& outputTransaction = m_blocks[outputIndex.transactionIndex.block].transactions[outputIndex.transactionIndex.transaction].tx;
  outputReference.first = getObjectHash(outputTransaction);
  outputReference.second = outputIndex.outputIndex;
  return true;
}

bool Blockchain::storeBlockchainIndices() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  logger(INFO, BRIGHT_WHITE) << "Saving blockchain indices...";
  BlockchainIndicesSerializer ser(*this, getTailId(), logger.getLogger());

  if (!storeToBinaryFile(ser, appendPath(m_config_folder, m_currency.blockchainIndicesFileName()))) {
    logger(ERROR, BRIGHT_RED) << "Failed to save blockchain indices";
    return false;
  }

  return true;
}

bool Blockchain::loadBlockchainIndices() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  logger(INFO, BRIGHT_WHITE) << "Loading blockchain indices for BlockchainExplorer...";
  BlockchainIndicesSerializer loader(*this, get_block_hash(m_blocks.back().bl), logger.getLogger());

  loadFromBinaryFile(loader, appendPath(m_config_folder, m_currency.blockchainIndicesFileName()));

  if (!loader.loaded()) {
    logger(WARNING, BRIGHT_YELLOW) << "No actual blockchain indices for BlockchainExplorer found, rebuilding...";
    std::chrono::steady_clock::time_point timePoint = std::chrono::steady_clock::now();

    m_paymentIdIndex.clear();
    m_timestampIndex.clear();
    m_generatedTransactionsIndex.clear();

    for (uint32_t b = 0; b < m_blocks.size(); ++b) {
      if (b % 1000 == 0) {
        logger(INFO, BRIGHT_WHITE) << "Height " << b << " of " << m_blocks.size();
      }
      const BlockEntry& block = m_blocks[b];
      m_timestampIndex.add(block.bl.timestamp, get_block_hash(block.bl));
      m_generatedTransactionsIndex.add(block.bl);
      for (uint16_t t = 0; t < block.transactions.size(); ++t) {
        const TransactionEntry& transaction = block.transactions[t];
        m_paymentIdIndex.add(transaction.tx);
      }
    }

    std::chrono::duration<double> duration = std::chrono::steady_clock::now() - timePoint;
    logger(INFO, BRIGHT_WHITE) << "Rebuilding blockchain indices took: " << duration.count();
  }
  return true;
}

bool Blockchain::getGeneratedTransactionsNumber(uint32_t height, uint64_t& generatedTransactions) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return m_generatedTransactionsIndex.find(height, generatedTransactions);
}

bool Blockchain::getOrphanBlockIdsByHeight(uint32_t height, std::vector<Crypto::Hash>& blockHashes) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return m_orphanBlocksIndex.find(height, blockHashes);
}

bool Blockchain::getBlockIdsByTimestamp(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<Crypto::Hash>& hashes, uint32_t& blocksNumberWithinTimestamps) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return m_timestampIndex.find(timestampBegin, timestampEnd, blocksNumberLimit, hashes, blocksNumberWithinTimestamps);
}

bool Blockchain::getTransactionIdsByPaymentId(const Crypto::Hash& paymentId, std::vector<Crypto::Hash>& transactionHashes) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return m_paymentIdIndex.find(paymentId, transactionHashes);
}

bool Blockchain::loadTransactions(const Block& block, std::vector<Transaction>& transactions) {
  transactions.resize(block.transactionHashes.size());
  size_t transactionSize;
  uint64_t fee;
  for (size_t i = 0; i < block.transactionHashes.size(); ++i) {
    if (!m_tx_pool.take_tx(block.transactionHashes[i], transactions[i], transactionSize, fee)) {
      tx_verification_context context;
      for (size_t j = 0; j < i; ++j) {
        if (!m_tx_pool.add_tx(transactions[i - 1 - j], context, true)) {
          throw std::runtime_error("Blockchain::loadTransactions, failed to add transaction to pool");
        }
      }

      return false;
    }
  }

  return true;
}

void Blockchain::saveTransactions(const std::vector<Transaction>& transactions) {
  tx_verification_context context;
  for (size_t i = 0; i < transactions.size(); ++i) {
    if (!m_tx_pool.add_tx(transactions[transactions.size() - 1 - i], context, true)) {
      logger(WARNING, BRIGHT_YELLOW) << "Blockchain::saveTransactions, failed to add transaction to pool";
    }
  }
}

bool Blockchain::addMessageQueue(MessageQueue<BlockchainMessage>& messageQueue) {
  return m_messageQueueList.insert(messageQueue);
}

bool Blockchain::removeMessageQueue(MessageQueue<BlockchainMessage>& messageQueue) {
  return m_messageQueueList.remove(messageQueue);
}

void Blockchain::sendMessage(const BlockchainMessage& message) {
  for (IntrusiveLinkedList<MessageQueue<BlockchainMessage>>::iterator iter = m_messageQueueList.begin(); iter != m_messageQueueList.end(); ++iter) {
    iter->push(message);
  }
}

bool Blockchain::isBlockInMainChain(const Crypto::Hash& blockId) {
  return m_blockIndex.hasBlock(blockId);
}

bool Blockchain::isInCheckpointZone(const uint32_t height) {
  return m_checkpoints.is_in_checkpoint_zone(height);
}

}
