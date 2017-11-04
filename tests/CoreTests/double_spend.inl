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

//======================================================================================================================

template <class concrete_test>
gen_double_spend_base<concrete_test>::gen_double_spend_base()
    : m_invalid_tx_index(invalid_index_value), m_invalid_block_index(invalid_index_value) {
  REGISTER_CALLBACK_METHOD(gen_double_spend_base<concrete_test>, mark_last_valid_block);
  REGISTER_CALLBACK_METHOD(gen_double_spend_base<concrete_test>, mark_invalid_tx);
  REGISTER_CALLBACK_METHOD(gen_double_spend_base<concrete_test>, mark_invalid_block);
  REGISTER_CALLBACK_METHOD(gen_double_spend_base<concrete_test>, check_double_spend);
}

template <class concrete_test>
bool gen_double_spend_base<concrete_test>::check_tx_verification_context(bool tve, bool tx_added, size_t event_idx,
                                                                         const CryptoNote::Transaction& /*tx*/) {
  //std::cout << "invIndex: " << m_invalid_tx_index << " id: " << event_idx << " tve: " << tve << " tx_added: " << tx_added << std::endl;
  if (m_invalid_tx_index == event_idx)
    return !tve || !tx_added; // FIXME: correct???
  else
    return tve && tx_added;
}

template <class concrete_test>
bool gen_double_spend_base<concrete_test>::check_block_verification_context(
    std::error_code bve, size_t event_idx, const CryptoNote::BlockTemplate& /*block*/) {
  return (m_invalid_block_index == event_idx) ? blockWasNotAdded(bve) : blockWasAdded(bve);
}

template <class concrete_test>
bool gen_double_spend_base<concrete_test>::check_block_verification_context(std::error_code bve, size_t event_idx,
                                                                            const CryptoNote::RawBlock& /*block*/) {
  return (m_invalid_block_index == event_idx) ? blockWasNotAdded(bve) : blockWasAdded(bve);
}

template <class concrete_test>
bool gen_double_spend_base<concrete_test>::mark_last_valid_block(CryptoNote::Core& c, size_t /*ev_index*/,
                                                                 const std::vector<test_event_entry>& /*events*/) {
  std::vector<CryptoNote::BlockTemplate> blocks;
  auto rawBlocks = c.getBlocks(c.getTopBlockIndex(), 1);
  assert(rawBlocks.size() == 1);
  blocks.resize(rawBlocks.size());
  for (size_t i = 0; i < rawBlocks.size(); ++i) {
    CHECK_AND_ASSERT_MES(fromBinaryArray(blocks[i], rawBlocks[i].block), false,
                         "Failed to deserealize blocks from core");
    std::cout << CryptoNote::CachedBlock(blocks[i]).getBlockHash() << std::endl;
  }

  m_last_valid_block = blocks.back();
  return true;
}

template <class concrete_test>
bool gen_double_spend_base<concrete_test>::mark_invalid_tx(CryptoNote::Core& /*c*/, size_t ev_index,
                                                           const std::vector<test_event_entry>& /*events*/) {
  m_invalid_tx_index = ev_index + 1;
  return true;
}

template <class concrete_test>
bool gen_double_spend_base<concrete_test>::mark_invalid_block(CryptoNote::Core& /*c*/, size_t ev_index,
                                                              const std::vector<test_event_entry>& /*events*/) {
  m_invalid_block_index = ev_index + 1;
  return true;
}

