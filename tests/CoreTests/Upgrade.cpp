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

#include "Upgrade.h"

using namespace CryptoNote;

namespace {
  bool makeBlocks(std::vector<test_event_entry>& events, test_generator& generator, BlockTemplate& lastBlock,
                  const BlockTemplate& parentBlock, const CryptoNote::AccountBase& minerAcc, size_t count,
                  uint8_t majorVersion, uint8_t minorVersion) {
   CryptoNote::BlockTemplate prevBlock = parentBlock;
    for (size_t i = 0; i < count; ++i) {
     CryptoNote::BlockTemplate b;
      bool r = generator.constructBlockManually(b, prevBlock, minerAcc, test_generator::bf_major_ver | test_generator::bf_minor_ver,
        majorVersion, minorVersion);
      if (!r) {
        return false;
      } else {
        prevBlock = b;
        events.push_back(b);
      }
    }

    lastBlock = prevBlock;

    return true;
  }
}

gen_upgrade::gen_upgrade() : m_invalidBlockIndex(0), m_checkBlockTemplateVersionCallCounter(0),
    m_coinsInCirculationBeforeUpgrade(0), m_coinsInCirculationAfterUpgrade(0) {
  CryptoNote::CurrencyBuilder currencyBuilder(m_logger);
  currencyBuilder.maxBlockSizeInitial(std::numeric_limits<size_t>::max() / 2);
  currencyBuilder.upgradeHeightV2(std::numeric_limits<uint32_t>::max());
  // Disable voting and never upgrade to v.3.0
  currencyBuilder.upgradeHeightV3(CryptoNote::parameters::CRYPTONOTE_MAX_BLOCK_NUMBER);
  m_currency.reset(new Currency(currencyBuilder.currency()));

  REGISTER_CALLBACK_METHOD(gen_upgrade, markInvalidBlock);
  REGISTER_CALLBACK_METHOD(gen_upgrade, checkBlockTemplateVersionIsV1);
  REGISTER_CALLBACK_METHOD(gen_upgrade, checkBlockTemplateVersionIsV2);
  REGISTER_CALLBACK_METHOD(gen_upgrade, checkBlockRewardEqFee);
  REGISTER_CALLBACK_METHOD(gen_upgrade, checkBlockRewardIsZero);
  REGISTER_CALLBACK_METHOD(gen_upgrade, rememberCoinsInCirculationBeforeUpgrade);
  REGISTER_CALLBACK_METHOD(gen_upgrade, rememberCoinsInCirculationAfterUpgrade);
}

bool gen_upgrade::generate(std::vector<test_event_entry>& events) const {
  const uint64_t tsStart = 1338224400;

  GENERATE_ACCOUNT(minerAccount);
  MAKE_GENESIS_BLOCK(events, blk0, minerAccount, tsStart);

  // Vote for upgrade
  BlockTemplate blk1;
  if (!makeBlocks(events, generator, blk1, blk0, minerAccount, m_currency->minNumberVotingBlocks(), BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1)) {
    return false;
  }

  if (!checkBeforeUpgrade(events, generator, blk1, minerAccount, true)) {
    return false;
  }

  // Fill m_currency.upgradeVotingWindow()
  BlockTemplate blk2;
  if (!makeBlocks(events, generator, blk2, blk1, minerAccount, m_currency->upgradeVotingWindow() - m_currency->minNumberVotingBlocks() - 1,
      BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_0)) {
    return false;
  }

  // Upgrade voting complete!
  uint32_t votingCompleteHeight = CachedBlock(blk2).getBlockIndex();
  uint32_t upgradeHeight = m_currency->calculateUpgradeHeight(votingCompleteHeight);

  if (!checkBeforeUpgrade(events, generator, blk2, minerAccount, true)) {
    return false;
  }

  // Create blocks up to upgradeHeight
  BlockTemplate blk3;
  if (!makeBlocks(events, generator, blk3, blk2, minerAccount, upgradeHeight - votingCompleteHeight - 1, BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_0)) {
    return false;
  }

  if (!checkBeforeUpgrade(events, generator, blk3, minerAccount, false)) {
    return false;
  }

  // Create last block with version 1.x
  BlockTemplate blk4;
  if (!makeBlocks(events, generator, blk4, blk3, minerAccount, 1, BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_0)) {
    return false;
  }

  generator.defaultMajorVersion = BLOCK_MAJOR_VERSION_2;
  generator.defaultMinorVersion = BLOCK_MINOR_VERSION_0;

  if (!checkAfterUpgrade(events, generator, blk4, minerAccount)) {
    return false;
  }

  // Create a few blocks with version 2.0
  BlockTemplate blk5;
  if (!makeBlocks(events, generator, blk5, blk4, minerAccount, 3, BLOCK_MAJOR_VERSION_2, BLOCK_MINOR_VERSION_0)) {
    return false;
  }

  if (!checkAfterUpgrade(events, generator, blk5, minerAccount)) {
    return false;
  }

  return true;
}

