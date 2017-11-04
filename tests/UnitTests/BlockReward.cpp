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

#include "gtest/gtest.h"

#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/Currency.h"
#include <Logging/LoggerGroup.h>

using namespace CryptoNote;

namespace
{
  const uint64_t TEST_GRANTED_FULL_REWARD_ZONE = 10000;
  const uint64_t TEST_MONEY_SUPPLY = static_cast<uint64_t>(-1);
  const uint64_t TEST_EMISSION_SPEED_FACTOR = 18;

  //--------------------------------------------------------------------------------------------------------------------
  class getBlockReward_and_already_generated_coins : public ::testing::Test {
  public:
    getBlockReward_and_already_generated_coins() :
      ::testing::Test(),
      m_currency(CryptoNote::CurrencyBuilder(m_logger).
        blockGrantedFullRewardZone(TEST_GRANTED_FULL_REWARD_ZONE).
        moneySupply(TEST_MONEY_SUPPLY).
        emissionSpeedFactor(TEST_EMISSION_SPEED_FACTOR).
        currency()) {
    }

  protected:
    static const size_t currentBlockSize = TEST_GRANTED_FULL_REWARD_ZONE / 2;

    Logging::LoggerGroup m_logger;
    CryptoNote::Currency m_currency;
    bool m_blockTooBig;
    int64_t m_emissionChange;
    uint64_t m_blockReward;
  };

  #define TEST_ALREADY_GENERATED_COINS(alreadyGeneratedCoins, expectedReward)              \
    m_blockTooBig = !m_currency.getBlockReward(BLOCK_MAJOR_VERSION_1, 0, currentBlockSize, \
      alreadyGeneratedCoins, 0, m_blockReward, m_emissionChange);                          \
    ASSERT_FALSE(m_blockTooBig);                                                           \
    ASSERT_EQ(UINT64_C(expectedReward), m_blockReward);                                    \
    ASSERT_EQ(UINT64_C(expectedReward), m_emissionChange);

  TEST_F(getBlockReward_and_already_generated_coins, handles_first_values) {
    TEST_ALREADY_GENERATED_COINS(0, 70368744177663);
    TEST_ALREADY_GENERATED_COINS(m_blockReward, 70368475742208);
    TEST_ALREADY_GENERATED_COINS(UINT64_C(2756434948434199641), 59853779316998);
  }

  TEST_F(getBlockReward_and_already_generated_coins, correctly_steps_from_reward_2_to_1) {
    TEST_ALREADY_GENERATED_COINS(m_currency.moneySupply() - ((UINT64_C(2) << m_currency.emissionSpeedFactor()) + 1), 2);
    TEST_ALREADY_GENERATED_COINS(m_currency.moneySupply() -  (UINT64_C(2) << m_currency.emissionSpeedFactor())     , 2);
    TEST_ALREADY_GENERATED_COINS(m_currency.moneySupply() - ((UINT64_C(2) << m_currency.emissionSpeedFactor()) - 1), 1);
  }

  TEST_F(getBlockReward_and_already_generated_coins, handles_max_already_generaged_coins) {
    TEST_ALREADY_GENERATED_COINS(m_currency.moneySupply() - ((UINT64_C(1) << m_currency.emissionSpeedFactor()) + 1), 1);
    TEST_ALREADY_GENERATED_COINS(m_currency.moneySupply() -  (UINT64_C(1) << m_currency.emissionSpeedFactor())     , 1);
    TEST_ALREADY_GENERATED_COINS(m_currency.moneySupply() - ((UINT64_C(1) << m_currency.emissionSpeedFactor()) - 1), 0);
    TEST_ALREADY_GENERATED_COINS(m_currency.moneySupply() - 1, 0);
    TEST_ALREADY_GENERATED_COINS(m_currency.moneySupply(), 0);
  }

  //--------------------------------------------------------------------------------------------------------------------
  class getBlockReward_and_median_and_blockSize : public ::testing::Test {
  public:
    getBlockReward_and_median_and_blockSize() :
      ::testing::Test(),
      m_currency(CryptoNote::CurrencyBuilder(m_logger).
        blockGrantedFullRewardZone(TEST_GRANTED_FULL_REWARD_ZONE).
        moneySupply(TEST_MONEY_SUPPLY).
        emissionSpeedFactor(TEST_EMISSION_SPEED_FACTOR).
        currency()) {
    }

  protected:
    static const uint64_t alreadyGeneratedCoins = 0;

    virtual void SetUp() override {
      m_blockTooBig = !m_currency.getBlockReward(BLOCK_MAJOR_VERSION_1, 0, 0, alreadyGeneratedCoins, 0, m_standardBlockReward, m_emissionChange);
      ASSERT_FALSE(m_blockTooBig);
      ASSERT_EQ(UINT64_C(70368744177663), m_standardBlockReward);
    }

