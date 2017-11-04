// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <numeric>

#include <boost/program_options.hpp>
#include <boost/serialization/variant.hpp>

#include <numeric>

#include "Common/CommandLine.h"
#include "Common/ConsoleTools.h"

#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/Core.h"
#include "CryptoNoteCore/DatabaseBlockchainCacheFactory.h"
#include "CryptoNoteCore/MemoryBlockchainCacheFactory.h"
#include "CryptoNoteCore/IUpgradeDetector.h"
#include "CryptoNoteCore/TransactionExtra.h"

#include "CryptoNoteProtocol/CryptoNoteProtocolHandlerCommon.h"

#include "../TestGenerator/TestGenerator.h"
#include "CryptoNoteCore/CryptoNoteTools.h"

#include "BoostSerializationHelper.h"
#include "AccountBoostSerialization.h"

#include <Logging/LoggerGroup.h>
#include <Logging/ConsoleLogger.h>

#include <../tests/UnitTests/DataBaseMock.h>
#include <../tests/Common/VectorMainChainStorage.h>

namespace concolor {
using namespace Common::Console;

inline std::basic_ostream<char, std::char_traits<char>>&
bright_white(std::basic_ostream<char, std::char_traits<char>>& ostr) {
  setTextColor(Color::BrightWhite);
  return ostr;
}

inline std::basic_ostream<char, std::char_traits<char>>& red(std::basic_ostream<char, std::char_traits<char>>& ostr) {
  setTextColor(Color::BrightRed);
  return ostr;
}

inline std::basic_ostream<char, std::char_traits<char>>& green(std::basic_ostream<char, std::char_traits<char>>& ostr) {
  setTextColor(Color::BrightGreen);
  return ostr;
}

inline std::basic_ostream<char, std::char_traits<char>>&
magenta(std::basic_ostream<char, std::char_traits<char>>& ostr) {
  setTextColor(Color::BrightMagenta);
  return ostr;
}

inline std::basic_ostream<char, std::char_traits<char>>&
yellow(std::basic_ostream<char, std::char_traits<char>>& ostr) {
  setTextColor(Color::BrightYellow);
  return ostr;
}

inline std::basic_ostream<char, std::char_traits<char>>&
normal(std::basic_ostream<char, std::char_traits<char>>& ostr) {
  setTextColor(Color::Default);
  return ostr;
}
}

#define LOG_ERROR(msg) std::cout << concolor::red << msg << concolor::normal << std::endl
#define CHECK_AND_ASSERT_MES(expr, fail_ret_val, message)                                                              \
  do {                                                                                                                 \
    if (!(expr)) {                                                                                                     \
      std::cout << concolor::red << message << concolor::normal << std::endl;                                          \
      return fail_ret_val;                                                                                             \
    };                                                                                                                 \
  } while (0)
#define CHECK_AND_NO_ASSERT_MES(expr, fail_ret_val, message)                                                           \
  do {                                                                                                                 \
    if (!(expr)) {                                                                                                     \
      std::cout << concolor::red << message << concolor::normal << std::endl;                                          \
      return fail_ret_val;                                                                                             \
    };                                                                                                                 \
  } while (0)

namespace {

Crypto::Hash getBlockHash(const CryptoNote::BlockTemplate& blk) {
  return CryptoNote::CachedBlock(blk).getBlockHash();
}
uint64_t getSummaryOutsAmount(const CryptoNote::Transaction& transaction) {
  return std::accumulate(transaction.outputs.begin(), transaction.outputs.end(), uint64_t(0),
                         [](uint64_t sum, const CryptoNote::TransactionOutput& out) { return sum + out.amount; });
}
}

