// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016, The Forknote developers
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

#include "TransactionPool.h"

#include <algorithm>
#include <ctime>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include <boost/filesystem.hpp>

#include "Common/int-util.h"
#include "Common/Util.h"
#include "crypto/hash.h"

#include "Serialization/SerializationTools.h"
#include "Serialization/BinarySerializationTools.h"

#include "CryptoNoteFormatUtils.h"
#include "CryptoNoteTools.h"
#include "CryptoNoteConfig.h"

using namespace Logging;

#undef ERROR

namespace CryptoNote {

  //---------------------------------------------------------------------------------
  // BlockTemplate
  //---------------------------------------------------------------------------------
  class BlockTemplate {
  public:

    bool addTransaction(const Crypto::Hash& txid, const Transaction& tx) {
      if (!canAdd(tx))
        return false;

      for (const auto& in : tx.inputs) {
        if (in.type() == typeid(KeyInput)) {
          auto r = m_keyImages.insert(boost::get<KeyInput>(in).keyImage);
          (void)r; //just to make compiler to shut up
          assert(r.second);
        } else if (in.type() == typeid(MultisignatureInput)) {
          const auto& msig = boost::get<MultisignatureInput>(in);
          auto r = m_usedOutputs.insert(std::make_pair(msig.amount, msig.outputIndex));
          (void)r; //just to make compiler to shut up
          assert(r.second);
        }
      }

      m_txHashes.push_back(txid);
      return true;
    }

    const std::vector<Crypto::Hash>& getTransactions() const {
      return m_txHashes;
    }

  private:

    bool canAdd(const Transaction& tx) {
      for (const auto& in : tx.inputs) {
        if (in.type() == typeid(KeyInput)) {
          if (m_keyImages.count(boost::get<KeyInput>(in).keyImage)) {
            return false;
          }
        } else if (in.type() == typeid(MultisignatureInput)) {
          const auto& msig = boost::get<MultisignatureInput>(in);
          if (m_usedOutputs.count(std::make_pair(msig.amount, msig.outputIndex))) {
            return false;
          }
        }
      }
      return true;
    }
    
    std::unordered_set<Crypto::KeyImage> m_keyImages;
    std::set<std::pair<uint64_t, uint64_t>> m_usedOutputs;
    std::vector<Crypto::Hash> m_txHashes;
  };

  using CryptoNote::BlockInfo;

  std::unordered_set<Crypto::Hash> m_validated_transactions;