    void do_test(size_t medianBlockSize, size_t currentBlockSize) {
      m_blockTooBig = !m_currency.getBlockReward(BLOCK_MAJOR_VERSION_1, medianBlockSize, currentBlockSize, alreadyGeneratedCoins, 0,
        m_blockReward, m_emissionChange);
    }

    Logging::LoggerGroup m_logger;
    CryptoNote::Currency m_currency;
    bool m_blockTooBig;
    int64_t m_emissionChange;
    uint64_t m_blockReward;
    uint64_t m_standardBlockReward;
  };

  TEST_F(getBlockReward_and_median_and_blockSize, handles_zero_median) {
    do_test(0, TEST_GRANTED_FULL_REWARD_ZONE);
    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(m_standardBlockReward, m_blockReward);
  }

  TEST_F(getBlockReward_and_median_and_blockSize, handles_median_lt_relevance_level) {
    do_test(TEST_GRANTED_FULL_REWARD_ZONE - 1, TEST_GRANTED_FULL_REWARD_ZONE);
    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(m_standardBlockReward, m_blockReward);
  }

  TEST_F(getBlockReward_and_median_and_blockSize, handles_median_eq_relevance_level) {
    do_test(TEST_GRANTED_FULL_REWARD_ZONE, TEST_GRANTED_FULL_REWARD_ZONE - 1);
    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(m_standardBlockReward, m_blockReward);
  }

  TEST_F(getBlockReward_and_median_and_blockSize, handles_median_gt_relevance_level) {
    do_test(TEST_GRANTED_FULL_REWARD_ZONE + 1, TEST_GRANTED_FULL_REWARD_ZONE);
    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(m_standardBlockReward, m_blockReward);
  }

  TEST_F(getBlockReward_and_median_and_blockSize, handles_big_median) {
    size_t blockSize = 1;
    size_t medianSize = std::numeric_limits<uint32_t>::max();

    do_test(medianSize, blockSize);

    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(m_standardBlockReward, m_blockReward);
  }

  TEST_F(getBlockReward_and_median_and_blockSize, handles_big_block_size) {
    size_t blockSize = std::numeric_limits<uint32_t>::max() - 1; // even
    size_t medianSize = blockSize / 2; // 2 * medianSize == blockSize

    do_test(medianSize, blockSize);
    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(0, m_blockReward);
  }

  TEST_F(getBlockReward_and_median_and_blockSize, handles_big_block_size_fail) {
    size_t blockSize = std::numeric_limits<uint32_t>::max();
    size_t medianSize = blockSize / 2 - 1;

    do_test(medianSize, blockSize);
    ASSERT_TRUE(m_blockTooBig);
  }

  TEST_F(getBlockReward_and_median_and_blockSize, handles_big_median_and_block_size) {
    // blockSize should be greater medianSize
    size_t blockSize = std::numeric_limits<uint32_t>::max();
    size_t medianSize = std::numeric_limits<uint32_t>::max() - 1;

    do_test(medianSize, blockSize);
    ASSERT_FALSE(m_blockTooBig);
    ASSERT_LT(m_blockReward, m_standardBlockReward);
  }

  //--------------------------------------------------------------------------------------------------------------------
  class getBlockReward_and_currentBlockSize : public ::testing::Test {
  public:
    getBlockReward_and_currentBlockSize() :
      ::testing::Test(),
      m_currency(CryptoNote::CurrencyBuilder(m_logger).
        blockGrantedFullRewardZone(TEST_GRANTED_FULL_REWARD_ZONE).
        moneySupply(TEST_MONEY_SUPPLY).
        emissionSpeedFactor(TEST_EMISSION_SPEED_FACTOR).
        currency()) {
    }

  protected:
    static const size_t testMedian = 7 * TEST_GRANTED_FULL_REWARD_ZONE;
    static const uint64_t alreadyGeneratedCoins = 0;

    virtual void SetUp() override {
      m_blockTooBig = !m_currency.getBlockReward(BLOCK_MAJOR_VERSION_3, testMedian, 0, alreadyGeneratedCoins, 0, m_standardBlockReward, m_emissionChange);

      ASSERT_FALSE(m_blockTooBig);
      ASSERT_EQ(UINT64_C(70368744177663), m_standardBlockReward);
    }

    void do_test(size_t currentBlockSize) {
      m_blockTooBig = !m_currency.getBlockReward(BLOCK_MAJOR_VERSION_3, testMedian, currentBlockSize, alreadyGeneratedCoins, 0, m_blockReward, m_emissionChange);
    }

    Logging::LoggerGroup m_logger;
    CryptoNote::Currency m_currency;
    bool m_blockTooBig;
    int64_t m_emissionChange;
    uint64_t m_blockReward;
    uint64_t m_standardBlockReward;
  };