namespace CryptoNote {

inline bool operator==(const CryptoNote::Transaction& a, const CryptoNote::Transaction& b) {
  using SigVect = decltype(a.signatures)::value_type;
  return std::equal(a.signatures.begin(), a.signatures.end(), b.signatures.begin(),
                    [](const SigVect& l, const SigVect& r) { return std::equal(l.begin(), l.end(), r.begin()); });
}
inline bool operator==(const CryptoNote::BaseTransaction& a, const CryptoNote::BaseTransaction& b) {
  return getObjectHash(a) == getObjectHash(b);
}
inline bool operator==(const CryptoNote::BlockHeader& a, const CryptoNote::BlockHeader& b) {
  return a.majorVersion == b.majorVersion && a.minorVersion == b.minorVersion && a.nonce == b.nonce &&
         a.timestamp == b.timestamp && a.previousBlockHash == b.previousBlockHash;
}
inline bool operator==(const CryptoNote::ParentBlock& a, const CryptoNote::ParentBlock& b) {
  return std::equal(a.blockchainBranch.begin(), a.blockchainBranch.end(), b.blockchainBranch.end()) &&
         std::equal(a.baseTransactionBranch.begin(), a.baseTransactionBranch.end(), b.baseTransactionBranch.begin()) &&
         a.majorVersion == b.majorVersion && a.minorVersion == b.minorVersion &&
         a.previousBlockHash == b.previousBlockHash && a.transactionCount == b.transactionCount &&
         a.previousBlockHash == b.previousBlockHash && a.baseTransaction == b.baseTransaction;
}

// remove parentBlock comparison here, cause it isn't usually initialized
inline bool operator==(const CryptoNote::BlockTemplate& a, const CryptoNote::BlockTemplate& b) {
  return static_cast<const BlockHeader&>(a) == static_cast<const BlockHeader&>(b) &&
         a.baseTransaction == b.baseTransaction && /*a.parentBlock == b.parentBlock &&*/
         std::equal(a.transactionHashes.begin(), a.transactionHashes.end(), b.transactionHashes.begin());
}
}

struct callback_entry {
  std::string callback_name;

private:
  friend class boost::serialization::access;

  template <class Archive> void serialize(Archive& ar, const unsigned int /*version*/) {
    ar& callback_name;
  }
};

template <typename T> struct serialized_object {
  serialized_object() {
  }

  serialized_object(const CryptoNote::BinaryArray& a_data) : data(a_data) {
  }

  CryptoNote::BinaryArray data;

private:
  friend class boost::serialization::access;

  template <class Archive> void serialize(Archive& ar, const unsigned int /*version*/) {
    ar& data;
  }
};

namespace CryptoNote {
template <class Archive> void serialize(Archive& ar, RawBlock& raw, const unsigned int /*version*/) {
  ar& raw.block;
  ar& raw.transactions;
}
}

typedef serialized_object<CryptoNote::BlockTemplate> serialized_block;
typedef serialized_object<CryptoNote::Transaction> serialized_transaction;

struct event_visitor_settings {
  int valid_mask;
  bool txs_keeped_by_block;

  enum settings { set_txs_keeped_by_block = 1 << 0 };

  event_visitor_settings(int a_valid_mask = 0, bool a_txs_keeped_by_block = false)
      : valid_mask(a_valid_mask), txs_keeped_by_block(a_txs_keeped_by_block) {
  }

private:
  friend class boost::serialization::access;

  template <class Archive> void serialize(Archive& ar, const unsigned int /*version*/) {
    ar& valid_mask;
    ar& txs_keeped_by_block;
  }
};

typedef boost::variant<CryptoNote::BlockTemplate, CryptoNote::RawBlock, CryptoNote::Transaction, CryptoNote::AccountBase, callback_entry,
                       serialized_block, serialized_transaction, event_visitor_settings> test_event_entry;
typedef std::unordered_map<Crypto::Hash, CryptoNote::Transaction> map_hash2tx_t;

class test_chain_unit_base : boost::noncopyable {
public:
  using BlockError = CryptoNote::error::AddBlockErrorCode;
  using Currency = CryptoNote::Currency;
  test_chain_unit_base() : m_currency(new Currency(CryptoNote::CurrencyBuilder(m_logger).currency())) {
  }

  typedef std::function<bool(CryptoNote::Core& c, size_t ev_index, const std::vector<test_event_entry>& events)>
      verify_callback;
  typedef std::map<std::string, verify_callback> callbacks_map;

