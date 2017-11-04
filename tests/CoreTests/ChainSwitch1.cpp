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

#include "ChainSwitch1.h"

using namespace CryptoNote;

gen_chain_switch_1::gen_chain_switch_1() {
  REGISTER_CALLBACK("check_split_not_switched", gen_chain_switch_1::check_split_not_switched);
  REGISTER_CALLBACK("check_split_switched", gen_chain_switch_1::check_split_switched);
}

//-----------------------------------------------------------------------------------------------------
bool gen_chain_switch_1::generate(std::vector<test_event_entry>& events) const {
  /*
  (0 )-(1 )-(2 ) -(3 )-(4 )                  <- main chain, until 7 isn't connected
              \ |-(5 )-(6 )-(7 )|            <- alt chain, until 7 isn't connected

  transactions ([n] - tx amount, (m) - block):
  (1)     : miner -[ 5]-> account_1 ( +5 in main chain,  +5 in alt chain)
  (3)     : miner -[ 7]-> account_2 ( +7 in main chain,  +0 in alt chain), tx will be in tx pool after switch
  (4), (6): miner -[11]-> account_3 (+11 in main chain, +11 in alt chain)
  (5)     : miner -[13]-> account_4 ( +0 in main chain, +13 in alt chain), tx will be in tx pool before switch

  transactions orders ([n] - tx amount, (m) - block):
  miner -[1], [2]-> account_1: in main chain (3), (3), in alt chain (5), (6)
  miner -[1], [2]-> account_2: in main chain (3), (4), in alt chain (5), (5)
  miner -[1], [2]-> account_3: in main chain (3), (4), in alt chain (6), (5)
  miner -[1], [2]-> account_4: in main chain (4), (3), in alt chain (5), (6)
  */

  GENERATE_ACCOUNT(miner_account);

  //                                                                                              events
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);                     //  0
  MAKE_NEXT_BLOCK(events, blk_0f, blk_0, miner_account);                     //  1
  MAKE_ACCOUNT(events, recipient_account_1);                                      //  2
  MAKE_ACCOUNT(events, recipient_account_2);                                      //  3
  MAKE_ACCOUNT(events, recipient_account_3);                                      //  4
  MAKE_ACCOUNT(events, recipient_account_4);                                      //  5
  REWIND_BLOCKS(events, blk_0r, blk_0f, miner_account)                             // <N blocks>
  MAKE_TX(events, tx_00, miner_account, recipient_account_1, MK_COINS(5), blk_0f); //  6 + N
  MAKE_NEXT_BLOCK_TX1(events, blk_1, blk_0r, miner_account, tx_00);               //  7 + N
  MAKE_NEXT_BLOCK(events, blk_2, blk_1, miner_account);                           //  8 + N
  REWIND_BLOCKS(events, blk_2r, blk_2, miner_account)                             // <N blocks>

  // Transactions to test account balances after switch
  MAKE_TX_LIST_START(events, txs_blk_3, miner_account, recipient_account_2, MK_COINS(7), blk_2);  //  9 + 2N
  MAKE_TX_LIST_START(events, txs_blk_4, miner_account, recipient_account_3, MK_COINS(11), blk_2); //  10 + 2N
  MAKE_TX_LIST_START(events, txs_blk_5, miner_account, recipient_account_4, MK_COINS(13), blk_2); // 11 + 2N
  std::list<Transaction> txs_blk_6;
  txs_blk_6.push_back(txs_blk_4.front());

  // Transactions, that has different order in alt block chains
  MAKE_TX_LIST(events, txs_blk_3, miner_account, recipient_account_1, MK_COINS(1), blk_2); // 12 + 2N
  txs_blk_5.push_back(txs_blk_3.back());
  MAKE_TX_LIST(events, txs_blk_3, miner_account, recipient_account_1, MK_COINS(2), blk_2); // 13 + 2N
  txs_blk_6.push_back(txs_blk_3.back());

  MAKE_TX_LIST(events, txs_blk_3, miner_account, recipient_account_2, MK_COINS(1), blk_2); // 14 + 2N
  txs_blk_5.push_back(txs_blk_3.back());
  MAKE_TX_LIST(events, txs_blk_4, miner_account, recipient_account_2, MK_COINS(2), blk_2); // 15 + 2N
  txs_blk_5.push_back(txs_blk_4.back());

  MAKE_TX_LIST(events, txs_blk_3, miner_account, recipient_account_3, MK_COINS(1), blk_2); // 16 + 2N
  txs_blk_6.push_back(txs_blk_3.back());
  MAKE_TX_LIST(events, txs_blk_4, miner_account, recipient_account_3, MK_COINS(2), blk_2); // 17 + 2N
  txs_blk_5.push_back(txs_blk_4.back());

  MAKE_TX_LIST(events, txs_blk_4, miner_account, recipient_account_4, MK_COINS(1), blk_2); // 18 + 2N
  txs_blk_5.push_back(txs_blk_4.back());
  MAKE_TX_LIST(events, txs_blk_3, miner_account, recipient_account_4, MK_COINS(2), blk_2); // 19 + 2N
  txs_blk_6.push_back(txs_blk_3.back());

  MAKE_NEXT_BLOCK_TX_LIST(events, blk_3, blk_2r, miner_account, txs_blk_3); // 20 + 2N
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_4, blk_3, miner_account, txs_blk_4);  // 21 + 2N
  // split
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_5, blk_2r, miner_account, txs_blk_5); // 22 + 2N
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_6, blk_5, miner_account, txs_blk_6);  // 23 + 2N
  DO_CALLBACK(events, "check_split_not_switched");                          // 21 + 2N
  MAKE_NEXT_BLOCK(events, blk_7, blk_6, miner_account);                     // 24 + 2N
  DO_CALLBACK(events, "check_split_switched");                              // 25 + 2N

  return true;
}

