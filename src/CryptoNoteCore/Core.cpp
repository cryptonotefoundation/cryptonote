// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
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

#include "Core.h"

#include <sstream>
#include <unordered_set>
#include <boost/utility/value_init.hpp>
#include <boost/range/combine.hpp>
#include "../CryptoNoteConfig.h"
#include "../Common/CommandLine.h"
#include "../Common/Util.h"
#include "../Common/Math.h"
#include "../Common/StringTools.h"
#include "../crypto/crypto.h"
#include "../CryptoNoteProtocol/CryptoNoteProtocolDefinitions.h"
#include "../Logging/LoggerRef.h"
#include "../Rpc/CoreRpcServerCommandsDefinitions.h"
#include "CryptoNoteFormatUtils.h"
#include "CryptoNoteTools.h"
#include "CryptoNoteStatInfo.h"
#include "Miner.h"
#include "TransactionExtra.h"
#include "IBlock.h"

#undef ERROR

using namespace Logging;
#include "CryptoNoteCore/CoreConfig.h"

using namespace  Common;

namespace CryptoNote {

class BlockWithTransactions : public IBlock {
public:
  virtual const Block& getBlock() const override {
    return block;
  }

  virtual size_t getTransactionCount() const override {
    return transactions.size();
  }

  virtual const Transaction& getTransaction(size_t index) const override {
    assert(index < transactions.size());
    return transactions[index];
  }

private:
  Block block;
  std::vector<Transaction> transactions;