  const CryptoNote::Currency& currency() const;
  void register_callback(const std::string& cb_name, verify_callback cb);
  bool verify(const std::string& cb_name, CryptoNote::Core& c, size_t ev_index,
              const std::vector<test_event_entry>& events);

  bool blockWasNotAdded(std::error_code err) {
    return err != BlockError::ADDED_TO_MAIN && err != BlockError::ADDED_TO_ALTERNATIVE &&
           err != BlockError::ADDED_TO_ALTERNATIVE_AND_SWITCHED;
  }

  bool blockWasAdded(std::error_code err) {
    return !blockWasNotAdded(err);
  }

protected:
  mutable Logging::ConsoleLogger m_logger;
  std::unique_ptr<CryptoNote::Currency> m_currency;

private:
  callbacks_map m_callbacks;
};

bool construct_tx_to_key(Logging::ILogger& logger, const std::vector<test_event_entry>& events,
                         CryptoNote::Transaction& tx, const CryptoNote::BlockTemplate& blk_head,
                         const CryptoNote::AccountBase& from, const CryptoNote::AccountBase& to, uint64_t amount,
                         uint64_t fee, size_t nmix);
CryptoNote::Transaction construct_tx_with_fee(Logging::ILogger& logger, std::vector<test_event_entry>& events,
                                              const CryptoNote::BlockTemplate& blk_head,
                                              const CryptoNote::AccountBase& acc_from,
                                              const CryptoNote::AccountBase& acc_to, uint64_t amount, uint64_t fee);

void get_confirmed_txs(const std::vector<CryptoNote::BlockTemplate>& blockchain, const map_hash2tx_t& mtx,
                       map_hash2tx_t& confirmed_txs);
bool find_block_chain(const std::vector<test_event_entry>& events, std::vector<CryptoNote::BlockTemplate>& blockchain,
                      map_hash2tx_t& mtx, const Crypto::Hash& head);
void fill_tx_sources_and_destinations(const std::vector<test_event_entry>& events,
                                      const CryptoNote::BlockTemplate& blk_head, const CryptoNote::AccountBase& from,
                                      const CryptoNote::AccountBase& to, uint64_t amount, uint64_t fee, size_t nmix,
                                      std::vector<CryptoNote::TransactionSourceEntry>& sources,
                                      std::vector<CryptoNote::TransactionDestinationEntry>& destinations);
uint64_t get_balance(const CryptoNote::AccountBase& addr, const std::vector<CryptoNote::BlockTemplate>& blockchain,
                     const map_hash2tx_t& mtx);

