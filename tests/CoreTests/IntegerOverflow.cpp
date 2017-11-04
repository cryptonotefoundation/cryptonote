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

#include "IntegerOverflow.h"

using namespace CryptoNote;

namespace {
void split_miner_tx_outs(Transaction& miner_tx, uint64_t amount_1) {
  uint64_t total_amount = getSummaryOutsAmount(miner_tx);
  uint64_t amount_2 = total_amount - amount_1;
  TransactionOutputTarget target = miner_tx.outputs[0].target;

  miner_tx.outputs.clear();

  TransactionOutput out1;
  out1.amount = amount_1;
  out1.target = target;
  miner_tx.outputs.push_back(out1);

  TransactionOutput out2;
  out2.amount = amount_2;
  out2.target = target;
  miner_tx.outputs.push_back(out2);
}

void append_TransactionSourceEntry(std::vector<CryptoNote::TransactionSourceEntry>& sources, const Transaction& tx,
                                   size_t out_idx) {
  CryptoNote::TransactionSourceEntry se;
  se.amount = tx.outputs[out_idx].amount;
  se.outputs.push_back(std::make_pair(0, boost::get<CryptoNote::KeyOutput>(tx.outputs[out_idx].target).key));
  se.realOutput = 0;
  se.realTransactionPublicKey = getTransactionPublicKeyFromExtra(tx.extra);
  se.realOutputIndexInTransaction = out_idx;

  sources.push_back(se);
}
}

//======================================================================================================================

gen_uint_overflow_base::gen_uint_overflow_base() : m_last_valid_block_event_idx(static_cast<size_t>(-1)) {
  REGISTER_CALLBACK_METHOD(gen_uint_overflow_1, mark_last_valid_block);
}

bool gen_uint_overflow_base::check_tx_verification_context(bool tve, bool tx_added, size_t event_idx,
                                                           const CryptoNote::Transaction& /*tx*/) {
  return event_idx <= m_last_valid_block_event_idx ? tx_added && tve : !tx_added && !tve;
}

bool gen_uint_overflow_base::check_block_verification_context(std::error_code bve, size_t event_idx,
                                                              const CryptoNote::BlockTemplate& /*block*/) {
  return event_idx <= m_last_valid_block_event_idx ? blockWasAdded(bve) : blockWasNotAdded(bve);
}

bool gen_uint_overflow_base::check_block_verification_context(std::error_code bve, size_t event_idx,
                                                              const CryptoNote::RawBlock& /*block*/) {
  return check_block_verification_context(bve, event_idx, CryptoNote::BlockTemplate{});
}

bool gen_uint_overflow_base::mark_last_valid_block(CryptoNote::Core& c, size_t ev_index,
                                                   const std::vector<test_event_entry>& events) {
  m_last_valid_block_event_idx = ev_index - 1;
  return true;
}

//======================================================================================================================

bool gen_uint_overflow_1::generate(std::vector<test_event_entry>& events) const {
  //uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  MAKE_NEXT_BLOCK(events, blk_0f, blk_0, miner_account);
  DO_CALLBACK(events, "mark_last_valid_block");
  MAKE_ACCOUNT(events, bob_account);
  MAKE_ACCOUNT(events, alice_account);

  // Problem 1. Miner tx output overflow
  MAKE_MINER_TX_MANUALLY(miner_tx_0, blk_0f);
  split_miner_tx_outs(miner_tx_0, m_currency->moneySupply());
  BlockTemplate blk_1;
  if (!generator.constructBlockManually(blk_1, blk_0f, miner_account, test_generator::bf_miner_tx, 0, 0, 0,
                                        Crypto::Hash(), 0, miner_tx_0))
    return false;
  events.push_back(blk_1);

  // Problem 1. Miner tx outputs overflow
  MAKE_MINER_TX_MANUALLY(miner_tx_1, blk_1);
  split_miner_tx_outs(miner_tx_1, m_currency->moneySupply());
  BlockTemplate blk_2;
  if (!generator.constructBlockManually(blk_2, blk_1, miner_account, test_generator::bf_miner_tx, 0, 0, 0,
                                        Crypto::Hash(), 0, miner_tx_1))
    return false;
  events.push_back(blk_2);

  REWIND_BLOCKS(events, blk_2r, blk_2, miner_account);
  MAKE_TX_LIST_START(events, txs_0, miner_account, bob_account, m_currency->moneySupply(), blk_2);
  MAKE_TX_LIST(events, txs_0, miner_account, bob_account, m_currency->moneySupply(), blk_2);
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_3, blk_2r, miner_account, txs_0);
  REWIND_BLOCKS(events, blk_3r, blk_3, miner_account);

  // Problem 2. total_fee overflow, block_reward overflow
  std::list<CryptoNote::Transaction> txs_1;
  // Create txs with huge fee
  txs_1.push_back(construct_tx_with_fee(m_logger, events, blk_3, bob_account, alice_account, MK_COINS(1),
                                        m_currency->moneySupply() - MK_COINS(1)));
  txs_1.push_back(construct_tx_with_fee(m_logger, events, blk_3, bob_account, alice_account, MK_COINS(1),
                                        m_currency->moneySupply() - MK_COINS(1)));
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_4, blk_3r, miner_account, txs_1);

  return true;
}