  //---------------------------------------------------------------------------------
  tx_memory_pool::tx_memory_pool(
    const CryptoNote::Currency& currency,
    CryptoNote::ITransactionValidator& validator, 
    CryptoNote::ICore& core,
    CryptoNote::ITimeProvider& timeProvider,
    Logging::ILogger& log,
    bool blockchainIndexesEnabled) :
    m_currency(currency),
    m_validator(validator),
    m_core(core),
    m_timeProvider(timeProvider), 
    m_txCheckInterval(60, timeProvider),
    m_fee_index(boost::get<1>(m_transactions)),
    logger(log, "txpool"),
    m_paymentIdIndex(blockchainIndexesEnabled),
    m_timestampIndex(blockchainIndexesEnabled) {
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::add_tx(const Transaction &tx, /*const Crypto::Hash& tx_prefix_hash,*/ const Crypto::Hash &id, size_t blobSize, tx_verification_context& tvc, bool keptByBlock) {
    if (!check_inputs_types_supported(tx)) {
      tvc.m_verification_failed = true;
      return false;
    }

    uint64_t inputs_amount = 0;
    if (!get_inputs_money_amount(tx, inputs_amount)) {
      tvc.m_verification_failed = true;
      return false;
    }

    uint64_t outputs_amount = get_outs_money_amount(tx);

    if (outputs_amount > inputs_amount) {
      logger(INFO) << "transaction use more money then it has: use " << m_currency.formatAmount(outputs_amount) <<
        ", have " << m_currency.formatAmount(inputs_amount);
      tvc.m_verification_failed = true;
      return false;
    }

    const uint64_t fee = inputs_amount - outputs_amount;
    bool isFusionTransaction = fee == 0 && m_currency.isFusionTransaction(tx, blobSize, m_core.getCurrentBlockchainHeight());

    //check key images for transaction if it is not kept by block
    if (!keptByBlock) {
      std::lock_guard<std::recursive_mutex> lock(m_transactions_lock);
      if (haveSpentInputs(tx)) {
        logger(INFO) << "Transaction with id= " << id << " used already spent inputs";
        tvc.m_verification_failed = true;
        return false;
      }
    }

    BlockInfo maxUsedBlock;

    // check inputs
    bool inputsValid = m_validator.checkTransactionInputs(tx, maxUsedBlock);

    if (!inputsValid) {
      if (!keptByBlock) {
        logger(INFO) << "tx used wrong inputs, rejected";
        tvc.m_verification_failed = true;
        return false;
      }

      maxUsedBlock.clear();
      tvc.m_verifivation_impossible = true;
    }

    if (!keptByBlock) {
      bool sizeValid = m_validator.checkTransactionSize(blobSize);
      if (!sizeValid) {
        logger(INFO) << "tx too big, rejected";
        tvc.m_verification_failed = true;
        return false;
      }
    }

    std::lock_guard<std::recursive_mutex> lock(m_transactions_lock);

    if (!keptByBlock && m_recentlyDeletedTransactions.find(id) != m_recentlyDeletedTransactions.end()) {
      logger(INFO) << "Trying to add recently deleted transaction. Ignore: " << id;
      tvc.m_verification_failed = false;
      tvc.m_should_be_relayed = false;
      tvc.m_added_to_pool = false;
      return true;
    }

    // add to pool
    {
      TransactionDetails txd;

      txd.id = id;
      txd.blobSize = blobSize;
      txd.tx = tx;
      txd.fee = fee;
      txd.keptByBlock = keptByBlock;
      txd.receiveTime = m_timeProvider.now();

      txd.maxUsedBlock = maxUsedBlock;
      txd.lastFailedBlock.clear();

      auto txd_p = m_transactions.insert(txd);
      if (!(txd_p.second)) {
        logger(ERROR, BRIGHT_RED) << "transaction already exists at inserting in memory pool";
        return false;
      }
      m_paymentIdIndex.add(tx);
      m_timestampIndex.add(txd.receiveTime, txd.id);
    }

    tvc.m_added_to_pool = true;
    tvc.m_should_be_relayed = inputsValid && (fee > 0 || isFusionTransaction);
    tvc.m_verification_failed = true;

    if (!addTransactionInputs(id, tx, keptByBlock))
      return false;

    tvc.m_verification_failed = false;
    //succeed
    return true;
  }

  //---------------------------------------------------------------------------------
  bool tx_memory_pool::add_tx(const Transaction &tx, tx_verification_context& tvc, bool keeped_by_block) {
    Crypto::Hash h = NULL_HASH;
    size_t blobSize = 0;
    getObjectHash(tx, h, blobSize);
    return add_tx(tx, h, blobSize, tvc, keeped_by_block);
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::take_tx(const Crypto::Hash &id, Transaction &tx, size_t& blobSize, uint64_t& fee) {
    std::lock_guard<std::recursive_mutex> lock(m_transactions_lock);
    auto it = m_transactions.find(id);
    if (it == m_transactions.end()) {
      return false;
    }

    auto& txd = *it;

    tx = txd.tx;
    blobSize = txd.blobSize;
    fee = txd.fee;

    removeTransaction(it);
    return true;
  }

  //---------------------------------------------------------------------------------
  bool tx_memory_pool::getTransaction(const Crypto::Hash& id, Transaction& tx) {
    std::lock_guard<std::recursive_mutex> lock(m_transactions_lock);
    auto it = m_transactions.find(id);
    if (it == m_transactions.end()) {
      return false;
    }

    auto& txd = *it;
    tx = txd.tx;

    return true;
  }

  //---------------------------------------------------------------------------------
  size_t tx_memory_pool::get_transactions_count() const {
    std::lock_guard<std::recursive_mutex> lock(m_transactions_lock);
    return m_transactions.size();
  }
  //---------------------------------------------------------------------------------
  void tx_memory_pool::get_transactions(std::list<Transaction>& txs) const {
    std::lock_guard<std::recursive_mutex> lock(m_transactions_lock);
    for (const auto& tx_vt : m_transactions) {
      txs.push_back(tx_vt.tx);
    }
  }

  //---------------------------------------------------------------------------------
  void tx_memory_pool::getMemoryPool(std::list<tx_memory_pool::TransactionDetails> txs) const {
	  std::lock_guard<std::recursive_mutex> lock(m_transactions_lock);
	  for (const auto& txd : m_fee_index) {
		  txs.push_back(txd);
	  }
  }

  std::list<CryptoNote::tx_memory_pool::TransactionDetails> tx_memory_pool::getMemoryPool() const {
	  std::lock_guard<std::recursive_mutex> lock(m_transactions_lock);
	  std::list<tx_memory_pool::TransactionDetails> txs;
	  for (const auto& txd : m_fee_index) {
		  txs.push_back(txd);
	  }
	  return txs;
  }

  //---------------------------------------------------------------------------------
  void tx_memory_pool::get_difference(const std::vector<Crypto::Hash>& known_tx_ids, std::vector<Crypto::Hash>& new_tx_ids, std::vector<Crypto::Hash>& deleted_tx_ids) const {
    std::lock_guard<std::recursive_mutex> lock(m_transactions_lock);
    std::unordered_set<Crypto::Hash> ready_tx_ids;
    for (const auto& tx : m_transactions) {
      TransactionCheckInfo checkInfo(tx);
	  if (m_validated_transactions.find(tx.id) != m_validated_transactions.end()) {
		  ready_tx_ids.insert(tx.id);
		  logger(TRACE) << "MemPool - tx " << tx.id << " loaded from cache";
	  }
	  else if (is_transaction_ready_to_go(tx.tx, checkInfo)) {
		  ready_tx_ids.insert(tx.id);
		  m_validated_transactions.insert(tx.id);
		  logger(TRACE) << "MemPool - tx " << tx.id << " added to cache";
	  }
    }

    std::unordered_set<Crypto::Hash> known_set(known_tx_ids.begin(), known_tx_ids.end());
    for (auto it = ready_tx_ids.begin(), e = ready_tx_ids.end(); it != e;) {
      auto known_it = known_set.find(*it);
      if (known_it != known_set.end()) {
        known_set.erase(known_it);
        it = ready_tx_ids.erase(it);
      }
      else {
        ++it;
      }
    }

    new_tx_ids.assign(ready_tx_ids.begin(), ready_tx_ids.end());
    deleted_tx_ids.assign(known_set.begin(), known_set.end());
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::on_blockchain_inc(uint64_t new_block_height, const Crypto::Hash& top_block_id) {
    std::lock_guard<std::recursive_mutex> lock(m_transactions_lock);
    if (!m_validated_transactions.empty()) {
      logger(DEBUGGING) << "MemPool - Block height incremented, cleared " << m_validated_transactions.size() << " cached transaction hashes. New height: " << new_block_height << " Top block: " << top_block_id;
      m_validated_transactions.clear();
	}
    return true;
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::on_blockchain_dec(uint64_t new_block_height, const Crypto::Hash& top_block_id) {
    std::lock_guard<std::recursive_mutex> lock(m_transactions_lock);
    if (!m_validated_transactions.empty()) {
      logger(DEBUGGING, YELLOW) << "MemPool - Block height decremented " << m_validated_transactions.size() << " cached transaction hashes. New height: " << new_block_height << " Top block: " << top_block_id;
      m_validated_transactions.clear();
	}
    return true;
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::have_tx(const Crypto::Hash &id) const {
    std::lock_guard<std::recursive_mutex> lock(m_transactions_lock);
    if (m_transactions.count(id)) {
      return true;
    }
    return false;
  }
  //---------------------------------------------------------------------------------
  void tx_memory_pool::lock() const {
    m_transactions_lock.lock();
  }
  //---------------------------------------------------------------------------------
  void tx_memory_pool::unlock() const {
    m_transactions_lock.unlock();
  }

  std::unique_lock<std::recursive_mutex> tx_memory_pool::obtainGuard() const {
    return std::unique_lock<std::recursive_mutex>(m_transactions_lock);
  }

  //---------------------------------------------------------------------------------
  bool tx_memory_pool::is_transaction_ready_to_go(const Transaction& tx, TransactionCheckInfo& txd) const {

    if (!m_validator.checkTransactionInputs(tx, txd.maxUsedBlock, txd.lastFailedBlock))
      return false;

    //if we here, transaction seems valid, but, anyway, check for key_images collisions with blockchain, just to be sure
    if (m_validator.haveSpentKeyImages(tx))
      return false;

    //transaction is ok.
    return true;
  }
  //---------------------------------------------------------------------------------
  std::string tx_memory_pool::print_pool(bool short_format) const {
    std::stringstream ss;
    std::lock_guard<std::recursive_mutex> lock(m_transactions_lock);
    for (const auto& txd : m_fee_index) {
      ss << "id: " << txd.id << std::endl;
      
      if (!short_format) {
        ss << storeToJson(txd.tx) << std::endl;
      }

      ss << "blobSize: " << txd.blobSize << std::endl
        << "fee: " << m_currency.formatAmount(txd.fee) << std::endl
        << "keptByBlock: " << (txd.keptByBlock ? 'T' : 'F') << std::endl
        << "max_used_block_height: " << txd.maxUsedBlock.height << std::endl
        << "max_used_block_id: " << txd.maxUsedBlock.id << std::endl
        << "last_failed_height: " << txd.lastFailedBlock.height << std::endl
        << "last_failed_id: " << txd.lastFailedBlock.id << std::endl
        << "amount_out: " << get_outs_money_amount(txd.tx) << std::endl
        << "fee_atomic_units: " << txd.fee << std::endl
        << "received_timestamp: " << txd.receiveTime << std::endl
        << "received: " << std::ctime(&txd.receiveTime) << std::endl;
    }

    return ss.str();
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::fill_block_template(Block& bl, size_t median_size, size_t maxCumulativeSize,
                                           uint64_t already_generated_coins, size_t& total_size, uint64_t& fee) {
    std::lock_guard<std::recursive_mutex> lock(m_transactions_lock);

    total_size = 0;
    fee = 0;
    int counter = 0;

    size_t max_total_size = (125 * median_size) / 100;
    max_total_size = std::min(max_total_size, maxCumulativeSize) - m_currency.minerTxBlobReservedSize();

    BlockTemplate blockTemplate;

    for (auto i = m_fee_index.begin(); i != m_fee_index.end(); ++i) {
      if (counter == 127)
        break;

      const auto& txd = *i;

      size_t blockSizeLimit = (txd.fee == 0) ? median_size : max_total_size;
      if (blockSizeLimit < total_size + txd.blobSize) {
        continue;
      }

      tx_verification_context tvc = boost::value_initialized<tx_verification_context>();
      if (!m_core.check_tx_fee(txd.tx, getObjectHash(txd.tx), txd.blobSize, tvc, m_core.getCurrentBlockchainHeight())) {
        logger(DEBUGGING) << "Transaction " << txd.id << " not included to block template because fee is insufficient";
        continue;
      }

      TransactionCheckInfo checkInfo(txd);
      bool ready = false;
      if (m_validated_transactions.find(txd.id) != m_validated_transactions.end()) {
        ready = true;
        logger(DEBUGGING) << "Fill block template - tx added from cache: " << txd.id;
      }
      else if (is_transaction_ready_to_go(txd.tx, checkInfo)) {
        ready = true;
        m_validated_transactions.insert(txd.id);
        logger(DEBUGGING) << "Fill block template - tx added to cache: " << txd.id;
      }

      // update item state
      m_fee_index.modify(i, [&checkInfo](TransactionCheckInfo& item) {
        item = checkInfo;
      });

      if (ready && blockTemplate.addTransaction(txd.id, txd.tx)) {
        total_size += txd.blobSize;
        fee += txd.fee;
        ++counter;
        logger(DEBUGGING) << "Transaction " << txd.id << " included to block template";
      } else {
        logger(DEBUGGING) << "Transaction " << txd.id << " is failed to include to block template";
      }
    }

    bl.transactionHashes = blockTemplate.getTransactions();
    return true;
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::init(const std::string& config_folder) {
    std::lock_guard<std::recursive_mutex> lock(m_transactions_lock);

    m_config_folder = config_folder;
    std::string state_file_path = config_folder + "/" + m_currency.txPoolFileName();
    boost::system::error_code ec;
    if (!boost::filesystem::exists(state_file_path, ec)) {
      return true;
    }

    if (!loadFromBinaryFile(*this, state_file_path)) {
      logger(ERROR) << "Failed to load memory pool from file " << state_file_path;

      m_transactions.clear();
      m_spent_key_images.clear();
      m_spentOutputs.clear();

      m_paymentIdIndex.clear();
      m_timestampIndex.clear();
    } else {
      buildIndices();
    }

    removeExpiredTransactions();

    // Ignore deserialization error
    return true;
  }
  //---------------------------------------------------------------------------------
  bool tx_memory_pool::deinit() {
    if (!Tools::create_directories_if_necessary(m_config_folder)) {
      logger(INFO) << "Failed to create data directory: " << m_config_folder;
      return false;
    }

    std::string state_file_path = m_config_folder + "/" + m_currency.txPoolFileName();

    if (!storeToBinaryFile(*this, state_file_path)) {
      logger(INFO) << "Failed to serialize memory pool to file " << state_file_path;
    }

    m_paymentIdIndex.clear();
    m_timestampIndex.clear();
    
    return true;
  }

#define CURRENT_MEMPOOL_ARCHIVE_VER 1

  void serialize(CryptoNote::tx_memory_pool::TransactionDetails& td, ISerializer& s) {
    s(td.id, "id");
    s(td.blobSize, "blobSize");
    s(td.fee, "fee");
    s(td.tx, "tx");
    s(td.maxUsedBlock.height, "maxUsedBlock.height");
    s(td.maxUsedBlock.id, "maxUsedBlock.id");
    s(td.lastFailedBlock.height, "lastFailedBlock.height");
    s(td.lastFailedBlock.id, "lastFailedBlock.id");
    s(td.keptByBlock, "keptByBlock");
    s(reinterpret_cast<uint64_t&>(td.receiveTime), "receiveTime");
  }

  //---------------------------------------------------------------------------------
  void tx_memory_pool::serialize(ISerializer& s) {

    uint8_t version = CURRENT_MEMPOOL_ARCHIVE_VER;

    s(version, "version");

    if (version != CURRENT_MEMPOOL_ARCHIVE_VER) {
      return;
    }

    std::lock_guard<std::recursive_mutex> lock(m_transactions_lock);

    if (s.type() == ISerializer::INPUT) {
      m_transactions.clear();
      readSequence<TransactionDetails>(std::inserter(m_transactions, m_transactions.end()), "transactions", s);
    } else {
      writeSequence<TransactionDetails>(m_transactions.begin(), m_transactions.end(), "transactions", s);
    }

    KV_MEMBER(m_spent_key_images);
    KV_MEMBER(m_spentOutputs);
    KV_MEMBER(m_recentlyDeletedTransactions);
  }

  //---------------------------------------------------------------------------------
  void tx_memory_pool::on_idle() {
    m_txCheckInterval.call([this](){ return removeExpiredTransactions(); });
  }

  //---------------------------------------------------------------------------------
  bool tx_memory_pool::removeExpiredTransactions() {
    bool somethingRemoved = false;
    {
      std::lock_guard<std::recursive_mutex> lock(m_transactions_lock);

      uint64_t now = m_timeProvider.now();

      for (auto it = m_recentlyDeletedTransactions.begin(); it != m_recentlyDeletedTransactions.end();) {
        uint64_t elapsedTimeSinceDeletion = now - it->second;
        if (elapsedTimeSinceDeletion > m_currency.numberOfPeriodsToForgetTxDeletedFromPool() * m_currency.mempoolTxLiveTime()) {
          it = m_recentlyDeletedTransactions.erase(it);
        } else {
          ++it;
        }
      }

      for (auto it = m_transactions.begin(); it != m_transactions.end();) {
        uint64_t txAge = now - it->receiveTime;
        bool remove = txAge > (it->keptByBlock ? m_currency.mempoolTxFromAltBlockLiveTime() : m_currency.mempoolTxLiveTime());

        if (remove) {
          logger(TRACE) << "Tx " << it->id << " removed from tx pool due to outdated, age: " << txAge;
          m_recentlyDeletedTransactions.emplace(it->id, now);
          it = removeTransaction(it);
          somethingRemoved = true;
        } else {
          ++it;
        }
      }
    }

    if (somethingRemoved) {
      m_observerManager.notify(&ITxPoolObserver::txDeletedFromPool);
    }

    return true;
  }

  tx_memory_pool::tx_container_t::iterator tx_memory_pool::removeTransaction(tx_memory_pool::tx_container_t::iterator i) {
    removeTransactionInputs(i->id, i->tx, i->keptByBlock);
    m_paymentIdIndex.remove(i->tx);
    m_timestampIndex.remove(i->receiveTime, i->id);
    if (m_validated_transactions.find(i->id) != m_validated_transactions.end()) {
      m_validated_transactions.erase(i->id);
      logger(DEBUGGING) << "Removing transaction from MemPool cache " << i->id << ". Cache size: " << m_validated_transactions.size();
    }
    return m_transactions.erase(i);
  }

  bool tx_memory_pool::removeTransactionInputs(const Crypto::Hash& tx_id, const Transaction& tx, bool keptByBlock) {
    for (const auto& in : tx.inputs) {
      if (in.type() == typeid(KeyInput)) {
        const auto& txin = boost::get<KeyInput>(in);
        auto it = m_spent_key_images.find(txin.keyImage);
        if (!(it != m_spent_key_images.end())) { logger(ERROR, BRIGHT_RED) << "failed to find transaction input in key images. img=" << txin.keyImage << std::endl
          << "transaction id = " << tx_id; return false; }
        std::unordered_set<Crypto::Hash>& key_image_set = it->second;
        if (!(!key_image_set.empty())) { logger(ERROR, BRIGHT_RED) << "empty key_image set, img=" << txin.keyImage << std::endl
          << "transaction id = " << tx_id; return false; }

        auto it_in_set = key_image_set.find(tx_id);
        if (!(it_in_set != key_image_set.end())) { logger(ERROR, BRIGHT_RED) << "transaction id not found in key_image set, img=" << txin.keyImage << std::endl
          << "transaction id = " << tx_id; return false; }
        key_image_set.erase(it_in_set);
        if (key_image_set.empty()) {
          //it is now empty hash container for this key_image
          m_spent_key_images.erase(it);
        }
      } else if (in.type() == typeid(MultisignatureInput)) {
        if (!keptByBlock) {
          const auto& msig = boost::get<MultisignatureInput>(in);
          auto output = GlobalOutput(msig.amount, msig.outputIndex);
          assert(m_spentOutputs.count(output));
          m_spentOutputs.erase(output);
        }
      }
    }

    return true;
  }

  //---------------------------------------------------------------------------------
  bool tx_memory_pool::addTransactionInputs(const Crypto::Hash& id, const Transaction& tx, bool keptByBlock) {
    // should not fail
    for (const auto& in : tx.inputs) {
      if (in.type() == typeid(KeyInput)) {
        const auto& txin = boost::get<KeyInput>(in);
        std::unordered_set<Crypto::Hash>& kei_image_set = m_spent_key_images[txin.keyImage];
        if (!(keptByBlock || kei_image_set.size() == 0)) {
          logger(ERROR, BRIGHT_RED)
              << "internal error: keptByBlock=" << keptByBlock
              << ",  kei_image_set.size()=" << kei_image_set.size() << ENDL
              << "txin.keyImage=" << txin.keyImage << ENDL << "tx_id=" << id;
          return false;
        }
        auto ins_res = kei_image_set.insert(id);
        if (!(ins_res.second)) {
          logger(ERROR, BRIGHT_RED) << "internal error: try to insert duplicate iterator in key_image set";
          return false;
        }
      } else if (in.type() == typeid(MultisignatureInput)) {
        if (!keptByBlock) {
          const auto& msig = boost::get<MultisignatureInput>(in);
          auto r = m_spentOutputs.insert(GlobalOutput(msig.amount, msig.outputIndex));
          (void)r;
          assert(r.second);
        }
      }
    }

    return true;
  }

  //---------------------------------------------------------------------------------
  bool tx_memory_pool::haveSpentInputs(const Transaction& tx) const {
    for (const auto& in : tx.inputs) {
      if (in.type() == typeid(KeyInput)) {
        const auto& tokey_in = boost::get<KeyInput>(in);
        if (m_spent_key_images.count(tokey_in.keyImage)) {
          return true;
        }
      } else if (in.type() == typeid(MultisignatureInput)) {
        const auto& msig = boost::get<MultisignatureInput>(in);
        if (m_spentOutputs.count(GlobalOutput(msig.amount, msig.outputIndex))) {
          return true;
        }
      }
    }
    return false;
  }

  bool tx_memory_pool::addObserver(ITxPoolObserver* observer) {
    return m_observerManager.add(observer);
  }

  bool tx_memory_pool::removeObserver(ITxPoolObserver* observer) {
    return m_observerManager.remove(observer);
  }

  void tx_memory_pool::buildIndices() {
    std::lock_guard<std::recursive_mutex> lock(m_transactions_lock);
    for (auto it = m_transactions.begin(); it != m_transactions.end(); it++) {
      m_paymentIdIndex.add(it->tx);
      m_timestampIndex.add(it->receiveTime, it->id);
    }
  }

  bool tx_memory_pool::getTransactionIdsByPaymentId(const Crypto::Hash& paymentId, std::vector<Crypto::Hash>& transactionIds) {
    std::lock_guard<std::recursive_mutex> lock(m_transactions_lock);
    //return m_paymentIdIndex.find(paymentId, transactionIds);
	transactionIds = m_paymentIdIndex.find(paymentId);
	return true;
  }

  bool tx_memory_pool::getTransactionIdsByTimestamp(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<Crypto::Hash>& hashes, uint64_t& transactionsNumberWithinTimestamps) {
    std::lock_guard<std::recursive_mutex> lock(m_transactions_lock);
    return m_timestampIndex.find(timestampBegin, timestampEnd, transactionsNumberLimit, hashes, transactionsNumberWithinTimestamps);
  }
}
