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

#include "BlockReward.h"

#include <numeric>

#include "CryptoNoteCore/CryptoNoteTools.h"
#include <Common/Math.h>

using namespace CryptoNote;

namespace {
bool rewind_blocks(std::vector<test_event_entry>& events, test_generator& generator, BlockTemplate& blk,
                   const BlockTemplate& blk_prev, const AccountBase& miner_account, size_t block_count) {
  blk = blk_prev;
  for (size_t i = 0; i < block_count; ++i) {
    BlockTemplate blk_i;
    if (!generator.constructMaxSizeBlock(blk_i, blk, miner_account))
      return false;

    events.push_back(blk_i);
    blk = blk_i;
  }

  return true;
}

uint64_t get_tx_out_amount(const Transaction& tx) {
  return std::accumulate(tx.outputs.begin(), tx.outputs.end(), uint64_t(0),
                         [](uint64_t sum, const TransactionOutput& o) { return sum + o.amount; });
}
}

gen_block_reward::gen_block_reward() : m_invalid_block_index(0) {
  CryptoNote::CurrencyBuilder currencyBuilder(m_logger);
  currencyBuilder.maxBlockSizeInitial(std::numeric_limits<size_t>::max() / 2);
  m_currency.reset(new Currency(currencyBuilder.currency()));
  const_cast<CryptoNote::BlockTemplate&>(m_currency->genesisBlock()).timestamp = 1338224400;

  REGISTER_CALLBACK_METHOD(gen_block_reward, mark_invalid_block);
  REGISTER_CALLBACK_METHOD(gen_block_reward, mark_checked_block);
  REGISTER_CALLBACK_METHOD(gen_block_reward, check_block_rewards);
}

bool gen_block_reward::generate(std::vector<test_event_entry>& events) const {
  GENERATE_ACCOUNT(miner_account);
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, ts_start);
  DO_CALLBACK(events, "mark_checked_block");
  MAKE_ACCOUNT(events, bob_account);

  // Test case 1: miner transactions without outputs (block reward == 0)
  BlockTemplate blk_0r;
  if (!rewind_blocks(events, generator, blk_0r, blk_0, miner_account, m_currency->rewardBlocksWindow())) {
    return false;
  }

  // Test: block reward is calculated using median of the latest m_currency.rewardBlocksWindow() blocks
  DO_CALLBACK(events, "mark_invalid_block");
  BlockTemplate blk_1_bad_1;
  if (!generator.constructMaxSizeBlock(blk_1_bad_1, blk_0r, miner_account, m_currency->rewardBlocksWindow() + 1)) {
    return false;
  }
  events.push_back(blk_1_bad_1);

  DO_CALLBACK(events, "mark_invalid_block");
  BlockTemplate blk_1_bad_2;
  if (!generator.constructMaxSizeBlock(blk_1_bad_2, blk_0r, miner_account, m_currency->rewardBlocksWindow() - 1)) {
    return false;
  }
  events.push_back(blk_1_bad_2);

  // Test 1.2: miner transactions without outputs (block reward == 0)
  BlockTemplate blk_1;
  if (!generator.constructMaxSizeBlock(blk_1, blk_0r, miner_account)) {
    return false;
  }
  events.push_back(blk_1);
  // End of Test case 1

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  MAKE_NEXT_BLOCK(events, blk_2, blk_1, miner_account);
  DO_CALLBACK(events, "mark_checked_block");
  MAKE_NEXT_BLOCK(events, blk_3, blk_2, miner_account);
  DO_CALLBACK(events, "mark_checked_block");
  MAKE_NEXT_BLOCK(events, blk_4, blk_3, miner_account);
  DO_CALLBACK(events, "mark_checked_block");
  MAKE_NEXT_BLOCK(events, blk_5, blk_4, miner_account);
  DO_CALLBACK(events, "mark_checked_block");

  BlockTemplate blk_5r;
  if (!rewind_blocks(events, generator, blk_5r, blk_5, miner_account, m_currency->minedMoneyUnlockWindow()))
    return false;

  // Test: fee increases block reward
  Transaction tx_0(construct_tx_with_fee(m_logger, events, blk_5, miner_account, bob_account, MK_COINS(1),
                                         3 * m_currency->minimumFee()));
  MAKE_NEXT_BLOCK_TX1(events, blk_6, blk_5r, miner_account, tx_0);
  DO_CALLBACK(events, "mark_checked_block");

  // Test: fee from all block transactions increase block reward
  std::list<Transaction> txs_0;
  txs_0.push_back(construct_tx_with_fee(m_logger, events, blk_5, miner_account, bob_account, MK_COINS(1),
                                        5 * m_currency->minimumFee()));
  txs_0.push_back(construct_tx_with_fee(m_logger, events, blk_5, miner_account, bob_account, MK_COINS(1),
                                        7 * m_currency->minimumFee()));
  MAKE_NEXT_BLOCK_TX_LIST(events, blk_7, blk_6, miner_account, txs_0);
  DO_CALLBACK(events, "mark_checked_block");

  // Test: block reward == transactions fee
  {
    Transaction tx_1 = construct_tx_with_fee(m_logger, events, blk_5, miner_account, bob_account, MK_COINS(1),
                                             11 * m_currency->minimumFee());
    Transaction tx_2 = construct_tx_with_fee(m_logger, events, blk_5, miner_account, bob_account, MK_COINS(1),
                                             13 * m_currency->minimumFee());
    size_t txs_1_size = getObjectBinarySize(tx_1) + getObjectBinarySize(tx_2);
    uint64_t txs_fee = get_tx_fee(tx_1) + get_tx_fee(tx_2);

    std::vector<size_t> block_sizes;
    generator.getLastNBlockSizes(block_sizes, getBlockHash(blk_7), m_currency->rewardBlocksWindow());
    size_t median = Common::medianValue(block_sizes);

    Transaction miner_tx;
    bool r = constructMinerTxBySize(*m_currency, miner_tx, blk_7.majorVersion, CachedBlock(blk_7).getBlockIndex() + 1,
                                    generator.getAlreadyGeneratedCoins(blk_7), miner_account.getAccountKeys().address,
                                    block_sizes, 2 * median - txs_1_size, 2 * median, txs_fee);
    if (!r)
      return false;

    std::vector<Crypto::Hash> txs_1_hashes;
    txs_1_hashes.push_back(getObjectHash(tx_1));
    txs_1_hashes.push_back(getObjectHash(tx_2));

    BlockTemplate blk_8;
    generator.constructBlockManually(blk_8, blk_7, miner_account,
                                     test_generator::bf_miner_tx | test_generator::bf_tx_hashes, 0, 0, 0,
                                     Crypto::Hash(), 0, miner_tx, txs_1_hashes, txs_1_size, txs_fee);

    events.push_back(populateBlock(blk_8, {tx_1, tx_2}));
    DO_CALLBACK(events, "mark_checked_block");
  }

  DO_CALLBACK(events, "check_block_rewards");

  return true;
}