//======================================================================================================================

bool gen_uint_overflow_2::generate(std::vector<test_event_entry>& events) const {
  //uint64_t ts_start = 1338224400;

  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  MAKE_NEXT_BLOCK(events, blk_0f, blk_0, miner_account);
  MAKE_ACCOUNT(events, bob_account);
  MAKE_ACCOUNT(events, alice_account);
  REWIND_BLOCKS(events, blk_0r, blk_0f, miner_account);
  DO_CALLBACK(events, "mark_last_valid_block");

  // Problem 1. Regular tx outputs overflow
  std::vector<CryptoNote::TransactionSourceEntry> sources;
  for (size_t i = 0; i < blk_0f.baseTransaction.outputs.size(); ++i) {
    if (m_currency->minimumFee() < blk_0f.baseTransaction.outputs[i].amount) {
      append_TransactionSourceEntry(sources, blk_0f.baseTransaction, i);
      break;
    }
  }
  if (sources.empty()) {
    return false;
  }

  std::vector<CryptoNote::TransactionDestinationEntry> destinations;
  const AccountPublicAddress& bob_addr = bob_account.getAccountKeys().address;
  destinations.push_back(TransactionDestinationEntry(m_currency->moneySupply(), bob_addr));
  destinations.push_back(TransactionDestinationEntry(m_currency->moneySupply() - 1, bob_addr));
  // sources.front().amount = destinations[0].amount + destinations[2].amount + destinations[3].amount +
  // m_currency->minimumFee()
  destinations.push_back(TransactionDestinationEntry(sources.front().amount - m_currency->moneySupply() -
                                                         m_currency->moneySupply() + 1 - m_currency->minimumFee(),
                                                     bob_addr));

  CryptoNote::Transaction tx_1;
  if (!constructTransaction(miner_account.getAccountKeys(), sources, destinations, std::vector<uint8_t>(), tx_1, 0,
                            m_logger))
    return false;
  events.push_back(tx_1);

  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0r, miner_account, tx_1);
  REWIND_BLOCKS(events, blk_1r, blk_1, miner_account);

  // Problem 2. Regular tx inputs overflow
  sources.clear();
  for (size_t i = 0; i < tx_1.outputs.size(); ++i) {
    auto& tx_1_out = tx_1.outputs[i];
    if (tx_1_out.amount < m_currency->moneySupply() - 1)
      continue;

    append_TransactionSourceEntry(sources, tx_1, i);
  }

  destinations.clear();
  CryptoNote::TransactionDestinationEntry de;
  de.addr = alice_account.getAccountKeys().address;
  de.amount = m_currency->moneySupply() - m_currency->minimumFee();
  destinations.push_back(de);
  destinations.push_back(de);

  CryptoNote::Transaction tx_2;
  if (!constructTransaction(bob_account.getAccountKeys(), sources, destinations, std::vector<uint8_t>(), tx_2, 0,
                            m_logger))
    return false;
  events.push_back(tx_2);

  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1r, miner_account, tx_2);

  return true;
}