  friend class Core;
};

Core::Core(const Currency& currency, i_cryptonote_protocol* pprotocol, Logging::ILogger& logger, System::Dispatcher& dispatcher, bool blockchainIndexesEnabled) :
  m_dispatcher(dispatcher),
  m_currency(currency),
  logger(logger, "Core"),
  m_mempool(currency, m_blockchain, *this, m_timeProvider, logger, blockchainIndexesEnabled),
  m_blockchain(currency, m_mempool, logger, blockchainIndexesEnabled),
  m_miner(new miner(currency, *this, logger)),
  m_starter_message_showed(false),
  m_checkpoints(logger) {
    set_cryptonote_protocol(pprotocol);
    m_blockchain.addObserver(this);
    m_mempool.addObserver(this);
  }
  //-----------------------------------------------------------------------------------------------
  Core::~Core() {
  m_blockchain.removeObserver(this);
}

void Core::set_cryptonote_protocol(i_cryptonote_protocol* pprotocol) {
  if (pprotocol)
    m_pprotocol = pprotocol;
  else
    m_pprotocol = &m_protocol_stub;
}
//-----------------------------------------------------------------------------------
void Core::set_checkpoints(Checkpoints&& chk_pts) {
  m_blockchain.setCheckpoints(std::move(chk_pts));
  m_checkpoints = std::move(chk_pts);
}
//-----------------------------------------------------------------------------------
void Core::init_options(boost::program_options::options_description& /*desc*/) {
}

bool Core::handle_command_line(const boost::program_options::variables_map& vm) {
  m_config_folder = command_line::get_arg(vm, command_line::arg_data_dir);
  return true;
}

uint32_t Core::getCurrentBlockchainHeight() {
  return m_blockchain.getCurrentBlockchainHeight();
}

uint8_t Core::getCurrentBlockMajorVersion() {
  assert(m_blockchain.getCurrentBlockchainHeight() > 0);
  return m_blockchain.getBlockMajorVersionForHeight(m_blockchain.getCurrentBlockchainHeight());
}

uint8_t Core::getBlockMajorVersionForHeight(uint32_t height) {
  assert(m_blockchain.getCurrentBlockchainHeight() > 0);
  return m_blockchain.getBlockMajorVersionForHeight(height);
}

void Core::get_blockchain_top(uint32_t& height, Crypto::Hash& top_id) {
  assert(m_blockchain.getCurrentBlockchainHeight() > 0);
  top_id = m_blockchain.getTailId(height);
}

bool Core::get_blocks(uint32_t start_offset, uint32_t count, std::list<Block>& blocks, std::list<Transaction>& txs) {
  return m_blockchain.getBlocks(start_offset, count, blocks, txs);
}

bool Core::get_blocks(uint32_t start_offset, uint32_t count, std::list<Block>& blocks) {
  return m_blockchain.getBlocks(start_offset, count, blocks);
}  
void Core::getTransactions(const std::vector<Crypto::Hash>& txs_ids, std::list<Transaction>& txs, std::list<Crypto::Hash>& missed_txs, bool checkTxPool) {
  m_blockchain.getTransactions(txs_ids, txs, missed_txs, checkTxPool);
}

bool Core::getTransactionsWithOutputGlobalIndexes(const std::vector<Crypto::Hash>& txs_ids, std::list<Crypto::Hash>& missed_txs, std::vector<std::pair<Transaction, std::vector<uint32_t>>>& txs) {
  return m_blockchain.getTransactionsWithOutputGlobalIndexes(txs_ids, missed_txs, txs);
}

bool Core::getTransaction(const Crypto::Hash& id, Transaction& tx, bool checkTxPool) {
  std::vector<Crypto::Hash> txs_ids;
  std::list<Transaction> txs;
  std::list<Crypto::Hash> missed_txs;

  txs_ids.push_back(id);
  m_blockchain.getTransactions(txs_ids, txs, missed_txs, checkTxPool);

  if (missed_txs.empty() && !txs.empty() && txs.size() == 1) {
    tx = txs.front();
    return true;
  }

  return false;
}

bool Core::getTransactionHeight(const Crypto::Hash &txId, uint32_t& blockHeight) {
  return m_blockchain.getTransactionHeight(txId, blockHeight);
}

bool Core::get_alternative_blocks(std::list<Block>& blocks) {
  return m_blockchain.getAlternativeBlocks(blocks);
}

size_t Core::getAlternativeBlocksCount() {
  return m_blockchain.getAlternativeBlocksCount();
}

bool Core::getblockEntry(uint32_t height, uint64_t& block_cumulative_size, difficulty_type& difficulty, uint64_t& already_generated_coins, uint64_t& reward, uint64_t& transactions_count, uint64_t& timestamp) {
  return m_blockchain.getblockEntry(static_cast<size_t>(height), block_cumulative_size, difficulty, already_generated_coins, reward, transactions_count, timestamp);
}

std::time_t Core::getStartTime() const {
  return start_time;
}

//-----------------------------------------------------------------------------------------------
bool Core::init(const CoreConfig& config, const MinerConfig& minerConfig, bool load_existing) {
  m_config_folder = config.configFolder;
  bool r = m_mempool.init(m_config_folder);
  if (!(r)) { logger(ERROR, BRIGHT_RED) << "Failed to initialize memory pool"; return false; }

  r = m_blockchain.init(m_config_folder, load_existing);
  if (!(r)) { logger(ERROR, BRIGHT_RED) << "Failed to initialize blockchain storage"; return false; }

  r = m_miner->init(minerConfig);
  if (!(r)) { logger(ERROR, BRIGHT_RED) << "Failed to initialize miner"; return false; }

  start_time = std::time(nullptr);

  return load_state_data();
}

bool Core::set_genesis_block(const Block& b) {
  return m_blockchain.resetAndSetGenesisBlock(b);
}

bool Core::load_state_data() {
  // may be some code later
  return true;
}

bool Core::deinit() {
  m_miner->stop();
  m_mempool.deinit();
  m_blockchain.deinit();
  return true;
}

size_t Core::addChain(const std::vector<const IBlock*>& chain) {
  size_t blocksCounter = 0;

  for (const IBlock* block : chain) {
    bool allTransactionsAdded = true;
    for (size_t txNumber = 0; txNumber < block->getTransactionCount(); ++txNumber) {
      const Transaction& tx = block->getTransaction(txNumber);

      Crypto::Hash txHash = NULL_HASH;
      size_t blobSize = 0;
      getObjectHash(tx, txHash, blobSize);
      tx_verification_context tvc = boost::value_initialized<tx_verification_context>();

      if (!handleIncomingTransaction(tx, txHash, blobSize, tvc, true, get_block_height(block->getBlock()))) {
        logger(ERROR, BRIGHT_RED) << "Core::addChain() failed to handle transaction " << txHash << " from block " << blocksCounter << "/" << chain.size();
        allTransactionsAdded = false;
        break;
      }
    }

    if (!allTransactionsAdded) {
      break;
    }

    block_verification_context bvc = boost::value_initialized<block_verification_context>();
    m_blockchain.addNewBlock(block->getBlock(), bvc);
    if (bvc.m_marked_as_orphaned || bvc.m_verification_failed) {
      logger(ERROR, BRIGHT_RED) << "Core::addChain() failed to handle incoming block " << get_block_hash(block->getBlock()) <<
        ", " << blocksCounter << "/" << chain.size();
      break;
    }

    ++blocksCounter;
    m_dispatcher.yield();
  }

  return blocksCounter;
}

bool Core::handle_incoming_tx(const BinaryArray& tx_blob, tx_verification_context& tvc, bool keeped_by_block) { //Deprecated. Should be removed with CryptoNoteProtocolHandler.
  tvc = boost::value_initialized<tx_verification_context>();
  //want to process all transactions sequentially

  if (tx_blob.size() > m_currency.maxTransactionSizeLimit() && getCurrentBlockMajorVersion() >= BLOCK_MAJOR_VERSION_4) {
    logger(INFO) << "WRONG TRANSACTION BLOB, too big size " << tx_blob.size() << ", rejected";
    tvc.m_verification_failed = true;
    return false;
  }

  Crypto::Hash tx_hash = NULL_HASH;
  Crypto::Hash tx_prefixt_hash = NULL_HASH;
  Transaction tx;

  if (!parse_tx_from_blob(tx, tx_hash, tx_prefixt_hash, tx_blob)) {
    logger(INFO) << "WRONG TRANSACTION BLOB, Failed to parse, rejected";
    tvc.m_verification_failed = true;
    return false;
  }
  //std::cout << "!"<< tx.inputs.size() << std::endl;

  Crypto::Hash blockId;
  uint32_t blockHeight;
  bool ok = getBlockContainingTx(tx_hash, blockId, blockHeight);
  if (!ok) blockHeight = this->getCurrentBlockchainHeight();
  return handleIncomingTransaction(tx, tx_hash, tx_blob.size(), tvc, keeped_by_block, blockHeight);
}

bool Core::get_stat_info(core_stat_info& st_inf) {
  st_inf.mining_speed = m_miner->get_speed();
  st_inf.alternative_blocks = m_blockchain.getAlternativeBlocksCount();
  st_inf.blockchain_height = m_blockchain.getCurrentBlockchainHeight();
  st_inf.tx_pool_size = m_mempool.get_transactions_count();
  st_inf.top_block_id_str = Common::podToHex(m_blockchain.getTailId());
  return true;
}

bool Core::check_tx_mixin(const Transaction& tx, const Crypto::Hash& txHash, uint32_t height) {
  size_t inputIndex = 0;
  for (const auto& txin : tx.inputs) {
    assert(inputIndex < tx.signatures.size());
    if (txin.type() == typeid(KeyInput)) {
      uint64_t txMixin = boost::get<KeyInput>(txin).outputIndexes.size();
      if ((height > CryptoNote::parameters::UPGRADE_HEIGHT_V3_1 &&
           txMixin > CryptoNote::parameters::MAX_TX_MIXIN_SIZE)) {
        logger(ERROR) << "Transaction " << Common::podToHex(txHash) << " has too large mixIn count, rejected";
        return false;
      }
      if (getCurrentBlockMajorVersion() >= BLOCK_MAJOR_VERSION_4 && txMixin < m_currency.minMixin() && txMixin != 1) {
        logger(ERROR) << "Transaction " << Common::podToHex(txHash) << " has mixIn count below the required minimum, rejected";
        return false;
      }
      ++inputIndex;
    }
  }
  return true;
}

bool Core::check_tx_fee(const Transaction& tx, const Crypto::Hash& txHash, size_t blobSize, tx_verification_context& tvc, uint32_t height) {
  uint64_t inputs_amount = 0;
  if (!get_inputs_money_amount(tx, inputs_amount)) {
    tvc.m_verification_failed = true;
    return false;
  }

  uint64_t outputs_amount = get_outs_money_amount(tx);

  if (outputs_amount > inputs_amount) {
    logger(DEBUGGING) << "transaction use more money then it has: use " << m_currency.formatAmount(outputs_amount) <<
      ", have " << m_currency.formatAmount(inputs_amount);
    tvc.m_verification_failed = true;
    return false;
  }

  const uint64_t fee = inputs_amount - outputs_amount;
  bool isFusionTransaction = fee == 0 && m_currency.isFusionTransaction(tx, blobSize, height);
  if (!isFusionTransaction && !m_checkpoints.is_in_checkpoint_zone(height)) {
    bool enough = true;

    if ((height <= CryptoNote::parameters::UPGRADE_HEIGHT_V3_1 && fee < CryptoNote::parameters::MINIMUM_FEE_V1) ||
        (height > CryptoNote::parameters::UPGRADE_HEIGHT_V3_1 && height <= CryptoNote::parameters::UPGRADE_HEIGHT_V4 && fee < CryptoNote::parameters::MINIMUM_FEE_V2))
    {
      enough = false;
    }
    else if (height > CryptoNote::parameters::UPGRADE_HEIGHT_V4) {
      uint64_t min = getMinimalFeeForHeight(height);

      if (fee < (min - (min * 20 / 100))) {      
        enough = false;
      }

      if (height > CryptoNote::parameters::UPGRADE_HEIGHT_V4_2) {
        uint64_t extraSize = (uint64_t)tx.extra.size();
        uint64_t feePerByte = m_currency.getFeePerByte(extraSize, min);
        min += feePerByte;
        if (fee < (min - min * 20 / 100)) {
          logger(DEBUGGING) << "Transaction fee is insufficient due to additional data in extra";
          enough = false;
        }
      }
    }

    if (!enough) {
      tvc.m_verification_failed = true;
      tvc.m_tx_fee_too_small = true;
      logger(DEBUGGING) << "The fee for transaction " 
                        << Common::podToHex(txHash) 
                        << " is insufficient and it is not a fusion transaction";
      return false;
    }
  }

  return true;
}

bool Core::check_tx_unmixable(const Transaction& tx, const Crypto::Hash& txHash, uint32_t height) {
  for (const auto& out : tx.outputs) {
    if (height >= CryptoNote::parameters::UPGRADE_HEIGHT_V4_2 && !is_valid_decomposed_amount(out.amount)) {
      logger(ERROR) << "Invalid decomposed output amount " << out.amount << " for tx id= " << Common::podToHex(txHash);
      return false;
    }
  }
  return true;
}

bool Core::check_tx_semantic(const Transaction& tx, const Crypto::Hash& txHash, bool keeped_by_block) {
  if (!tx.inputs.size()) {
    logger(ERROR) << "tx with empty inputs, rejected for tx id= " << Common::podToHex(txHash);
    return false;
  }

  if (tx.inputs.size() != tx.signatures.size()) {
    logger(ERROR) << "tx signatures size doesn't match inputs size, rejected for tx id= " << Common::podToHex(txHash);
    return false;
  }

  for (size_t i = 0; i < tx.inputs.size(); ++i) {
    if (tx.inputs[i].type() == typeid(KeyInput)) {
      if (boost::get<KeyInput>(tx.inputs[i]).outputIndexes.size() != tx.signatures[i].size()) {
        logger(ERROR) << "tx signatures count doesn't match outputIndexes count for input " 
          << i << ", rejected for tx id= " << Common::podToHex(txHash);
        return false;
      }
    }
  }

  if (!check_inputs_types_supported(tx)) {
    logger(ERROR) << "unsupported input types for tx id= " << Common::podToHex(txHash);
    return false;
  }

  std::string errmsg;
  if (!check_outs_valid(tx, &errmsg)) {
    logger(ERROR) << "tx with invalid outputs, rejected for tx id= " << Common::podToHex(txHash) << ": " << errmsg;
    return false;
  }

  if (!check_money_overflow(tx)) {
    logger(ERROR) << "tx have money overflow, rejected for tx id= " << Common::podToHex(txHash);
    return false;
  }

  uint64_t amount_in = 0;
  get_inputs_money_amount(tx, amount_in);
  uint64_t amount_out = get_outs_money_amount(tx);

  if (amount_in < amount_out) {
    logger(ERROR) << "tx with wrong amounts: ins " << amount_in << ", outs " << amount_out << ", rejected for tx id= " << Common::podToHex(txHash);
    return false;
  }

  //check if tx use different key images
  if (!check_tx_inputs_keyimages_diff(tx)) {
    logger(ERROR) << "tx has a few inputs with identical keyimages";
    return false;
  }

  if (!checkMultisignatureInputsDiff(tx)) {
    logger(ERROR) << "tx has a few multisignature inputs with identical output indexes";
    return false;
  }

  return true;
}

bool Core::check_tx_inputs_keyimages_diff(const Transaction& tx) {
  std::unordered_set<Crypto::KeyImage> ki;
  std::set<std::pair<uint64_t, uint32_t>> outputsUsage;
  for (const auto& input : tx.inputs) {
    if (input.type() == typeid(KeyInput)) {
      const KeyInput& in = boost::get<KeyInput>(input);
      if (!ki.insert(in.keyImage).second) {
        logger(ERROR) << "Transaction has identical key images";
          return false;
      }

      if (in.outputIndexes.empty()) {
        logger(ERROR) << "Transaction's input uses empty output";
        return false;
      }

      // outputIndexes are packed here, first is absolute, others are offsets to previous,
      // so first can be zero, others can't
      if (std::find(++std::begin(in.outputIndexes), std::end(in.outputIndexes), 0) != std::end(in.outputIndexes)) {
        logger(ERROR) << "Transaction has identical output indexes";
        return false;
      }
    }
  }
  return true;
}

size_t Core::getBlockchainTotalTransactions() {
  return m_blockchain.getTotalTransactions();
}

//bool Core::get_outs(uint64_t amount, std::list<Crypto::PublicKey>& pkeys)
//{
//  return m_blockchain.get_outs(amount, pkeys);
//}

bool Core::add_new_tx(const Transaction& tx, const Crypto::Hash& tx_hash, size_t blob_size, tx_verification_context& tvc, bool keeped_by_block) {
  //Locking on m_mempool and m_blockchain closes possibility to add tx to memory pool which is already in blockchain 
  std::lock_guard<decltype(m_mempool)> lk(m_mempool);
  LockedBlockchainStorage lbs(m_blockchain);

  if (m_blockchain.haveTransaction(tx_hash)) {
    logger(TRACE) << "tx " << tx_hash << " is already in blockchain";
    return true;
  }

  if (m_mempool.have_tx(tx_hash)) {
    logger(TRACE) << "tx " << tx_hash << " is already in transaction pool";
    return true;
  }

  return m_mempool.add_tx(tx, tx_hash, blob_size, tvc, keeped_by_block);
}

bool Core::get_block_template(Block& b, const AccountPublicAddress& adr, difficulty_type& diffic, uint32_t& height, const BinaryArray& ex_nonce) {
  size_t median_size;
  uint64_t already_generated_coins;

  {
    LockedBlockchainStorage blockchainLock(m_blockchain);
    height = m_blockchain.getCurrentBlockchainHeight();
    diffic = m_blockchain.getDifficultyForNextBlock();
    if (!(diffic)) {
      logger(ERROR, BRIGHT_RED) << "difficulty overhead.";
      return false;
    }

    b = boost::value_initialized<Block>();
    b.majorVersion = m_blockchain.getBlockMajorVersionForHeight(height);

    if (b.majorVersion == BLOCK_MAJOR_VERSION_1) {
      b.minorVersion = m_currency.upgradeHeight(BLOCK_MAJOR_VERSION_2) == UpgradeDetectorBase::UNDEF_HEIGHT ? BLOCK_MINOR_VERSION_1 : BLOCK_MINOR_VERSION_0;
    } else if (b.majorVersion == BLOCK_MAJOR_VERSION_2 || b.majorVersion == BLOCK_MAJOR_VERSION_3) {
      if (m_currency.upgradeHeight(BLOCK_MAJOR_VERSION_3) == UpgradeDetectorBase::UNDEF_HEIGHT) {
        b.minorVersion = b.majorVersion == BLOCK_MAJOR_VERSION_2 ? BLOCK_MINOR_VERSION_1 : BLOCK_MINOR_VERSION_0;
      } else {
        b.minorVersion = BLOCK_MINOR_VERSION_0;
      }

      b.parentBlock.majorVersion = BLOCK_MAJOR_VERSION_1;
      b.parentBlock.majorVersion = BLOCK_MINOR_VERSION_0;
      b.parentBlock.transactionCount = 1;
      TransactionExtraMergeMiningTag mm_tag = boost::value_initialized<decltype(mm_tag)>();

      if (!appendMergeMiningTagToExtra(b.parentBlock.baseTransaction.extra, mm_tag)) {
        logger(ERROR, BRIGHT_RED) << "Failed to append merge mining tag to extra of the parent block miner transaction";
        return false;
      }
    } else if (b.majorVersion == BLOCK_MAJOR_VERSION_4) {
      b.minorVersion = m_currency.upgradeHeight(BLOCK_MAJOR_VERSION_4) == UpgradeDetectorBase::UNDEF_HEIGHT ? BLOCK_MINOR_VERSION_1 : BLOCK_MINOR_VERSION_0;
    } else if (b.majorVersion >= BLOCK_MAJOR_VERSION_5) {
      b.minorVersion = m_currency.upgradeHeight(BLOCK_MAJOR_VERSION_5) == UpgradeDetectorBase::UNDEF_HEIGHT ? BLOCK_MINOR_VERSION_1 : BLOCK_MINOR_VERSION_0;
    }

    b.previousBlockHash = get_tail_id();
    b.timestamp = time(NULL);

    // Don't generate a block template with invalid timestamp
    // Fix by Jagerman
    // https://github.com/graft-project/GraftNetwork/pull/118/commits

    if(height >= m_currency.timestampCheckWindow(b.majorVersion)) {
      std::vector<uint64_t> timestamps;
      for(uint32_t offset = height - static_cast<uint32_t>(m_currency.timestampCheckWindow(b.majorVersion)); offset < height; ++offset) {
        timestamps.push_back(m_blockchain.getBlockTimestamp(offset));
      }
      uint64_t median_ts = Common::medianValue(timestamps);
      if (b.timestamp < median_ts) {
          b.timestamp = median_ts;
      }
    }

    median_size = m_blockchain.getCurrentCumulativeBlocksizeLimit() / 2;
    already_generated_coins = m_blockchain.getCoinsInCirculation();
  }

  size_t txs_size;
  uint64_t fee;
  if (!m_mempool.fill_block_template(b, median_size, m_currency.maxBlockCumulativeSize(height), already_generated_coins, txs_size, fee)) {
    return false;
  }

  /*
     two-phase miner transaction generation: we don't know exact block size until we prepare block, but we don't know reward until we know
     block size, so first miner transaction generated with fake amount of money, and with phase we know think we know expected block size
     */
  //make blocks coin-base tx looks close to real coinbase tx to get truthful blob size
  bool r = m_currency.constructMinerTx(b.majorVersion, height, median_size, already_generated_coins, txs_size, fee, adr, b.baseTransaction, ex_nonce, 14);
  if (!r) { 
    logger(ERROR, BRIGHT_RED) << "Failed to construct miner tx, first chance"; 
    return false; 
  }

  size_t cumulative_size = txs_size + getObjectBinarySize(b.baseTransaction);
  for (size_t try_count = 0; try_count != 10; ++try_count) {
    r = m_currency.constructMinerTx(b.majorVersion, height, median_size, already_generated_coins, cumulative_size, fee, adr, b.baseTransaction, ex_nonce, 14);

    if (!(r)) { logger(ERROR, BRIGHT_RED) << "Failed to construct miner tx, second chance"; return false; }
    size_t coinbase_blob_size = getObjectBinarySize(b.baseTransaction);
    if (coinbase_blob_size > cumulative_size - txs_size) {
      cumulative_size = txs_size + coinbase_blob_size;
      continue;
    }

    if (coinbase_blob_size < cumulative_size - txs_size) {
      size_t delta = cumulative_size - txs_size - coinbase_blob_size;
      b.baseTransaction.extra.insert(b.baseTransaction.extra.end(), delta, 0);
      //here  could be 1 byte difference, because of extra field counter is varint, and it can become from 1-byte len to 2-bytes len.
      if (cumulative_size != txs_size + getObjectBinarySize(b.baseTransaction)) {
        if (!(cumulative_size + 1 == txs_size + getObjectBinarySize(b.baseTransaction))) { logger(ERROR, BRIGHT_RED) << "unexpected case: cumulative_size=" << cumulative_size << " + 1 is not equal txs_cumulative_size=" << txs_size << " + get_object_blobsize(b.baseTransaction)=" << getObjectBinarySize(b.baseTransaction); return false; }
        b.baseTransaction.extra.resize(b.baseTransaction.extra.size() - 1);
        if (cumulative_size != txs_size + getObjectBinarySize(b.baseTransaction)) {
          //fuck, not lucky, -1 makes varint-counter size smaller, in that case we continue to grow with cumulative_size
          logger(TRACE, BRIGHT_RED) <<
            "Miner tx creation have no luck with delta_extra size = " << delta << " and " << delta - 1;
          cumulative_size += delta - 1;
          continue;
        }
        logger(DEBUGGING, BRIGHT_GREEN) <<
          "Setting extra for block: " << b.baseTransaction.extra.size() << ", try_count=" << try_count;
      }
    }
    if (!(cumulative_size == txs_size + getObjectBinarySize(b.baseTransaction))) { logger(ERROR, BRIGHT_RED) << "unexpected case: cumulative_size=" << cumulative_size << " is not equal txs_cumulative_size=" << txs_size << " + get_object_blobsize(b.baseTransaction)=" << getObjectBinarySize(b.baseTransaction); return false; }

    return true;
  }

  logger(ERROR, BRIGHT_RED) <<
    "Failed to create_block_template with " << 10 << " tries";
  return false;
}

std::vector<Crypto::Hash> Core::findBlockchainSupplement(const std::vector<Crypto::Hash>& remoteBlockIds, size_t maxCount,
  uint32_t& totalBlockCount, uint32_t& startBlockIndex) {

  assert(!remoteBlockIds.empty());
  assert(remoteBlockIds.back() == m_blockchain.getBlockIdByHeight(0));

  return m_blockchain.findBlockchainSupplement(remoteBlockIds, maxCount, totalBlockCount, startBlockIndex);
}

void Core::print_blockchain(uint32_t start_index, uint32_t end_index) {
  m_blockchain.print_blockchain(start_index, end_index);
}

void Core::print_blockchain_index() {
  m_blockchain.print_blockchain_index();
}

void Core::print_blockchain_outs(const std::string& file) {
  m_blockchain.print_blockchain_outs(file);
}

bool Core::get_random_outs_for_amounts(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request& req, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response& res) {
  return m_blockchain.getRandomOutsByAmount(req, res);
}

bool Core::get_tx_outputs_gindexs(const Crypto::Hash& tx_id, std::vector<uint32_t>& indexs) {
  return m_blockchain.getTransactionOutputGlobalIndexes(tx_id, indexs);
}

bool Core::getOutByMSigGIndex(uint64_t amount, uint64_t gindex, MultisignatureOutput& out) {
  return m_blockchain.get_out_by_msig_gindex(amount, gindex, out);
}

void Core::pause_mining() {
  m_miner->pause();
}

void Core::update_block_template_and_resume_mining() {
  if (update_miner_block_template()) {
    m_miner->resume();
    logger(DEBUGGING) << "updated block template and resumed mining";
  }
  else {
    logger(ERROR) << "updating block template failed, mining not resumed";
    m_miner->stop();
  }
}

bool Core::handle_block_found(Block& b) {
  block_verification_context bvc = boost::value_initialized<block_verification_context>();
  handle_incoming_block(b, bvc, true, true);

  if (bvc.m_verification_failed) {
    logger(ERROR) << "mined block failed verification";
  }

  return bvc.m_added_to_main_chain;
}

void Core::on_synchronized() {
  m_miner->on_synchronized();
}

bool Core::getPoolChanges(const Crypto::Hash& tailBlockId, const std::vector<Crypto::Hash>& knownTxsIds,
                          std::vector<Transaction>& addedTxs, std::vector<Crypto::Hash>& deletedTxsIds) {
  getPoolChanges(knownTxsIds, addedTxs, deletedTxsIds);
  return tailBlockId == m_blockchain.getTailId();
}

bool Core::getPoolChangesLite(const Crypto::Hash& tailBlockId, const std::vector<Crypto::Hash>& knownTxsIds,
        std::vector<TransactionPrefixInfo>& addedTxs, std::vector<Crypto::Hash>& deletedTxsIds) {
  std::vector<Transaction> added;
  bool returnStatus = getPoolChanges(tailBlockId, knownTxsIds, added, deletedTxsIds);

  for (const auto& tx: added) {
    TransactionPrefixInfo tpi;
    tpi.txPrefix = tx;
    tpi.txHash = getObjectHash(tx);

    addedTxs.push_back(std::move(tpi));
  }

  return returnStatus;
}

void Core::getPoolChanges(const std::vector<Crypto::Hash>& knownTxsIds, std::vector<Transaction>& addedTxs,
                          std::vector<Crypto::Hash>& deletedTxsIds) {
  std::vector<Crypto::Hash> addedTxsIds;
  auto guard = m_mempool.obtainGuard();
  m_mempool.get_difference(knownTxsIds, addedTxsIds, deletedTxsIds);
  std::vector<Crypto::Hash> misses;
  m_mempool.getTransactions(addedTxsIds, addedTxs, misses);
  assert(misses.empty());
}

bool Core::handle_incoming_block_blob(const BinaryArray& block_blob, block_verification_context& bvc, bool control_miner, bool relay_block) {
  if (block_blob.size() > m_currency.maxBlockBlobSize()) {
    logger(INFO) << "WRONG BLOCK BLOB, too big size " << block_blob.size() << ", rejected";
    bvc.m_verification_failed = true;
    return false;
  }

  Block b;
  if (!fromBinaryArray(b, block_blob)) {
    logger(INFO) << "Failed to parse and validate new block";
    bvc.m_verification_failed = true;
    return false;
  }

  return handle_incoming_block(b, bvc, control_miner, relay_block);
}

bool Core::handle_incoming_block(const Block& b, block_verification_context& bvc, bool control_miner, bool relay_block) {
  if (control_miner) {
    pause_mining();
  }

  m_blockchain.addNewBlock(b, bvc);

  if (control_miner) {
    update_block_template_and_resume_mining();
  }

  if (relay_block && bvc.m_added_to_main_chain) {
    std::list<Crypto::Hash> missed_txs;
    std::list<Transaction> txs;
    m_blockchain.getTransactions(b.transactionHashes, txs, missed_txs);
    if (!missed_txs.empty() && getBlockIdByHeight(get_block_height(b)) != get_block_hash(b)) {
      logger(INFO) << "Block added, but it seems that reorganize just happened after that, do not relay this block";
    } else {
      if (!(txs.size() == b.transactionHashes.size() && missed_txs.empty())) {
        logger(ERROR, BRIGHT_RED) << "can't find some transactions in found block:" <<
          get_block_hash(b) << " txs.size()=" << txs.size() << ", b.transactionHashes.size()=" << b.transactionHashes.size() << ", missed_txs.size()" << missed_txs.size(); return false;
      }

      NOTIFY_NEW_BLOCK::request arg;
      arg.hop = 0;
      arg.current_blockchain_height = m_blockchain.getCurrentBlockchainHeight();
      BinaryArray blockBa;
      bool r = toBinaryArray(b, blockBa);
      if (!(r)) { logger(ERROR, BRIGHT_RED) << "failed to serialize block"; return false; }
      arg.b.block = asString(blockBa);
      for (auto& tx : txs) {
        arg.b.txs.push_back(asString(toBinaryArray(tx)));
      }

      m_pprotocol->relay_block(arg);
    }
  }

  return true;
}

Crypto::Hash Core::get_tail_id() {
  return m_blockchain.getTailId();
}

size_t Core::getPoolTransactionsCount() {
  return m_mempool.get_transactions_count();
}

bool Core::have_block(const Crypto::Hash& id) {
  return m_blockchain.haveBlock(id);
}

bool Core::haveTransaction(const Crypto::Hash& id) {
  if (m_blockchain.haveTransaction(id)) {
    return true;
  }

  if (m_mempool.have_tx(id)) {
    return true;
  }

  return false;
}

bool Core::parse_tx_from_blob(Transaction& tx, Crypto::Hash& tx_hash, Crypto::Hash& tx_prefix_hash, const BinaryArray& blob) {
  return parseAndValidateTransactionFromBinaryArray(blob, tx, tx_hash, tx_prefix_hash);
}

bool Core::check_tx_syntax(const Transaction& tx, const Crypto::Hash& tx_hash) {
  return true;
}

std::vector<Transaction> Core::getPoolTransactions() {
  std::list<Transaction> txs;
  m_mempool.get_transactions(txs);

  std::vector<Transaction> result;
  for (auto& tx : txs) {
    result.emplace_back(std::move(tx));
  }
  return result;
}

bool Core::getPoolTransaction(const Crypto::Hash& tx_hash, Transaction& transaction) {
  if (!m_mempool.have_tx(tx_hash)) {
    return false;
  }

  return m_mempool.getTransaction(tx_hash, transaction);
}

std::list<CryptoNote::tx_memory_pool::TransactionDetails> Core::getMemoryPool() const {
  //std::list<CryptoNote::tx_memory_pool::TransactionDetails> txs;
  //m_mempool.getMemoryPool(txs);
  //return txs;
	return m_mempool.getMemoryPool();
}

std::vector<Crypto::Hash> Core::buildSparseChain() {
  assert(m_blockchain.getCurrentBlockchainHeight() != 0);
  return m_blockchain.buildSparseChain();
}

std::vector<Crypto::Hash> Core::buildSparseChain(const Crypto::Hash& startBlockId) {
  LockedBlockchainStorage lbs(m_blockchain);
  assert(m_blockchain.haveBlock(startBlockId));
  return m_blockchain.buildSparseChain(startBlockId);
}

bool Core::handle_get_objects(NOTIFY_REQUEST_GET_OBJECTS::request& arg, NOTIFY_RESPONSE_GET_OBJECTS::request& rsp) { //Deprecated. Should be removed with CryptoNoteProtocolHandler.
  return m_blockchain.handleGetObjects(arg, rsp);
}

Crypto::Hash Core::getBlockIdByHeight(uint32_t height) {
  LockedBlockchainStorage lbs(m_blockchain);
  if (height < m_blockchain.getCurrentBlockchainHeight()) {
    return m_blockchain.getBlockIdByHeight(height);
  } else {
    return NULL_HASH;
  }
}

bool Core::getBlockByHash(const Crypto::Hash &h, Block &blk) {
  return m_blockchain.getBlockByHash(h, blk);
}

bool Core::getBlockHeight(const Crypto::Hash& blockId, uint32_t& blockHeight) {
  return m_blockchain.getBlockHeight(blockId, blockHeight);
}

//void Core::get_all_known_block_ids(std::list<Crypto::Hash> &main, std::list<Crypto::Hash> &alt, std::list<Crypto::Hash> &invalid) {
//  m_blockchain.get_all_known_block_ids(main, alt, invalid);
//}

std::string Core::print_pool(bool short_format) {
  return m_mempool.print_pool(short_format);
}

bool Core::update_miner_block_template() {
  return m_miner->on_block_chain_update();
}

bool Core::on_idle() {
  if (!m_starter_message_showed) {
    std::cout << ENDL << "**********************************************************************" << ENDL
      << "The daemon will start synchronizing with the network. It may take up to several hours." << ENDL
      << ENDL
      << "You can set the level of process detailization* through \"set_log <level>\" command*, where <level> is between 0 (no details) and 4 (very verbose)." << ENDL
      << ENDL
      << "Use \"help\" command to see the list of available commands." << ENDL
      << ENDL
      << "Note: in case you need to interrupt the process, use \"exit\" command. Otherwise, the current progress won't be saved." << ENDL
      << "**********************************************************************" << ENDL;
    m_starter_message_showed = true;
  }

  m_miner->on_idle();
  m_mempool.on_idle();
  return true;
}

bool Core::addObserver(ICoreObserver* observer) {
  return m_observerManager.add(observer);
}

bool Core::removeObserver(ICoreObserver* observer) {
  return m_observerManager.remove(observer);
}

void Core::blockchainUpdated() {
  m_observerManager.notify(&ICoreObserver::blockchainUpdated);
}

void Core::txDeletedFromPool() {
  poolUpdated();
}

void Core::poolUpdated() {
  m_observerManager.notify(&ICoreObserver::poolUpdated);
}

bool Core::queryBlocks(const std::vector<Crypto::Hash>& knownBlockIds, uint64_t timestamp,
  uint32_t& resStartHeight, uint32_t& resCurrentHeight, uint32_t& resFullOffset, std::vector<BlockFullInfo>& entries) {

  LockedBlockchainStorage lbs(m_blockchain);

  uint32_t currentHeight = lbs->getCurrentBlockchainHeight();
  uint32_t startOffset = 0;
  uint32_t startFullOffset = 0;

  if (!findStartAndFullOffsets(knownBlockIds, timestamp, startOffset, startFullOffset)) {
    return false;
  }

  resFullOffset = startFullOffset;
  std::vector<Crypto::Hash> blockIds = findIdsForShortBlocks(startOffset, startFullOffset);
  entries.reserve(blockIds.size());

  for (const auto& id : blockIds) {
    entries.push_back(BlockFullInfo());
    entries.back().block_id = id;
  }

  resCurrentHeight = currentHeight;
  resStartHeight = startOffset;

  uint32_t blocksLeft = static_cast<uint32_t>(std::min(BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT - entries.size(), size_t(BLOCKS_SYNCHRONIZING_DEFAULT_COUNT)));

  if (blocksLeft == 0) {
    return true;
  }

  std::list<Block> blocks;
  lbs->getBlocks(startFullOffset, blocksLeft, blocks);

  for (auto& b : blocks) {
    BlockFullInfo item;

    item.block_id = get_block_hash(b);

    if (b.timestamp >= timestamp) {
      // query transactions
      std::list<Transaction> txs;
      std::list<Crypto::Hash> missedTxs;
      lbs->getTransactions(b.transactionHashes, txs, missedTxs);

      // fill data
      block_complete_entry& completeEntry = item;
      completeEntry.block = asString(toBinaryArray(b));
      for (auto& tx : txs) {
        completeEntry.txs.push_back(asString(toBinaryArray(tx)));
      }
    }

    entries.push_back(std::move(item));
  }

  return true;
}

bool Core::findStartAndFullOffsets(const std::vector<Crypto::Hash>& knownBlockIds, uint64_t timestamp, uint32_t& startOffset, uint32_t& startFullOffset) {
  LockedBlockchainStorage lbs(m_blockchain);

  if (knownBlockIds.empty()) {
    logger(ERROR, BRIGHT_RED) << "knownBlockIds is empty";
    return false;
  }

  if (knownBlockIds.back() != m_blockchain.getBlockIdByHeight(0)) {
    logger(ERROR, BRIGHT_RED) << "knownBlockIds doesn't end with genesis block hash: " << knownBlockIds.back();
    return false;
  }

  startOffset = lbs->findBlockchainSupplement(knownBlockIds);
  if (!lbs->getLowerBound(timestamp, startOffset, startFullOffset)) {
    startFullOffset = startOffset;
  }

  return true;
}

std::vector<Crypto::Hash> Core::findIdsForShortBlocks(uint32_t startOffset, uint32_t startFullOffset) {
  assert(startOffset <= startFullOffset);

  LockedBlockchainStorage lbs(m_blockchain);

  std::vector<Crypto::Hash> result;
  if (startOffset < startFullOffset) {
    result = lbs->getBlockIds(startOffset, std::min(static_cast<uint32_t>(BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT), startFullOffset - startOffset));
  }

  return result;
}

bool Core::queryBlocksLite(const std::vector<Crypto::Hash>& knownBlockIds, uint64_t timestamp, uint32_t& resStartHeight,
  uint32_t& resCurrentHeight, uint32_t& resFullOffset, std::vector<BlockShortInfo>& entries) {
  LockedBlockchainStorage lbs(m_blockchain);

  resCurrentHeight = lbs->getCurrentBlockchainHeight();
  resStartHeight = 0;
  resFullOffset = 0;

  if (!findStartAndFullOffsets(knownBlockIds, timestamp, resStartHeight, resFullOffset)) {
    return false;
  }

  std::vector<Crypto::Hash> blockIds = findIdsForShortBlocks(resStartHeight, resFullOffset);
  entries.reserve(blockIds.size());

  for (const auto& id : blockIds) {
    entries.push_back(BlockShortInfo());
    entries.back().blockId = id;
  }

  uint32_t blocksLeft = static_cast<uint32_t>(std::min(BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT - entries.size(), size_t(BLOCKS_SYNCHRONIZING_DEFAULT_COUNT)));

  if (blocksLeft == 0) {
    return true;
  }

  std::list<Block> blocks;
  lbs->getBlocks(resFullOffset, blocksLeft, blocks);

  for (auto& b : blocks) {
    BlockShortInfo item;

    item.blockId = get_block_hash(b);

    if (b.timestamp >= timestamp) {
      std::list<Transaction> txs;
      std::list<Crypto::Hash> missedTxs;
      lbs->getTransactions(b.transactionHashes, txs, missedTxs);

      item.block = asString(toBinaryArray(b));

      for (const auto& tx: txs) {
        TransactionPrefixInfo info;
        info.txPrefix = tx;
        info.txHash = getObjectHash(tx);

        item.txPrefixes.push_back(std::move(info));
      }
    }

    entries.push_back(std::move(item));
  }

  return true;
}

bool Core::getBackwardBlocksSizes(uint32_t fromHeight, std::vector<size_t>& sizes, size_t count) {
  return m_blockchain.getBackwardBlocksSize(fromHeight, sizes, count);
}

bool Core::getBlockSize(const Crypto::Hash& hash, size_t& size) {
  return m_blockchain.getBlockSize(hash, size);
}

bool Core::getAlreadyGeneratedCoins(const Crypto::Hash& hash, uint64_t& generatedCoins) {
  return m_blockchain.getAlreadyGeneratedCoins(hash, generatedCoins);
}

bool Core::getBlockReward(uint8_t blockMajorVersion, size_t medianSize, size_t currentBlockSize, uint64_t alreadyGeneratedCoins, uint64_t fee,
                          uint64_t& reward, int64_t& emissionChange) {
  return m_currency.getBlockReward(blockMajorVersion, medianSize, currentBlockSize, alreadyGeneratedCoins, fee, reward, emissionChange);
}

bool Core::scanOutputkeysForIndices(const KeyInput& txInToKey, std::list<std::pair<Crypto::Hash, size_t>>& outputReferences) {
  struct outputs_visitor
  {
    std::list<std::pair<Crypto::Hash, size_t>>& m_resultsCollector;
    outputs_visitor(std::list<std::pair<Crypto::Hash, size_t>>& resultsCollector):m_resultsCollector(resultsCollector){}
    bool handle_output(const Transaction& tx, const TransactionOutput& out, size_t transactionOutputIndex)
    {
      m_resultsCollector.push_back(std::make_pair(getObjectHash(tx), transactionOutputIndex));
      return true;
    }
  };
    
  outputs_visitor vi(outputReferences);
    
  return m_blockchain.scanOutputKeysForIndexes(txInToKey, vi);
}

bool Core::getBlockTimestamp(uint32_t height, uint64_t& timestamp) {
  if (height > getCurrentBlockchainHeight()) {
    return false;
  }

  timestamp = m_blockchain.getBlockTimestamp(height);
  return true;
}

bool Core::getBlockDifficulty(uint32_t height, difficulty_type& difficulty) {
  difficulty = m_blockchain.blockDifficulty(height);
  return true;
}

bool Core::getBlockCumulativeDifficulty(uint32_t height, difficulty_type& difficulty) {
  difficulty = m_blockchain.blockCumulativeDifficulty(height);
  return true;
}

bool Core::getBlockContainingTx(const Crypto::Hash& txId, Crypto::Hash& blockId, uint32_t& blockHeight) {
  return m_blockchain.getBlockContainingTransaction(txId, blockId, blockHeight);
}

bool Core::getMultisigOutputReference(const MultisignatureInput& txInMultisig, std::pair<Crypto::Hash, size_t>& outputReference) {
  return m_blockchain.getMultisigOutputReference(txInMultisig, outputReference);
}

bool Core::getGeneratedTransactionsNumber(uint32_t height, uint64_t& generatedTransactions) {
  return m_blockchain.getGeneratedTransactionsNumber(height, generatedTransactions);
}

bool Core::getOrphanBlocksByHeight(uint32_t height, std::vector<Block>& blocks) {
  std::vector<Crypto::Hash> blockHashes;
  if (!m_blockchain.getOrphanBlockIdsByHeight(height, blockHashes)) {
    return false;
  }
  for (const Crypto::Hash& hash : blockHashes) {
    Block blk;
    if (!getBlockByHash(hash, blk)) {
      return false;
    }
    blocks.push_back(std::move(blk));
  }
  return true;
}

bool Core::getBlocksByTimestamp(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<Block>& blocks, uint32_t& blocksNumberWithinTimestamps) {
  std::vector<Crypto::Hash> blockHashes;
  if (!m_blockchain.getBlockIdsByTimestamp(timestampBegin, timestampEnd, blocksNumberLimit, blockHashes, blocksNumberWithinTimestamps)) {
    return false;
  }
  for (const Crypto::Hash& hash : blockHashes) {
    Block blk;
    if (!getBlockByHash(hash, blk)) {
      return false;
    }
    blocks.push_back(std::move(blk));
  }
  return true;
}

bool Core::getPoolTransactionsByTimestamp(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<Transaction>& transactions, uint64_t& transactionsNumberWithinTimestamps) {
  std::vector<Crypto::Hash> poolTransactionHashes;
  if (!m_mempool.getTransactionIdsByTimestamp(timestampBegin, timestampEnd, transactionsNumberLimit, poolTransactionHashes, transactionsNumberWithinTimestamps)) {
    return false;
  }
  std::list<Transaction> txs;
  std::list<Crypto::Hash> missed_txs;

  getTransactions(poolTransactionHashes, txs, missed_txs, true);
  if (missed_txs.size() > 0) {
    return false;
  }

  transactions.insert(transactions.end(), txs.begin(), txs.end());
  return true;
}

bool Core::getTransactionsByPaymentId(const Crypto::Hash& paymentId, std::vector<Transaction>& transactions) {
  std::vector<Crypto::Hash> blockchainTransactionHashes;
  m_blockchain.getTransactionIdsByPaymentId(paymentId, blockchainTransactionHashes);

  std::vector<Crypto::Hash> poolTransactionHashes;
  m_mempool.getTransactionIdsByPaymentId(paymentId, poolTransactionHashes);

  std::list<Transaction> txs;
  std::list<Crypto::Hash> missed_txs;

  if (!poolTransactionHashes.empty()) {
    blockchainTransactionHashes.insert(blockchainTransactionHashes.end(), poolTransactionHashes.begin(), poolTransactionHashes.end());
  }

  if (blockchainTransactionHashes.empty()) {
    return false;
  }

  getTransactions(blockchainTransactionHashes, txs, missed_txs, true);
    if (missed_txs.size() > 0) {
      return false;
  }

  transactions.insert(transactions.end(), txs.begin(), txs.end());
  return true;
}

std::vector<Crypto::Hash> Core::getTransactionHashesByPaymentId(const Crypto::Hash& paymentId) {
  logger(DEBUGGING) << "getTransactionHashesByPaymentId request with paymentId " << paymentId;

  std::vector<Crypto::Hash> blockchainTransactionHashes;
  m_blockchain.getTransactionIdsByPaymentId(paymentId, blockchainTransactionHashes);

  std::vector<Crypto::Hash> poolTransactionHashes;
  m_mempool.getTransactionIdsByPaymentId(paymentId, poolTransactionHashes);

  blockchainTransactionHashes.reserve(blockchainTransactionHashes.size() + poolTransactionHashes.size());
  std::move(poolTransactionHashes.begin(), poolTransactionHashes.end(), std::back_inserter(blockchainTransactionHashes));

  return blockchainTransactionHashes;
}

difficulty_type Core::getAvgDifficulty(uint32_t height, size_t window) {
  return m_blockchain.getAvgDifficulty(height, window);
}

difficulty_type Core::getAvgDifficulty(uint32_t height) {
  return m_blockchain.getAvgDifficulty(height);
}

uint64_t Core::getMinimalFee() {
  return getMinimalFeeForHeight(getCurrentBlockchainHeight() - 1);
}

uint64_t Core::getMinimalFeeForHeight(uint32_t height) {
	return m_blockchain.getMinimalFee(height);
}

std::error_code Core::executeLocked(const std::function<std::error_code()>& func) {
  std::lock_guard<decltype(m_mempool)> lk(m_mempool);
  LockedBlockchainStorage lbs(m_blockchain);

  return func();
}

uint64_t Core::getNextBlockDifficulty() {
  return m_blockchain.getDifficultyForNextBlock();
}

uint64_t Core::getTotalGeneratedAmount() {
  return m_blockchain.getCoinsInCirculation();
}

uint8_t Core::getBlockMajorVersionForHeight(uint32_t height) const {
  return m_blockchain.getBlockMajorVersionForHeight(height);
}

bool Core::getPaymentId(const Transaction& transaction, Crypto::Hash& paymentId) {
  std::vector<TransactionExtraField> txExtraFields;
  parseTransactionExtra(transaction.extra, txExtraFields);
  TransactionExtraNonce extraNonce;
  if (!findTransactionExtraFieldByType(txExtraFields, extraNonce)) {
    return false;
  }
  return getPaymentIdFromTransactionExtraNonce(extraNonce.nonce, paymentId);
}

bool Core::handleIncomingTransaction(const Transaction& tx, const Crypto::Hash& txHash, size_t blobSize, tx_verification_context& tvc, bool keptByBlock, uint32_t height) {
  if (!check_tx_syntax(tx, txHash)) {
    logger(INFO) << "WRONG TRANSACTION BLOB, Failed to check tx " << txHash << " syntax, rejected";
    tvc.m_verification_failed = true;
    return false;
  }

  // is in checkpoint zone
  if (!m_blockchain.isInCheckpointZone(getCurrentBlockchainHeight())) {
    if (blobSize > m_currency.maxTransactionSizeLimit() && getCurrentBlockMajorVersion() >= BLOCK_MAJOR_VERSION_4) {
      logger(INFO) << "Transaction verification failed: too big size " << blobSize << " of transaction " << txHash << ", rejected";
      tvc.m_verification_failed = true;
      return false;
    }
  
    if (!check_tx_fee(tx, txHash, blobSize, tvc, height)) {
      tvc.m_verification_failed = true;
      return false;
    }

    if (!check_tx_mixin(tx, txHash, height)) {
      logger(INFO) << "Transaction verification failed: mixin count for transaction " << txHash << " is too large, rejected";
      tvc.m_verification_failed = true;
      return false;
    }

    if (!check_tx_unmixable(tx, txHash, height)) {
      logger(ERROR) << "Transaction verification failed: unmixable output for transaction " << txHash << ", rejected";
      tvc.m_verification_failed = true;
      return false;
    }

  }

  if (!check_tx_semantic(tx, txHash, keptByBlock)) {
    logger(INFO) << "WRONG TRANSACTION BLOB, Failed to check tx " << txHash << " semantic, rejected";
    tvc.m_verification_failed = true;
    return false;
  }

  bool r = add_new_tx(tx, txHash, blobSize, tvc, keptByBlock);
  if (tvc.m_verification_failed) {
    if (!tvc.m_tx_fee_too_small) {
      logger(ERROR) << "Transaction verification failed: " << txHash;
    } else {
      logger(INFO) << "Transaction verification failed: " << txHash;
    }
  } else if (tvc.m_verifivation_impossible) {
    logger(ERROR) << "Transaction verification impossible: " << txHash;
  }

  if (tvc.m_added_to_pool) {
    logger(DEBUGGING) << "Transaction added to pool: " << txHash;
    poolUpdated();
  }

  return r;
}

std::unique_ptr<IBlock> Core::getBlock(const Crypto::Hash& blockId) {
  std::lock_guard<decltype(m_mempool)> lk(m_mempool);
  LockedBlockchainStorage lbs(m_blockchain);

  std::unique_ptr<BlockWithTransactions> blockPtr(new BlockWithTransactions());
  if (!lbs->getBlockByHash(blockId, blockPtr->block)) {
    logger(DEBUGGING) << "Can't find block: " << blockId;
    return std::unique_ptr<BlockWithTransactions>(nullptr);
  }

  blockPtr->transactions.reserve(blockPtr->block.transactionHashes.size());
  std::vector<Crypto::Hash> missedTxs;
  lbs->getTransactions(blockPtr->block.transactionHashes, blockPtr->transactions, missedTxs, true);
  assert(missedTxs.empty() || !lbs->isBlockInMainChain(blockId)); //if can't find transaction for blockchain block -> error

  if (!missedTxs.empty()) {
    logger(DEBUGGING) << "Can't find transactions for block: " << blockId;
    return std::unique_ptr<BlockWithTransactions>(nullptr);
  }

  return blockPtr;
}

bool Core::getMixin(const Transaction& transaction, uint64_t& mixin) {
  mixin = 0;
  for (const TransactionInput& txin : transaction.inputs) {
    if (txin.type() != typeid(KeyInput)) {
      continue;
    }
    uint64_t currentMixin = boost::get<KeyInput>(txin).outputIndexes.size();
    if (currentMixin > mixin) {
      mixin = currentMixin;
    }
  }
  return true;
}

bool Core::is_key_image_spent(const Crypto::KeyImage& key_im) {
  return m_blockchain.have_tx_keyimg_as_spent(key_im);
}

bool Core::is_key_image_spent(const Crypto::KeyImage& key_im, uint32_t height) {
  return m_blockchain.checkIfSpent(key_im, height);
}

bool Core::is_tx_spendtime_unlocked(uint64_t unlock_time) {
  return m_blockchain.is_tx_spendtime_unlocked(unlock_time);
}

bool Core::is_tx_spendtime_unlocked(uint64_t unlock_time, uint32_t height) {
  return m_blockchain.is_tx_spendtime_unlocked(unlock_time, height);
}

bool Core::isInCheckpointZone(uint32_t height) const {
  return m_checkpoints.is_in_checkpoint_zone(height);
}

bool Core::addMessageQueue(MessageQueue<BlockchainMessage>& messageQueue) {
  return m_blockchain.addMessageQueue(messageQueue);
}

bool Core::removeMessageQueue(MessageQueue<BlockchainMessage>& messageQueue) {
  return m_blockchain.removeMessageQueue(messageQueue);
}

void Core::rollbackBlockchain(const uint32_t height) {
  logger(INFO, BRIGHT_YELLOW) << "Rewinding blockchain to height: " << height;
  m_blockchain.rollbackBlockchainTo(height);
}

bool Core::saveBlockchain() {
  return m_blockchain.storeCache();
}

}