bool gen_block_reward::check_block_verification_context(std::error_code bve, size_t event_idx,
                                                        const CryptoNote::RawBlock& /*blk*/) {
  return check_block_verification_context(bve, event_idx, CryptoNote::BlockTemplate{});
}

bool gen_block_reward::check_block_verification_context(std::error_code bve, size_t event_idx,
                                                        const CryptoNote::BlockTemplate& /*blk*/) {
  if (m_invalid_block_index == event_idx) {
    m_invalid_block_index = 0;
    return blockWasNotAdded(bve);
  } else {
    return blockWasAdded(bve);
  }
}

bool gen_block_reward::mark_invalid_block(CryptoNote::Core& /*c*/, size_t ev_index,
                                          const std::vector<test_event_entry>& /*events*/) {
  m_invalid_block_index = ev_index + 1;
  return true;
}

bool gen_block_reward::mark_checked_block(CryptoNote::Core& /*c*/, size_t ev_index,
                                          const std::vector<test_event_entry>& /*events*/) {
  m_checked_blocks_indices.push_back(ev_index - 1);
  return true;
}

BlockTemplate extract(const test_event_entry& ent) {
  if (ent.type() == typeid(BlockTemplate)) return boost::get<BlockTemplate>(ent);
  else if (ent.type() == typeid(RawBlock)) {
    auto raw = boost::get<RawBlock>(ent);
    return fromBinaryArray<BlockTemplate>(raw.block);
  } else {
    assert(false);
    return {};
  }
}

bool gen_block_reward::check_block_rewards(CryptoNote::Core& /*c*/, size_t /*ev_index*/,
                                           const std::vector<test_event_entry>& events) {
  DEFINE_TESTS_ERROR_CONTEXT("gen_block_reward_without_txs::check_block_rewards");

  std::array<uint64_t, 7> blk_rewards;
  blk_rewards[0] = m_currency->moneySupply() >> m_currency->emissionSpeedFactor();
  uint64_t cumulative_reward = blk_rewards[0];
  for (size_t i = 1; i < blk_rewards.size(); ++i) {
    blk_rewards[i] = (m_currency->moneySupply() - cumulative_reward) >> m_currency->emissionSpeedFactor();
    cumulative_reward += blk_rewards[i];
  }

  for (size_t i = 0; i < 5; ++i) {
    BlockTemplate blk_i = extract(events[m_checked_blocks_indices[i]]);
    CHECK_EQ(blk_rewards[i], get_tx_out_amount(blk_i.baseTransaction));
  }

  BlockTemplate blk_n1 = extract(events[m_checked_blocks_indices[5]]);
  CHECK_EQ(blk_rewards[5] + 3 * m_currency->minimumFee(), get_tx_out_amount(blk_n1.baseTransaction));

  BlockTemplate blk_n2 = extract(events[m_checked_blocks_indices[6]]);
  CHECK_EQ(blk_rewards[6] + (5 + 7) * m_currency->minimumFee(), get_tx_out_amount(blk_n2.baseTransaction));

  BlockTemplate blk_n3 = extract(events[m_checked_blocks_indices[7]]);
  CHECK_EQ((11 + 13) * m_currency->minimumFee(), get_tx_out_amount(blk_n3.baseTransaction));

  return true;
}