  TEST_F(getBlockReward_and_currentBlockSize, handles_zero_block_size) {
    do_test(0);
    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(m_standardBlockReward, m_blockReward);
  }

  TEST_F(getBlockReward_and_currentBlockSize, handles_block_size_less_median) {
    do_test(testMedian - 1);
    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(m_standardBlockReward, m_blockReward);
  }

  TEST_F(getBlockReward_and_currentBlockSize, handles_block_size_eq_median) {
    do_test(testMedian);
    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(m_standardBlockReward, m_blockReward);
  }

  TEST_F(getBlockReward_and_currentBlockSize, handles_block_size_gt_median) {
    do_test(testMedian + 1);
    ASSERT_FALSE(m_blockTooBig);
    ASSERT_LT(m_blockReward, m_standardBlockReward);
  }

  TEST_F(getBlockReward_and_currentBlockSize, handles_block_size_less_2_medians) {
    do_test(2 * testMedian - 1);
    ASSERT_FALSE(m_blockTooBig);
    ASSERT_LT(m_blockReward, m_standardBlockReward);
    ASSERT_GT(m_blockReward, 0);
  }

  TEST_F(getBlockReward_and_currentBlockSize, handles_block_size_eq_2_medians) {
    do_test(2 * testMedian);
    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(0, m_blockReward);
  }

  TEST_F(getBlockReward_and_currentBlockSize, handles_block_size_gt_2_medians) {
    do_test(2 * testMedian + 1);
    ASSERT_TRUE(m_blockTooBig);
  }

  TEST_F(getBlockReward_and_currentBlockSize, calculates_correctly) {
    ASSERT_EQ(0, testMedian % 8);

    // reward = 1 - (k - 1)^2
    // k = 9/8 => reward = 63/64
    do_test(testMedian * 9 / 8);
    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(m_standardBlockReward * 63 / 64, m_blockReward);

    // 3/2 = 12/8
    do_test(testMedian * 3 / 2);
    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(m_standardBlockReward * 3 / 4, m_blockReward);

    do_test(testMedian * 15 / 8);
    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(m_standardBlockReward * 15 / 64, m_blockReward);
  }
  //--------------------------------------------------------------------------------------------------------------------
  const unsigned int testEmissionSpeedFactor = 4;
  const size_t testGrantedFullRewardZoneV1 = CryptoNote::parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1;
  const size_t testGrantedFullRewardZoneV2 = CryptoNote::parameters::CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2;
  const size_t testMedianV1 = testGrantedFullRewardZoneV1;
  const size_t testMedianV2 = testGrantedFullRewardZoneV2;
  const size_t testBlockSizeV1 = testMedianV1 + testMedianV1 * 8 / 10; // expected penalty 0.64 * reward
  const size_t testBlockSizeV2 = testMedianV2 + testMedianV2 * 8 / 10; // expected penalty 0.64 * reward
  const uint64_t testPenalty = 64; // percentage
  const uint64_t testMoneySupply = UINT64_C(1000000000);
  const uint64_t expectedBaseReward = 62500000;  // testMoneySupply >> testEmissionSpeedFactor
  const uint64_t expectedBlockReward = 22500000; // expectedBaseReward - expectedBaseReward * testPenalty / 100
  //--------------------------------------------------------------------------------------------------------------------
  class getBlockReward_fee_and_penalizeFee_test : public ::testing::Test {
  public:
    getBlockReward_fee_and_penalizeFee_test() :
      ::testing::Test(),
      m_currency(CryptoNote::CurrencyBuilder(m_logger).
        blockGrantedFullRewardZone(testGrantedFullRewardZoneV1).
        moneySupply(testMoneySupply).
        emissionSpeedFactor(testEmissionSpeedFactor).
        currency()) {
    }

  protected:
    virtual void SetUp() override {
      uint64_t blockReward;
      int64_t emissionChange;

      m_blockTooBig = !m_currency.getBlockReward(BLOCK_MAJOR_VERSION_3, testMedianV1, testBlockSizeV1, 0, 0, blockReward, emissionChange);

      ASSERT_FALSE(m_blockTooBig);
      ASSERT_EQ(expectedBlockReward, blockReward);
      ASSERT_EQ(expectedBlockReward, emissionChange);
    }

    void do_testV1(uint64_t alreadyGeneratedCoins, uint64_t fee) {
      m_blockTooBig = !m_currency.getBlockReward(CryptoNote::BLOCK_MAJOR_VERSION_1, testMedianV1, testBlockSizeV1, alreadyGeneratedCoins, fee, m_blockReward, m_emissionChange);
    }

    void do_testV2(uint64_t alreadyGeneratedCoins, uint64_t fee) {
      m_blockTooBig = !m_currency.getBlockReward(CryptoNote::BLOCK_MAJOR_VERSION_2, testMedianV2, testBlockSizeV2, alreadyGeneratedCoins, fee, m_blockReward, m_emissionChange);
    }