//-----------------------------------------------------------------------------------------------------
bool gen_chain_switch_1::check_split_not_switched(CryptoNote::Core& c, size_t ev_index,
                                                  const std::vector<test_event_entry>& events) {
  DEFINE_TESTS_ERROR_CONTEXT("gen_chain_switch_1::check_split_not_switched");

  m_recipient_account_1 = boost::get<AccountBase>(events[2]);
  m_recipient_account_2 = boost::get<AccountBase>(events[3]);
  m_recipient_account_3 = boost::get<AccountBase>(events[4]);
  m_recipient_account_4 = boost::get<AccountBase>(events[5]);

  std::vector<BlockTemplate> blocks;
  auto rawBlocks = c.getBlocks(0, 10000);
  blocks.resize(rawBlocks.size());
  for (size_t i = 0; i < rawBlocks.size(); ++i) {
    CHECK_TEST_CONDITION(fromBinaryArray(blocks[i], rawBlocks[i].block));
  }

  CHECK_EQ(6 + 2 * m_currency->minedMoneyUnlockWindow(), blocks.size());
  auto blk_4 = CryptoNote::fromBinaryArray<BlockTemplate>(
      boost::get<RawBlock>(events[21 + 2 * m_currency->minedMoneyUnlockWindow()]).block);
  CHECK_TEST_CONDITION(getBlockHash(blocks.back()) == getBlockHash(blk_4)); // blk_4

  CHECK_EQ(2, c.getAlternativeBlockCount());

  std::vector<CryptoNote::BlockTemplate> chain;
  map_hash2tx_t mtx;
  bool r = find_block_chain(events, chain, mtx, getBlockHash(blocks.back()));
  CHECK_TEST_CONDITION(r);
  CHECK_EQ(MK_COINS(8), get_balance(m_recipient_account_1, chain, mtx));
  CHECK_EQ(MK_COINS(10), get_balance(m_recipient_account_2, chain, mtx));
  CHECK_EQ(MK_COINS(14), get_balance(m_recipient_account_3, chain, mtx));
  CHECK_EQ(MK_COINS(3), get_balance(m_recipient_account_4, chain, mtx));

  auto tx_pool = c.getPoolTransactions();
  CHECK_EQ(1, tx_pool.size());

  std::vector<size_t> tx_outs;
  uint64_t transfered;
  lookup_acc_outs(m_recipient_account_4.getAccountKeys(), tx_pool.front(),
                  getTransactionPublicKeyFromExtra(tx_pool.front().extra), tx_outs, transfered);
  CHECK_EQ(MK_COINS(13), transfered);

  m_chain_1.swap(blocks);
  m_tx_pool.swap(tx_pool);

  return true;
}

//-----------------------------------------------------------------------------------------------------
bool gen_chain_switch_1::check_split_switched(CryptoNote::Core& c, size_t ev_index,
                                              const std::vector<test_event_entry>& events) {
  DEFINE_TESTS_ERROR_CONTEXT("gen_chain_switch_1::check_split_switched");

  std::vector<BlockTemplate> blocks;
  auto rawBlocks = c.getBlocks(0, 10000);
  blocks.resize(rawBlocks.size());
  for (size_t i = 0; i < rawBlocks.size(); ++i) {
    CHECK_TEST_CONDITION(fromBinaryArray(blocks[i], rawBlocks[i].block));
  }

  CHECK_EQ(7 + 2 * m_currency->minedMoneyUnlockWindow(), blocks.size());
  auto it = blocks.end();
  --it;
  --it;
  --it;
  CHECK_TEST_CONDITION(std::equal(blocks.begin(), it, m_chain_1.begin()));
  CHECK_TEST_CONDITION(blocks.back() ==
                       boost::get<BlockTemplate>(events[25 + 2 * m_currency->minedMoneyUnlockWindow()])); // blk_7

  auto alt_blocks = c.getAlternativeBlocks();
  CHECK_EQ(2, c.getAlternativeBlockCount());

  // Some blocks that were in main chain are in alt chain now
  for (const auto& b : alt_blocks) {
    CHECK_TEST_CONDITION(
        !(m_chain_1.end() == std::find_if(m_chain_1.cbegin(), m_chain_1.cend(),
                                          [&b](const CryptoNote::BlockTemplate& arg) -> bool { return b == arg; })));
  }

  std::vector<CryptoNote::BlockTemplate> chain;
  map_hash2tx_t mtx;
  bool r = find_block_chain(events, chain, mtx, getBlockHash(blocks.back()));
  CHECK_TEST_CONDITION(r);
  CHECK_EQ(MK_COINS(8), get_balance(m_recipient_account_1, chain, mtx));
  CHECK_EQ(MK_COINS(3), get_balance(m_recipient_account_2, chain, mtx));
  CHECK_EQ(MK_COINS(14), get_balance(m_recipient_account_3, chain, mtx));
  CHECK_EQ(MK_COINS(16), get_balance(m_recipient_account_4, chain, mtx));

  auto tx_pool = c.getPoolTransactions();
  CHECK_EQ(1, tx_pool.size());
  CHECK_TEST_CONDITION(!(tx_pool.front() == m_tx_pool.front()));

  std::vector<size_t> tx_outs;
  uint64_t transfered;
  lookup_acc_outs(m_recipient_account_2.getAccountKeys(), tx_pool.front(), tx_outs, transfered);
  CHECK_EQ(MK_COINS(7), transfered);

  return true;
}