//--------------------------------------------------------------------------
template <class t_test_class>
auto do_check_tx_verification_context(bool tve, bool tx_added, size_t event_index, const CryptoNote::Transaction& tx,
                                      t_test_class& validator, int)
    -> decltype(validator.check_tx_verification_context(tve, tx_added, event_index, tx)) {
  return validator.check_tx_verification_context(tve, tx_added, event_index, tx);
}
//--------------------------------------------------------------------------
template <class t_test_class>
bool do_check_tx_verification_context(bool tve, bool tx_added, size_t /*event_index*/,
                                      const CryptoNote::Transaction& /*tx*/, t_test_class&, long) {
  // Default block verification context check
  if (!tve)
    throw std::runtime_error("Transaction verification failed");
  return true;
}
//--------------------------------------------------------------------------
template <class t_test_class>
bool check_tx_verification_context(bool tve, bool tx_added, size_t event_index, const CryptoNote::Transaction& tx,
                                   t_test_class& validator) {
  // SFINAE in action
  return do_check_tx_verification_context(tve, tx_added, event_index, tx, validator, 0);
}
//--------------------------------------------------------------------------
template <class t_test_class>
auto do_check_block_verification_context(std::error_code bve, size_t event_index, const CryptoNote::BlockTemplate& blk,
                                         t_test_class& validator, int)
    -> decltype(validator.check_block_verification_context(bve, event_index, blk)) {
  return validator.check_block_verification_context(bve, event_index, blk);
}
//--------------------------------------------------------------------------
template <class t_test_class>
auto do_check_block_verification_context(std::error_code bve, size_t event_index, const CryptoNote::RawBlock& blk,
                                         t_test_class& validator, int)
    -> decltype(validator.check_block_verification_context(bve, event_index, blk)) {
  return validator.check_block_verification_context(bve, event_index, blk);
}
//--------------------------------------------------------------------------
template <class t_test_class>
bool do_check_block_verification_context(std::error_code bve, size_t /*event_index*/,
                                         const CryptoNote::BlockTemplate& /*blk*/, t_test_class&, long) {
  // Default block verification context check
  if (bve != CryptoNote::error::AddBlockErrorCode::ADDED_TO_MAIN && 
      bve != CryptoNote::error::AddBlockErrorCode::ADDED_TO_ALTERNATIVE &&
      bve != CryptoNote::error::AddBlockErrorCode::ADDED_TO_ALTERNATIVE_AND_SWITCHED)
    throw std::runtime_error("Block verification failed, " + bve.message());
  return true;
}
//--------------------------------------------------------------------------
template <class t_test_class>
bool do_check_block_verification_context(std::error_code bve, size_t /*event_index*/,
                                         const CryptoNote::RawBlock& /*blk*/, t_test_class&, long) {
  // SFINAE in action
  if (bve != CryptoNote::error::AddBlockErrorCode::ADDED_TO_MAIN && 
      bve != CryptoNote::error::AddBlockErrorCode::ADDED_TO_ALTERNATIVE &&
      bve != CryptoNote::error::AddBlockErrorCode::ADDED_TO_ALTERNATIVE_AND_SWITCHED)
    throw std::runtime_error("Block verification failed, " + bve.message());
  return true;
}
//--------------------------------------------------------------------------
template <class t_test_class>
bool check_block_verification_context(std::error_code bve, size_t event_index, const CryptoNote::BlockTemplate& blk,
                                      t_test_class& validator) {
  // SFINAE in action
  return do_check_block_verification_context(bve, event_index, blk, validator, 0);
}