bool gen_upgrade::checkBeforeUpgrade(std::vector<test_event_entry>& events, test_generator& generator,
                                     const CryptoNote::BlockTemplate& parentBlock, const CryptoNote::AccountBase& minerAcc,
                                     bool checkReward) const {
  // Checking 1: get_block_templare returns block with major version 1
  DO_CALLBACK(events, "checkBlockTemplateVersionIsV1");

  // Checking 2: penalty doesn't apply to transactions fee
  if (checkReward) {
    // Add block to the blockchain, later it become an alternative
    DO_CALLBACK(events, "rememberCoinsInCirculationBeforeUpgrade");
    MAKE_TX_LIST_START(events, txs, minerAcc, minerAcc, MK_COINS(1), parentBlock);
    BlockTemplate alternativeBlk;
    if (!generator.constructMaxSizeBlock(alternativeBlk, parentBlock, minerAcc, m_currency->rewardBlocksWindow(), txs)) {
      return false;
    }
    events.push_back(populateBlock(alternativeBlk, txs));
    DO_CALLBACK(events, "checkBlockRewardEqFee");
  }

  // Checking 3: block with version 2.0 doesn't accepted
  BlockTemplate badBlock;
  DO_CALLBACK(events, "markInvalidBlock");
  return makeBlocks(events, generator, badBlock, parentBlock, minerAcc, 1, BLOCK_MAJOR_VERSION_2, BLOCK_MINOR_VERSION_0);
}

bool gen_upgrade::checkAfterUpgrade(std::vector<test_event_entry>& events, test_generator& generator,
                                    const CryptoNote::BlockTemplate& parentBlock, const CryptoNote::AccountBase& minerAcc) const {
  // Checking 1: get_block_templare returns block with major version 2
  DO_CALLBACK(events, "checkBlockTemplateVersionIsV2");

  // Checking 2: penalty applies to transactions fee
  // Add block to the blockchain, later it become an alternative
  DO_CALLBACK(events, "rememberCoinsInCirculationAfterUpgrade");
  MAKE_TX_LIST_START(events, txs, minerAcc, minerAcc, MK_COINS(1), parentBlock);
  BlockTemplate alternativeBlk;
  if (!generator.constructMaxSizeBlock(alternativeBlk, parentBlock, minerAcc, m_currency->rewardBlocksWindow(), txs)) {
    return false;
  }
  events.push_back(populateBlock(alternativeBlk, txs));
  DO_CALLBACK(events, "checkBlockRewardIsZero");

  // Checking 3: block with version 1.0 doesn't accepted
  BlockTemplate badBlock;
  DO_CALLBACK(events, "markInvalidBlock");
  if (!makeBlocks(events, generator, badBlock, parentBlock, minerAcc, 1, BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_0)) {
    return false;
  }

  // Checking 2: block with version 1.1 doesn't accepted
  DO_CALLBACK(events, "markInvalidBlock");
  return makeBlocks(events, generator, badBlock, parentBlock, minerAcc, 1, BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1);
}

bool gen_upgrade::check_block_verification_context(std::error_code bve, size_t eventIdx, const CryptoNote::BlockTemplate& /*blk*/) {
  if (m_invalidBlockIndex == eventIdx) {
    m_invalidBlockIndex = 0;
    return blockWasNotAdded(bve);
  } else {
    return blockWasAdded(bve);
  }
}

