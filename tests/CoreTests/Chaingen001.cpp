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

#include "Chaingen001.h"

using namespace std;
using namespace CryptoNote;

////////
// class one_block;

one_block::one_block()
{
  REGISTER_CALLBACK("verify_1", one_block::verify_1);
}

bool one_block::generate(std::vector<test_event_entry> &events)
{
    uint64_t ts_start = 1338224400;

    MAKE_GENESIS_BLOCK(events, blk_0, alice, ts_start);
    MAKE_ACCOUNT(events, alice);
    DO_CALLBACK(events, "verify_1");

    return true;
}

bool one_block::verify_1(CryptoNote::Core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
    DEFINE_TESTS_ERROR_CONTEXT("one_block::verify_1");

    alice = boost::get<CryptoNote::AccountBase>(events[1]);

    // check balances
    //std::vector<const CryptoNote::Block*> chain;
    //map_hash2tx_t mtx;
    //CHECK_TEST_CONDITION(find_block_chain(events, chain, mtx, getBlockHash(boost::get<CryptoNote::BlockTemplate>(events[1]))));
    //CHECK_TEST_CONDITION(get_block_reward(0) == get_balance(alice, events, chain, mtx));

    // check height
    std::vector<BlockTemplate> blocks;
    auto rawBlocks = c.getBlocks(0, 10000);
    blocks.resize(rawBlocks.size());
    for (size_t i = 0; i < rawBlocks.size(); ++i) {
      CHECK_TEST_CONDITION(fromBinaryArray(blocks[i], rawBlocks[i].block));
    }

    CHECK_TEST_CONDITION(blocks.size() == 1);
    //CHECK_TEST_CONDITION(outs.size() == blocks.size());
    CHECK_TEST_CONDITION(c.getBlockchainTransactionCount() == 1);
    //CHECK_TEST_CONDITION(blocks.back() == boost::get<CryptoNote::BlockTemplate>(events[0]));

    return true;
}


////////
// class gen_simple_chain_001;

gen_simple_chain_001::gen_simple_chain_001()
{
  REGISTER_CALLBACK("verify_callback_1", gen_simple_chain_001::verify_callback_1);
  REGISTER_CALLBACK("verify_callback_2", gen_simple_chain_001::verify_callback_2);
}

bool gen_simple_chain_001::generate(std::vector<test_event_entry> &events)
{
    uint64_t ts_start = 1338224400;

    GENERATE_ACCOUNT(miner);
    GENERATE_ACCOUNT(alice);

    MAKE_GENESIS_BLOCK(events, blk_0, miner, ts_start);
    MAKE_NEXT_BLOCK(events, blk_1, blk_0, miner);
    MAKE_NEXT_BLOCK(events, blk_1_side, blk_0, miner);
    MAKE_NEXT_BLOCK(events, blk_2, blk_1, miner);
    //MAKE_TX(events, tx_0, first_miner_account, alice, 151, blk_2);

    std::vector<CryptoNote::BlockTemplate> chain;
    map_hash2tx_t mtx;
    /*bool r = */find_block_chain(events, chain, mtx, getBlockHash(boost::get<CryptoNote::BlockTemplate>(events[3])));
    std::cout << "BALANCE = " << get_balance(miner, chain, mtx) << std::endl;

    REWIND_BLOCKS(events, blk_2r, blk_2, miner);
    MAKE_TX_LIST_START(events, txlist_0, miner, alice, MK_COINS(1), blk_2);
    MAKE_TX_LIST(events, txlist_0, miner, alice, MK_COINS(2), blk_2);
    MAKE_TX_LIST(events, txlist_0, miner, alice, MK_COINS(4), blk_2);
    MAKE_NEXT_BLOCK_TX_LIST(events, blk_3, blk_2r, miner, txlist_0);
    REWIND_BLOCKS(events, blk_3r, blk_3, miner);
    MAKE_TX(events, tx_1, miner, alice, MK_COINS(50), blk_3);
    MAKE_NEXT_BLOCK_TX1(events, blk_4, blk_3r, miner, tx_1);
    REWIND_BLOCKS(events, blk_4r, blk_4, miner);
    MAKE_TX(events, tx_2, miner, alice, MK_COINS(50), blk_4);
    MAKE_NEXT_BLOCK_TX1(events, blk_5, blk_4r, miner, tx_2);
    REWIND_BLOCKS(events, blk_5r, blk_5, miner);
    MAKE_TX(events, tx_3, miner, alice, MK_COINS(50), blk_5);
    MAKE_NEXT_BLOCK_TX1(events, blk_6, blk_5r, miner, tx_3);

    DO_CALLBACK(events, "verify_callback_1");
    //e.t.c.
    //MAKE_BLOCK_TX1(events, blk_3, 3, getBlockHash(blk_0), get_test_target(), first_miner_account, ts_start + 10, tx_0);
    //MAKE_BLOCK_TX1(events, blk_3, 3, getBlockHash(blk_0), get_test_target(), first_miner_account, ts_start + 10, tx_0);
    //DO_CALLBACK(events, "verify_callback_2");

/*    std::vector<const CryptoNote::Block*> chain;
    map_hash2tx_t mtx;
    if (!find_block_chain(events, chain, mtx, getBlockHash(blk_6)))
        throw;
    cout << "miner = " << get_balance(first_miner_account, events, chain, mtx) << endl;
    cout << "alice = " << get_balance(alice, events, chain, mtx) << endl;*/

    return true;
}

bool gen_simple_chain_001::verify_callback_1(CryptoNote::Core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  return true;
}

bool gen_simple_chain_001::verify_callback_2(CryptoNote::Core& c, size_t ev_index, const std::vector<test_event_entry> &events)
{
  return true;
}