template <class concrete_test>
bool gen_double_spend_base<concrete_test>::check_double_spend(CryptoNote::Core& c, size_t /*ev_index*/,
                                                              const std::vector<test_event_entry>& events) {
  using std::cout;
  using std::endl;
  using CryptoNote::CachedBlock;
  DEFINE_TESTS_ERROR_CONTEXT("gen_double_spend_base::check_double_spend");

  /* if (concrete_test::has_invalid_tx) { */
  /*   CHECK_NOT_EQ(invalid_index_value, m_invalid_tx_index); */
  /* } */
  CHECK_NOT_EQ(invalid_index_value, m_invalid_block_index);

  std::vector<CryptoNote::BlockTemplate> blocks;
  auto rawBlocks = c.getBlocks(0, 100 + 2 * m_currency->minedMoneyUnlockWindow());
  blocks.resize(rawBlocks.size());
  for (size_t i = 0; i < rawBlocks.size(); ++i) {
    CHECK_TEST_CONDITION(fromBinaryArray(blocks[i], rawBlocks[i].block));
  }

  //cout << CryptoNote::CachedBlock(m_last_valid_block).getBlockHash() << endl;
  //cout << endl;
  //for (auto _ : blocks) cout << CryptoNote::CachedBlock(_).getBlockHash() << endl;
  CHECK_TEST_CONDITION(CachedBlock(m_last_valid_block).getBlockHash() == CachedBlock(blocks.back()).getBlockHash());

  CHECK_EQ(concrete_test::expected_pool_txs_count, c.getPoolTransactionCount());

  CryptoNote::AccountBase bob_account = boost::get<CryptoNote::AccountBase>(events[2]);
  CryptoNote::AccountBase alice_account = boost::get<CryptoNote::AccountBase>(events[3]);

  std::vector<CryptoNote::BlockTemplate> chain;
  map_hash2tx_t mtx;
  bool r = find_block_chain(events, chain, mtx, getBlockHash(blocks.back()));
  CHECK_TEST_CONDITION(r);
  CHECK_EQ(concrete_test::expected_bob_balance, get_balance(bob_account, blocks, mtx));
  CHECK_EQ(concrete_test::expected_alice_balance, get_balance(alice_account, blocks, mtx));

  return true;
}

//======================================================================================================================

template <bool txs_keeped_by_block>
bool gen_double_spend_in_tx<txs_keeped_by_block>::generate(std::vector<test_event_entry>& events) const {
  INIT_DOUBLE_SPEND_TEST();
  DO_CALLBACK(events, "mark_last_valid_block");

  std::vector<CryptoNote::TransactionSourceEntry> sources;
  CryptoNote::TransactionSourceEntry se;
  se.amount = tx_0.outputs[0].amount;
  se.outputs.push_back(std::make_pair(0, boost::get<CryptoNote::KeyOutput>(tx_0.outputs[0].target).key));
  se.realOutput = 0;
  se.realTransactionPublicKey = CryptoNote::getTransactionPublicKeyFromExtra(tx_0.extra);
  se.realOutputIndexInTransaction = 0;
  sources.push_back(se);
  // Double spend!
  sources.push_back(se);

  CryptoNote::TransactionDestinationEntry de;
  de.addr = alice_account.getAccountKeys().address;
  de.amount = 2 * se.amount - this->m_currency->minimumFee();
  std::vector<CryptoNote::TransactionDestinationEntry> destinations;
  destinations.push_back(de);

  CryptoNote::Transaction tx_1;
  if (!constructTransaction(bob_account.getAccountKeys(), sources, destinations, std::vector<uint8_t>(), tx_1, 0,
                            this->m_logger))
    return false;

  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, txs_keeped_by_block);
  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(tx_1);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1r, miner_account, tx_1);
  DO_CALLBACK(events, "check_double_spend");

  return true;
}

template <bool txs_keeped_by_block>
bool gen_double_spend_in_the_same_block<txs_keeped_by_block>::generate(std::vector<test_event_entry>& events) const {
  INIT_DOUBLE_SPEND_TEST();

  DO_CALLBACK(events, "mark_last_valid_block");
  //SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, txs_keeped_by_block);

  MAKE_TX_LIST_START(events, txs_1, bob_account, alice_account, send_amount - this->m_currency->minimumFee(), blk_1);
  CryptoNote::Transaction tx_1 = txs_1.front();
  auto tx_1_idx = events.size() - 1;
  // Remove tx_1, it is being inserted back a little later
  events.pop_back();

  // keept by block flag no longer exists, so this tx is always invalid
  DO_CALLBACK(events, "mark_invalid_tx");
  MAKE_TX_LIST(events, txs_1, bob_account, alice_account, send_amount - this->m_currency->minimumFee(), blk_1);
  events.insert(events.begin() + tx_1_idx, tx_1);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_2, blk_1r, miner_account, txs_1);
  DO_CALLBACK(events, "check_double_spend");

  return true;
}