template <class t_test_class>
bool check_block_verification_context(std::error_code bve, size_t event_index, const CryptoNote::RawBlock& blk,
                                      t_test_class& validator) {
  // SFINAE in action
  return do_check_block_verification_context(bve, event_index, blk, validator, 0);
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
template <class t_test_class> struct push_core_event_visitor : public boost::static_visitor<bool> {
private:
  CryptoNote::Core& m_c;
  const std::vector<test_event_entry>& m_events;
  t_test_class& m_validator;
  size_t m_ev_index;

  bool m_txs_keeped_by_block;

public:
  push_core_event_visitor(CryptoNote::Core& c, const std::vector<test_event_entry>& events, t_test_class& validator)
      : m_c(c), m_events(events), m_validator(validator), m_ev_index(0), m_txs_keeped_by_block(false) {
  }

  void event_index(size_t ev_index) {
    m_ev_index = ev_index;
  }

  bool operator()(const event_visitor_settings& settings) {
    log_event("event_visitor_settings");

    if (settings.valid_mask & event_visitor_settings::set_txs_keeped_by_block) {
      m_txs_keeped_by_block = settings.txs_keeped_by_block;
    }

    return true;
  }

  bool operator()(const CryptoNote::Transaction& tx) const {
    log_event("CryptoNote::Transaction");

    size_t pool_size = m_c.getPoolTransactionCount();
    CryptoNote::BinaryArray packedTx;
    toBinaryArray(tx, packedTx);
    auto result = m_c.addTransactionToPool(packedTx);
    bool tx_added = pool_size + 1 == m_c.getPoolTransactionCount();
    bool r = check_tx_verification_context(result, tx_added, m_ev_index, tx, m_validator);
    CHECK_AND_NO_ASSERT_MES(r, false, "tx verification context check failed");
    return true;
  }

  bool operator()(const CryptoNote::RawBlock& b) const {
    log_event("CryptoNote::BlockTemplate");

    auto rawBlock = b;
    auto result = m_c.addBlock(std::move(rawBlock));
    bool r = check_block_verification_context(result, m_ev_index, b, m_validator);
    CHECK_AND_NO_ASSERT_MES(r, false, "block verification context check failed");
    return r;
  }

  bool operator()(const CryptoNote::BlockTemplate& b) const {
    log_event("CryptoNote::BlockTemplate");

    CryptoNote::BinaryArray arr;
    toBinaryArray(b, arr); // ignore exceptions here
    CryptoNote::RawBlock rawBlock{arr, {}};
    auto result = m_c.addBlock(std::move(rawBlock));
    bool r = check_block_verification_context(result, m_ev_index, b, m_validator);
    CHECK_AND_NO_ASSERT_MES(r, false, "block verification context check failed");
    return r;
  }

  bool operator()(const callback_entry& cb) const {
    log_event(std::string("callback_entry ") + cb.callback_name);
    return m_validator.verify(cb.callback_name, m_c, m_ev_index, m_events);
  }

  bool operator()(const CryptoNote::AccountBase& ab) const {
    log_event("CryptoNote::account_base");
    return true;
  }

  bool operator()(const serialized_block& sr_block) const {
    log_event("serialized_block");

    CryptoNote::RawBlock rawBlock{sr_block.data, {}};
    auto bvc = m_c.addBlock(std::move(rawBlock));
    CryptoNote::BlockTemplate blk;
    if (!CryptoNote::fromBinaryArray(blk, sr_block.data)) {
      blk = CryptoNote::BlockTemplate();
    }

    auto r = check_block_verification_context(bvc, m_ev_index, blk, m_validator);
    CHECK_AND_NO_ASSERT_MES(r, false, "block verification context check failed");
    return true;
  }

  bool operator()(const serialized_transaction& sr_tx) const {
    log_event("serialized_transaction");

    size_t pool_size = m_c.getPoolTransactionCount();
    bool result = m_c.addTransactionToPool(sr_tx.data);
    bool tx_added = pool_size + 1 == m_c.getPoolTransactionCount();

    CryptoNote::Transaction tx;

    if (!CryptoNote::fromBinaryArray(tx, sr_tx.data)) {
      tx = CryptoNote::Transaction();
    }

    bool r = check_tx_verification_context(result, tx_added, m_ev_index, tx, m_validator);
    CHECK_AND_NO_ASSERT_MES(r, false, "transaction verification context check failed");
    return true;
  }

private:
  void log_event(const std::string& event_type) const {
    std::cout << concolor::yellow << "=== EVENT # " << m_ev_index << ": " << event_type << concolor::normal
              << std::endl;
  }
};
//--------------------------------------------------------------------------
template <class t_test_class>
inline bool replay_events_through_core(CryptoNote::Core& cr, const std::vector<test_event_entry>& events,
                                       t_test_class& validator) {
  try {
    bool r = true;
    push_core_event_visitor<t_test_class> visitor(cr, events, validator);
    // genesis is generated in core
    for (size_t i = 1; i < events.size() && r; ++i) {
      visitor.event_index(i);
      r = boost::apply_visitor(visitor, events[i]);
    }

    return r;
  } catch (std::exception& e) {
    std::cout << "replay_events_through_core: " << e.what();
    return false;
  }
}
//--------------------------------------------------------------------------
template <class t_test_class>
inline bool do_replay_events(std::vector<test_event_entry>& events, t_test_class& validator) {
  boost::program_options::options_description desc("Allowed options");
  command_line::add_arg(desc, command_line::arg_data_dir);
  boost::program_options::variables_map vm;
  bool r = command_line::handle_error_helper(desc, [&]() {
    boost::program_options::store(boost::program_options::basic_parsed_options<char>(&desc), vm);
    boost::program_options::notify(vm);
    return true;
  });
  if (!r)
    return false;

  Logging::ConsoleLogger logger;
  try {
    System::Dispatcher dispatcher;
    CryptoNote::DataBaseMock database;
    CryptoNote::Core c(
      validator.currency(),
      logger,
      CryptoNote::Checkpoints(logger),
      dispatcher,
      std::unique_ptr<CryptoNote::IBlockchainCacheFactory>(new CryptoNote::DatabaseBlockchainCacheFactory(database, logger)),
      CryptoNote::createVectorMainChainStorage(validator.currency()));
    c.load();
    return replay_events_through_core<t_test_class>(c, events, validator);
  } catch (std::exception& e) {
    std::cout << concolor::magenta << "Failed to init core: " << e.what() << concolor::normal << std::endl;
    return false;
  }
}
//--------------------------------------------------------------------------
template <class t_test_class> inline bool do_replay_file(const std::string& filename) {
  std::vector<test_event_entry> events;
  if (!Tools::unserialize_obj_from_file(events, filename)) {
    std::cout << concolor::magenta << "Failed to deserialize data from file: " << filename << concolor::normal
              << std::endl;
    return false;
  }
  t_test_class validator;
  return do_replay_events<t_test_class>(events, validator);
}
//--------------------------------------------------------------------------
#define GENERATE_ACCOUNT(account)                                                                                      \
  CryptoNote::AccountBase account;                                                                                     \
  account.generate();

#define MAKE_ACCOUNT(VEC_EVENTS, account)                                                                              \
  CryptoNote::AccountBase account;                                                                                     \
  account.generate();                                                                                                  \
  VEC_EVENTS.push_back(account);

#define DO_CALLBACK(VEC_EVENTS, CB_NAME)                                                                               \
  {                                                                                                                    \
    callback_entry CALLBACK_ENTRY;                                                                                     \
    CALLBACK_ENTRY.callback_name = CB_NAME;                                                                            \
    VEC_EVENTS.push_back(CALLBACK_ENTRY);                                                                              \
  }

#define REGISTER_CALLBACK(CB_NAME, CLBACK)                                                                             \
  register_callback(CB_NAME,                                                                                           \
                    std::bind(&CLBACK, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

#define REGISTER_CALLBACK_METHOD(CLASS, METHOD)                                                                        \
  register_callback(                                                                                                   \
      #METHOD, std::bind(&CLASS::METHOD, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

#define MAKE_GENESIS_BLOCK(VEC_EVENTS, BLK_NAME, MINER_ACC, TS)                                                        \
  test_generator generator(*this->m_currency);                                                                         \
  CryptoNote::BlockTemplate BLK_NAME = this->m_currency->genesisBlock();                                               \
  VEC_EVENTS.push_back(BLK_NAME);

#define MAKE_NEXT_BLOCK(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC)                                                   \
  CryptoNote::BlockTemplate BLK_NAME;                                                                                  \
  generator.constructBlock(BLK_NAME, PREV_BLOCK, MINER_ACC);                                                           \
  VEC_EVENTS.push_back(BLK_NAME);

#define MAKE_NEXT_BLOCK_TX1(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC, TX1)                                          \
  CryptoNote::BlockTemplate BLK_NAME;                                                                                  \
  {                                                                                                                    \
    std::list<CryptoNote::Transaction> tx_list;                                                                        \
    tx_list.push_back(TX1);                                                                                            \
    generator.constructBlock(BLK_NAME, PREV_BLOCK, MINER_ACC, tx_list);                                                \
    VEC_EVENTS.push_back(populateBlock(BLK_NAME, tx_list));                                                            \
  }

static inline CryptoNote::RawBlock populateBlock(const CryptoNote::BlockTemplate& block,
                                   const std::list<CryptoNote::Transaction>& txs) {
  CryptoNote::RawBlock raw{toBinaryArray(block), {}};
  std::transform(std::begin(txs), std::end(txs), std::back_inserter(raw.transactions),
                 [&](const CryptoNote::Transaction& tx) { return toBinaryArray(tx); });
  return raw;
}

#define MAKE_NEXT_BLOCK_TX_LIST(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC, TXLIST)                                   \
  CryptoNote::BlockTemplate BLK_NAME;                                                                                  \
  generator.constructBlock(BLK_NAME, PREV_BLOCK, MINER_ACC, TXLIST);                                                   \
  VEC_EVENTS.push_back(populateBlock(BLK_NAME, TXLIST));

#define REWIND_BLOCKS_N(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC, COUNT)                                            \
  CryptoNote::BlockTemplate BLK_NAME;                                                                                  \
  {                                                                                                                    \
    CryptoNote::BlockTemplate blk_last = PREV_BLOCK;                                                                   \
    for (size_t i = 0; i < COUNT; ++i) {                                                                               \
      MAKE_NEXT_BLOCK(VEC_EVENTS, blk, blk_last, MINER_ACC);                                                           \
      blk_last = blk;                                                                                                  \
    }                                                                                                                  \
    BLK_NAME = blk_last;                                                                                               \
  }

#define REWIND_BLOCKS(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC)                                                     \
  REWIND_BLOCKS_N(VEC_EVENTS, BLK_NAME, PREV_BLOCK, MINER_ACC, this->m_currency->minedMoneyUnlockWindow())

#define MAKE_TX_MIX(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, NMIX, HEAD)                                                 \
  CryptoNote::Transaction TX_NAME;                                                                                     \
  construct_tx_to_key(this->m_logger, VEC_EVENTS, TX_NAME, HEAD, FROM, TO, AMOUNT, this->m_currency->minimumFee(),     \
                      NMIX);                                                                                           \
  VEC_EVENTS.push_back(TX_NAME);

#define MAKE_TX(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, HEAD) MAKE_TX_MIX(VEC_EVENTS, TX_NAME, FROM, TO, AMOUNT, 0, HEAD)

#define MAKE_TX_MIX_LIST(VEC_EVENTS, SET_NAME, FROM, TO, AMOUNT, NMIX, HEAD)                                           \
  {                                                                                                                    \
    CryptoNote::Transaction t;                                                                                         \
    construct_tx_to_key(this->m_logger, VEC_EVENTS, t, HEAD, FROM, TO, AMOUNT, this->m_currency->minimumFee(), NMIX);  \
    SET_NAME.push_back(t);                                                                                             \
    VEC_EVENTS.push_back(t);                                                                                           \
  }

#define MAKE_TX_LIST(VEC_EVENTS, SET_NAME, FROM, TO, AMOUNT, HEAD)                                                     \
  MAKE_TX_MIX_LIST(VEC_EVENTS, SET_NAME, FROM, TO, AMOUNT, 0, HEAD)

#define MAKE_TX_LIST_START(VEC_EVENTS, SET_NAME, FROM, TO, AMOUNT, HEAD)                                               \
  std::list<CryptoNote::Transaction> SET_NAME;                                                                         \
  MAKE_TX_LIST(VEC_EVENTS, SET_NAME, FROM, TO, AMOUNT, HEAD);

#define MAKE_MINER_TX_AND_KEY_MANUALLY(TX, BLK, KEY)                                                                   \
  Transaction TX;                                                                                                      \
  if (!constructMinerTxManually(*this->m_currency, BLK.majorVersion, CachedBlock(BLK).getBlockIndex() + 1,             \
                                generator.getAlreadyGeneratedCoins(BLK), miner_account.getAccountKeys().address, TX,   \
                                0, KEY))                                                                               \
    return false;

#define MAKE_MINER_TX_MANUALLY(TX, BLK) MAKE_MINER_TX_AND_KEY_MANUALLY(TX, BLK, 0)

#define SET_EVENT_VISITOR_SETT(VEC_EVENTS, SETT, VAL) VEC_EVENTS.push_back(event_visitor_settings(SETT, VAL));

#define GENERATE(filename, genclass)                                                                                   \
  {                                                                                                                    \
    std::vector<test_event_entry> events;                                                                              \
    genclass g;                                                                                                        \
    g.generate(events);                                                                                                \
    if (!Tools::serialize_obj_to_file(events, filename)) {                                                             \
      std::cout << concolor::magenta << "Failed to serialize data to file: " << filename << concolor::normal           \
                << std::endl;                                                                                          \
      throw std::runtime_error("Failed to serialize data to file");                                                    \
    }                                                                                                                  \
  }

#define PLAY(filename, genclass)                                                                                       \
  if (!do_replay_file<genclass>(filename)) {                                                                           \
    std::cout << concolor::magenta << "Failed to pass test : " << #genclass << concolor::normal << std::endl;          \
    return 1;                                                                                                          \
  }

#define GENERATE_AND_PLAY(genclass)                                                                                    \
  {                                                                                                                    \
    std::vector<test_event_entry> events;                                                                              \
    ++tests_count;                                                                                                     \
    bool generated = false;                                                                                            \
    try {                                                                                                              \
      genclass g;                                                                                                      \
      generated = g.generate(events);                                                                                  \
      ;                                                                                                                \
    } catch (const std::exception& ex) {                                                                               \
      std::cout << #genclass << " generation failed: what=" << ex.what();                                              \
    } catch (...) {                                                                                                    \
      std::cout << #genclass << " generation failed: generic exception";                                               \
    }                                                                                                                  \
    genclass validator;                                                                                                \
    if (generated && do_replay_events<genclass>(events, validator)) {                                                  \
      std::cout << concolor::green << "#TEST# Succeeded " << #genclass << concolor::normal << '\n';                    \
    } else {                                                                                                           \
      std::cout << concolor::magenta << "#TEST# Failed " << #genclass << concolor::normal << '\n';                     \
      failed_tests.push_back(#genclass);                                                                               \
    }                                                                                                                  \
    std::cout << std::endl;                                                                                            \
  }

template <typename GenClassT> bool GenerateAndPlay(const char* testname, GenClassT&& g) {
  std::vector<test_event_entry> events;
  bool generated = false;

  try {
    generated = g.generate(events);
  } catch (const std::exception& ex) {
    std::cout << testname << " generation failed: what=" << ex.what();
  } catch (...) {
    std::cout << testname << " generation failed: generic exception";
  }

  bool succeeded = generated && do_replay_events<GenClassT>(events, g);

  if (succeeded) {
    std::cout << concolor::green << "#TEST# Succeeded " << testname << concolor::normal << '\n';
  } else {
    std::cout << concolor::magenta << "#TEST# Failed " << testname << concolor::normal << '\n';
  }

  std::cout << std::endl;
  return succeeded;
}

#define GENERATE_AND_PLAY_EX(genclass)                                                                                 \
  {                                                                                                                    \
    ++tests_count;                                                                                                     \
    if (!GenerateAndPlay(#genclass, genclass))                                                                         \
      failed_tests.push_back(#genclass);                                                                               \
  }

#define CALL_TEST(test_name, function)                                                                                 \
  {                                                                                                                    \
    if (!function()) {                                                                                                 \
      std::cout << concolor::magenta << "#TEST# Failed " << test_name << concolor::normal << std::endl;                \
      return 1;                                                                                                        \
    } else {                                                                                                           \
      std::cout << concolor::green << "#TEST# Succeeded " << test_name << concolor::normal << std::endl;               \
    }                                                                                                                  \
  }

template <uint64_t N> struct Pow10 { static const uint64_t value = 10 * Pow10<N - 1>::value; };

template <> struct Pow10<0> { static const uint64_t value = 1; };

const uint64_t COIN = Pow10<CryptoNote::parameters::CRYPTONOTE_DISPLAY_DECIMAL_POINT>::value;

#define QUOTEME(x) #x
#define DEFINE_TESTS_ERROR_CONTEXT(text) const char* perr_context = text;
#define CHECK_TEST_CONDITION(cond)                                                                                     \
  CHECK_AND_ASSERT_MES(cond, false, "[" << perr_context << "] failed: \"" << QUOTEME(cond) << "\"")
#define CHECK_EQ(v1, v2)                                                                                               \
  CHECK_AND_ASSERT_MES(v1 == v2, false, "[" << perr_context << "] failed: \"" << QUOTEME(v1) << " == " << QUOTEME(v2)  \
                                            << "\", " << v1 << " != " << v2)
#define CHECK_NOT_EQ(v1, v2)                                                                                           \
  CHECK_AND_ASSERT_MES(!(v1 == v2), false, "[" << perr_context << "] failed: \"" << QUOTEME(v1)                        \
                                               << " != " << QUOTEME(v2) << "\", " << v1 << " == " << v2)
#define MK_COINS(amount) (UINT64_C(amount) * COIN)