bool gen_upgrade::markInvalidBlock(CryptoNote::Core& /*c*/, size_t evIndex, const std::vector<test_event_entry>& /*events*/) {
  m_invalidBlockIndex = evIndex + 1;
  return true;
}

bool gen_upgrade::checkBlockTemplateVersionIsV1(CryptoNote::Core& c, size_t /*evIndex*/, const std::vector<test_event_entry>& /*events*/) {
  DEFINE_TESTS_ERROR_CONTEXT("gen_upgrade::checkBlockTemplateVersionIsV1");
  CHECK_TEST_CONDITION(checkBlockTemplateVersion(c, BLOCK_MAJOR_VERSION_1, BLOCK_MINOR_VERSION_1));
  return true;
}

bool gen_upgrade::checkBlockTemplateVersionIsV2(CryptoNote::Core& c, size_t /*evIndex*/, const std::vector<test_event_entry>& /*events*/) {
  DEFINE_TESTS_ERROR_CONTEXT("gen_upgrade::checkBlockTemplateVersionIsV2");
  CHECK_TEST_CONDITION(checkBlockTemplateVersion(c, BLOCK_MAJOR_VERSION_2, BLOCK_MINOR_VERSION_0));
  return true;
}

bool gen_upgrade::checkBlockTemplateVersion(CryptoNote::Core& c, uint8_t expectedMajorVersion, uint8_t expectedMinorVersion) {
  DEFINE_TESTS_ERROR_CONTEXT("gen_upgrade::checkBlockTemplateVersion");

  AccountBase account;
  account.generate();

  BlockTemplate b;
  Difficulty difficulty;
  uint32_t height;
  CHECK_TEST_CONDITION(c.getBlockTemplate(b, account.getAccountKeys().address, BinaryArray(), difficulty, height));
  CHECK_EQ(static_cast<int>(b.majorVersion), static_cast<int>(expectedMajorVersion));
  CHECK_EQ(static_cast<int>(b.minorVersion), static_cast<int>(expectedMinorVersion));

  return true;
}

bool gen_upgrade::checkBlockRewardEqFee(CryptoNote::Core& c, size_t evIndex, const std::vector<test_event_entry>& events) {
  DEFINE_TESTS_ERROR_CONTEXT("gen_upgrade::checkBlockRewardEqFee");

  BlockTemplate blk = boost::get<BlockTemplate>(events[evIndex - 1]);
  uint64_t blockReward = getSummaryOutsAmount(blk.baseTransaction);
  CHECK_EQ(blockReward, m_currency->minimumFee());

  CHECK_EQ(m_coinsInCirculationBeforeUpgrade, c.getTotalGeneratedAmount());

  return true;
}

bool gen_upgrade::checkBlockRewardIsZero(CryptoNote::Core& c, size_t evIndex, const std::vector<test_event_entry>& events) {
  DEFINE_TESTS_ERROR_CONTEXT("gen_upgrade::checkBlockRewardIsZero");

  BlockTemplate blk = boost::get<BlockTemplate>(events[evIndex - 1]);
  uint64_t blockReward = getSummaryOutsAmount(blk.baseTransaction);
  CHECK_EQ(blockReward, 0);

  CHECK_EQ(m_coinsInCirculationAfterUpgrade - m_currency->minimumFee(), c.getTotalGeneratedAmount());

  return true;
}

bool gen_upgrade::rememberCoinsInCirculationBeforeUpgrade(CryptoNote::Core& c, size_t /*evIndex*/, const std::vector<test_event_entry>& /*events*/) {
  m_coinsInCirculationBeforeUpgrade = c.getTotalGeneratedAmount();
  return true;
}

bool gen_upgrade::rememberCoinsInCirculationAfterUpgrade(CryptoNote::Core& c, size_t /*evIndex*/, const std::vector<test_event_entry>& /*events*/) {
  m_coinsInCirculationAfterUpgrade = c.getTotalGeneratedAmount();
  return true;
}