template <bool txs_keeped_by_block>
bool gen_double_spend_in_different_blocks<txs_keeped_by_block>::generate(std::vector<test_event_entry>& events) const {
  INIT_DOUBLE_SPEND_TEST();

  DO_CALLBACK(events, "mark_last_valid_block");
  //SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, txs_keeped_by_block);

  // Create two identical transactions, but don't push it to events list
  MAKE_TX(events, tx_blk_2, bob_account, alice_account, send_amount - this->m_currency->minimumFee(), blk_1);
  events.pop_back();
  MAKE_TX(events, tx_blk_3, bob_account, alice_account, send_amount - this->m_currency->minimumFee(), blk_1);
  events.pop_back();

  //events.push_back(tx_blk_2);
  MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1r, miner_account, tx_blk_2);
  DO_CALLBACK(events, "mark_last_valid_block");

  //if (has_invalid_tx) {
  // keept by block flag no longer exists, so this tx is always invalid
  //DO_CALLBACK(events, "mark_invalid_tx");
  //}
  //events.push_back(tx_blk_3);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_2, miner_account, tx_blk_3);

  DO_CALLBACK(events, "check_double_spend");

  return true;
}

template <bool txs_keeped_by_block>
bool gen_double_spend_in_alt_chain_in_the_same_block<txs_keeped_by_block>::generate(
    std::vector<test_event_entry>& events) const {
  INIT_DOUBLE_SPEND_TEST();

  //SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, txs_keeped_by_block);

  // Main chain
  MAKE_NEXT_BLOCK(events, blk_2, blk_1r, miner_account);
  DO_CALLBACK(events, "mark_last_valid_block");

  // Alt chain
  MAKE_TX_LIST_START(events, txs_1, bob_account, alice_account, send_amount - this->m_currency->minimumFee(), blk_1);
  CryptoNote::Transaction tx_1 = txs_1.front();
  auto tx_1_idx = events.size() - 1;
  // Remove tx_1, it is being inserted back a little later
  events.pop_back();

  if (has_invalid_tx) {
    DO_CALLBACK(events, "mark_invalid_tx");
  }
  MAKE_TX_LIST(events, txs_1, bob_account, alice_account, send_amount - this->m_currency->minimumFee(), blk_1);
  events.insert(events.begin() + tx_1_idx, tx_1);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_3, blk_1r, miner_account, txs_1);

  // Try to switch to alternative chain
  //MAKE_NEXT_BLOCK(events, blk_4, blk_3, miner_account);

  DO_CALLBACK(events, "check_double_spend");

  return true;
}

template <bool txs_keeped_by_block>
bool gen_double_spend_in_alt_chain_in_different_blocks<txs_keeped_by_block>::generate(
    std::vector<test_event_entry>& events) const {
  INIT_DOUBLE_SPEND_TEST();

  SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, txs_keeped_by_block);

  // Main chain
  MAKE_NEXT_BLOCK(events, blk_2, blk_1r, miner_account);
  DO_CALLBACK(events, "mark_last_valid_block");

  // Alternative chain
  MAKE_TX(events, tx_1, bob_account, alice_account, send_amount - this->m_currency->minimumFee(), blk_1);
  events.pop_back();
  MAKE_TX(events, tx_2, bob_account, alice_account, send_amount - this->m_currency->minimumFee(), blk_1);
  events.pop_back();

  events.push_back(tx_1);
  MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_1r, miner_account, tx_1);

  // Try to switch to alternative chain
  if (has_invalid_tx) {
    DO_CALLBACK(events, "mark_invalid_tx");
  }
  events.push_back(tx_2);
  DO_CALLBACK(events, "mark_invalid_block");
  MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3, miner_account, tx_2);

  DO_CALLBACK(events, "check_double_spend");

  return true;
}