    Logging::LoggerGroup m_logger;
    CryptoNote::Currency m_currency;
    bool m_blockTooBig;
    int64_t m_emissionChange;
    uint64_t m_blockReward;
  };

  TEST_F(getBlockReward_fee_and_penalizeFee_test, handles_zero_fee_and_no_penalize_fee) {
    do_testV1(0, 0);

    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(expectedBlockReward, m_blockReward);
    ASSERT_EQ(expectedBlockReward, m_emissionChange);
    ASSERT_GT(m_emissionChange, 0);
  }

  TEST_F(getBlockReward_fee_and_penalizeFee_test, handles_zero_fee_and_penalize_fee) {
    do_testV2(0, 0);

    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(expectedBlockReward, m_blockReward);
    ASSERT_EQ(expectedBlockReward, m_emissionChange);
    ASSERT_GT(m_emissionChange, 0);
  }

  TEST_F(getBlockReward_fee_and_penalizeFee_test, handles_fee_lt_block_reward_and_no_penalize_fee) {
    uint64_t fee = expectedBlockReward / 2;
    do_testV1(0, fee);

    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(expectedBlockReward + fee, m_blockReward);
    ASSERT_EQ(expectedBlockReward, m_emissionChange);
    ASSERT_GT(m_emissionChange, 0);
  }

  TEST_F(getBlockReward_fee_and_penalizeFee_test, handles_fee_lt_block_reward_and_penalize_fee) {
    uint64_t fee = expectedBlockReward / 2;
    do_testV2(0, fee);

    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(expectedBlockReward + fee - fee * testPenalty / 100, m_blockReward);
    ASSERT_EQ(expectedBlockReward - fee * testPenalty / 100, m_emissionChange);
    ASSERT_GT(m_emissionChange, 0);
  }

  TEST_F(getBlockReward_fee_and_penalizeFee_test, handles_fee_eq_block_reward_and_no_penalize_fee) {
    uint64_t fee = expectedBlockReward;
    do_testV1(0, fee);

    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(expectedBlockReward + fee, m_blockReward);
    ASSERT_EQ(expectedBlockReward, m_emissionChange);
    ASSERT_GT(m_emissionChange, 0);
  }

  TEST_F(getBlockReward_fee_and_penalizeFee_test, handles_fee_eq_block_reward_and_penalize_fee) {
    uint64_t fee = expectedBlockReward;
    do_testV2(0, fee);

    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(expectedBlockReward + fee - fee * testPenalty / 100, m_blockReward);
    ASSERT_EQ(expectedBlockReward - fee * testPenalty / 100, m_emissionChange);
    ASSERT_GT(m_emissionChange, 0);
  }

  TEST_F(getBlockReward_fee_and_penalizeFee_test, handles_fee_gt_block_reward_and_no_penalize_fee) {
    uint64_t fee = 2 * expectedBlockReward;
    do_testV1(0, fee);

    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(expectedBlockReward + fee, m_blockReward);
    ASSERT_EQ(expectedBlockReward, m_emissionChange);
  }

  TEST_F(getBlockReward_fee_and_penalizeFee_test, handles_fee_gt_block_reward_and_penalize_fee) {
    uint64_t fee = 2 * expectedBlockReward;
    do_testV2(0, fee);

    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(expectedBlockReward + fee - fee * testPenalty / 100, m_blockReward);
    ASSERT_EQ(expectedBlockReward - fee * testPenalty / 100, m_emissionChange);
    ASSERT_LT(m_emissionChange, 0);
  }

  TEST_F(getBlockReward_fee_and_penalizeFee_test, handles_emission_change_eq_zero) {
    uint64_t fee = expectedBlockReward * 100 / testPenalty;
    do_testV2(0, fee);

    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(expectedBlockReward + fee - fee * testPenalty / 100, m_blockReward);
    ASSERT_EQ(0, m_emissionChange);
  }

  TEST_F(getBlockReward_fee_and_penalizeFee_test, handles_fee_if_block_reward_is_zero_and_no_penalize_fee) {
    uint64_t fee = UINT64_C(100);
    do_testV1(m_currency.moneySupply(), fee);

    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(fee, m_blockReward);
    ASSERT_EQ(0, m_emissionChange);
  }

  TEST_F(getBlockReward_fee_and_penalizeFee_test, handles_fee_if_block_reward_is_zero_and_penalize_fee) {
    uint64_t fee = UINT64_C(100);
    do_testV2(m_currency.moneySupply(), fee);

    ASSERT_FALSE(m_blockTooBig);
    ASSERT_EQ(fee - fee * testPenalty / 100, m_blockReward);
    ASSERT_EQ(-static_cast<int64_t>(fee * testPenalty / 100), m_emissionChange);
  }
}
