// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016, The Karbowanec developers
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

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <boost/utility.hpp>
#include "../CryptoNoteConfig.h"
#include "../crypto/hash.h"
#include "../Logging/LoggerRef.h"
#include "CryptoNoteBasic.h"
#include "Difficulty.h"

namespace CryptoNote {

class AccountBase;

class Currency {
public:
  uint64_t maxBlockHeight() const { return m_maxBlockHeight; }
  size_t maxBlockBlobSize() const { return m_maxBlockBlobSize; }
  size_t maxTxSize() const { return m_maxTxSize; }
  uint64_t publicAddressBase58Prefix() const { return m_publicAddressBase58Prefix; }
  size_t minedMoneyUnlockWindow() const { return m_minedMoneyUnlockWindow; }
  size_t transactionSpendableAge() const { return m_transactionSpendableAge; }
  size_t expectedNumberOfBlocksPerDay() const { return m_expectedNumberOfBlocksPerDay; }

  size_t timestampCheckWindow() const { return m_timestampCheckWindow; }
  size_t timestampCheckWindow(uint8_t blockMajorVersion) const {
    if (blockMajorVersion >= BLOCK_MAJOR_VERSION_4) {
      return timestampCheckWindow_v1();
    }
    else {
      return timestampCheckWindow();
    }
  }
  size_t timestampCheckWindow_v1() const { return m_timestampCheckWindow_v1; }
  uint64_t blockFutureTimeLimit() const { return m_blockFutureTimeLimit; }
  uint64_t blockFutureTimeLimit(uint8_t blockMajorVersion) const {
    if (blockMajorVersion >= BLOCK_MAJOR_VERSION_4) {
      return blockFutureTimeLimit_v1();
    }
    else {
      return blockFutureTimeLimit();
    }
  }
  uint64_t blockFutureTimeLimit_v1() const { return m_blockFutureTimeLimit_v1; }

  uint64_t moneySupply() const { return m_moneySupply; }
  unsigned int emissionSpeedFactor() const { return m_emissionSpeedFactor; }
  size_t cryptonoteCoinVersion() const { return m_cryptonoteCoinVersion; }

  size_t rewardBlocksWindow() const { return m_rewardBlocksWindow; }
  size_t blockGrantedFullRewardZone() const { return m_blockGrantedFullRewardZone; }
  size_t blockGrantedFullRewardZoneByBlockVersion(uint8_t blockMajorVersion) const;
  size_t minerTxBlobReservedSize() const { return m_minerTxBlobReservedSize; }
  uint64_t maxTransactionSizeLimit() const { return m_maxTransactionSizeLimit; }

  size_t minMixin() const { return m_minMixin; }
  size_t maxMixin() const { return m_maxMixin; }

  size_t numberOfDecimalPlaces() const { return m_numberOfDecimalPlaces; }
  uint64_t coin() const { return m_coin; }

  uint64_t minimumFee() const { return m_minimumFee; }
  uint64_t getMinimalFee(uint64_t avgCurrentDifficulty, uint64_t currentReward, uint64_t avgReferenceDifficulty, uint64_t avgReferenceReward, uint32_t height) const;
  uint64_t getFeePerByte(const uint64_t txExtraSize, const uint64_t minFee) const;

  uint64_t defaultDustThreshold() const { return m_defaultDustThreshold; }

  uint64_t difficultyTarget() const { return m_difficultyTarget; }
  size_t difficultyWindow() const { return m_difficultyWindow; }
  size_t difficultyLag() const { return m_difficultyLag; }
  size_t difficultyCut() const { return m_difficultyCut; }
  size_t difficultyBlocksCountByBlockVersion(uint8_t blockMajorVersion) const {
    if (blockMajorVersion >= BLOCK_MAJOR_VERSION_5) {
      return difficultyBlocksCount4() + 1;
    }
    else if (blockMajorVersion == BLOCK_MAJOR_VERSION_3 || blockMajorVersion == BLOCK_MAJOR_VERSION_4) {
      return difficultyBlocksCount3() + 1;
    }
    else if (blockMajorVersion == BLOCK_MAJOR_VERSION_2) {
      return difficultyBlocksCount2();
    }
    else {
      return difficultyBlocksCount();
    }
  };
  size_t difficultyBlocksCount() const { return m_difficultyWindow + m_difficultyLag; }
  size_t difficultyBlocksCount2() const { return CryptoNote::parameters::DIFFICULTY_WINDOW_V2; }
  size_t difficultyBlocksCount3() const { return CryptoNote::parameters::DIFFICULTY_WINDOW_V3; }
  size_t difficultyBlocksCount4() const { return CryptoNote::parameters::DIFFICULTY_WINDOW_V4; }

  size_t maxBlockSizeInitial() const { return m_maxBlockSizeInitial; }
  uint64_t maxBlockSizeGrowthSpeedNumerator() const { return m_maxBlockSizeGrowthSpeedNumerator; }
  uint64_t maxBlockSizeGrowthSpeedDenominator() const { return m_maxBlockSizeGrowthSpeedDenominator; }

  uint64_t lockedTxAllowedDeltaSeconds() const { return m_lockedTxAllowedDeltaSeconds; }
  size_t lockedTxAllowedDeltaBlocks() const { return m_lockedTxAllowedDeltaBlocks; }

  uint64_t mempoolTxLiveTime() const { return m_mempoolTxLiveTime; }
  uint64_t mempoolTxFromAltBlockLiveTime() const { return m_mempoolTxFromAltBlockLiveTime; }
  uint64_t numberOfPeriodsToForgetTxDeletedFromPool() const { return m_numberOfPeriodsToForgetTxDeletedFromPool; }

  size_t fusionTxMaxSize() const { return m_fusionTxMaxSize; }
  size_t fusionTxMinInputCount() const { return m_fusionTxMinInputCount; }
  size_t fusionTxMinInOutCountRatio() const { return m_fusionTxMinInOutCountRatio; }

  uint32_t upgradeHeight(uint8_t majorVersion) const;
  unsigned int upgradeVotingThreshold() const { return m_upgradeVotingThreshold; }
  uint32_t upgradeVotingWindow() const { return m_upgradeVotingWindow; }
  uint32_t upgradeWindow() const { return m_upgradeWindow; }
  uint32_t minNumberVotingBlocks() const { return (m_upgradeVotingWindow * m_upgradeVotingThreshold + 99) / 100; }
  uint32_t maxUpgradeDistance() const { return 7 * m_upgradeWindow; }
  uint32_t calculateUpgradeHeight(uint32_t voteCompleteHeight) const { return voteCompleteHeight + m_upgradeWindow; }

  const std::string& blocksFileName() const { return m_blocksFileName; }
  const std::string& blocksCacheFileName() const { return m_blocksCacheFileName; }
  const std::string& blockIndexesFileName() const { return m_blockIndexesFileName; }
  const std::string& txPoolFileName() const { return m_txPoolFileName; }
  const std::string& blockchainIndicesFileName() const { return m_blockchainIndicesFileName; }

  bool isTestnet() const { return m_testnet; }

  const Block& genesisBlock() const { return m_genesisBlock; }
  const Crypto::Hash& genesisBlockHash() const { return m_genesisBlockHash; }

  uint64_t calculateReward(uint64_t alreadyGeneratedCoins) const;
  bool getBlockReward(uint8_t blockMajorVersion, size_t medianSize, size_t currentBlockSize, uint64_t alreadyGeneratedCoins, uint64_t fee,
    uint64_t& reward, int64_t& emissionChange) const;
  size_t maxBlockCumulativeSize(uint64_t height) const;

  bool constructMinerTx(uint8_t blockMajorVersion, uint32_t height, size_t medianSize, uint64_t alreadyGeneratedCoins, size_t currentBlockSize,
    uint64_t fee, const AccountPublicAddress& minerAddress, Transaction& tx, const BinaryArray& extraNonce = BinaryArray(), size_t maxOuts = 1) const;

  bool isFusionTransaction(const Transaction& transaction, uint32_t height) const;
  bool isFusionTransaction(const Transaction& transaction, size_t size, uint32_t height) const;
  bool isFusionTransaction(const std::vector<uint64_t>& inputsAmounts, const std::vector<uint64_t>& outputsAmounts, size_t size, uint32_t height) const;
  bool isAmountApplicableInFusionTransactionInput(uint64_t amount, uint64_t threshold, uint32_t height) const;
  bool isAmountApplicableInFusionTransactionInput(uint64_t amount, uint64_t threshold, uint8_t& amountPowerOfTen, uint32_t height) const;

  std::string accountAddressAsString(const AccountBase& account) const;
  std::string accountAddressAsString(const AccountPublicAddress& accountPublicAddress) const;
  bool parseAccountAddressString(const std::string& str, AccountPublicAddress& addr) const;

  std::string formatAmount(uint64_t amount) const;
  std::string formatAmount(int64_t amount) const;
  bool parseAmount(const std::string& str, uint64_t& amount) const;

  uint64_t roundUpMinFee(uint64_t minimalFee, int digits) const;

  difficulty_type nextDifficulty(uint32_t height, uint8_t blockMajorVersion, std::vector<uint64_t> timestamps, std::vector<difficulty_type> Difficulties) const;
  difficulty_type nextDifficultyV1(std::vector<uint64_t> timestamps, std::vector<difficulty_type> Difficulties) const;
  difficulty_type nextDifficultyV2(std::vector<uint64_t> timestamps, std::vector<difficulty_type> Difficulties) const;
  difficulty_type nextDifficultyV3(std::vector<uint64_t> timestamps, std::vector<difficulty_type> Difficulties) const;
  difficulty_type nextDifficultyV4(uint32_t height, uint8_t blockMajorVersion, std::vector<uint64_t> timestamps, std::vector<difficulty_type> Difficulties) const;
  difficulty_type nextDifficultyV5(uint32_t height, uint8_t blockMajorVersion, std::vector<uint64_t> timestamps, std::vector<difficulty_type> Difficulties) const;

  bool checkProofOfWorkV1(Crypto::cn_context& context, const Block& block, difficulty_type currentDiffic, Crypto::Hash& proofOfWork) const;
  bool checkProofOfWorkV2(Crypto::cn_context& context, const Block& block, difficulty_type currentDiffic, Crypto::Hash& proofOfWork) const;
  bool checkProofOfWork(Crypto::cn_context& context, const Block& block, difficulty_type currentDiffic, Crypto::Hash& proofOfWork) const;

  size_t getApproximateMaximumInputCount(size_t transactionSize, size_t outputCount, size_t mixinCount) const;

  static const std::vector<uint64_t> PRETTY_AMOUNTS;

private:
  Currency(Logging::ILogger& log) : logger(log, "currency") {
  }

  bool init();

  bool generateGenesisBlock();

private:
  uint64_t m_maxBlockHeight;
  size_t m_maxBlockBlobSize;
  size_t m_maxTxSize;
  uint64_t m_publicAddressBase58Prefix;
  size_t m_minedMoneyUnlockWindow;
  size_t m_transactionSpendableAge;
  size_t m_expectedNumberOfBlocksPerDay;

  size_t m_timestampCheckWindow;
  size_t m_timestampCheckWindow_v1;
  uint64_t m_blockFutureTimeLimit;
  uint64_t m_blockFutureTimeLimit_v1;

  uint64_t m_moneySupply;
  unsigned int m_emissionSpeedFactor;
  size_t m_cryptonoteCoinVersion;

  size_t m_rewardBlocksWindow;
  size_t m_blockGrantedFullRewardZone;
  size_t m_minerTxBlobReservedSize;
  uint64_t m_maxTransactionSizeLimit;

  size_t m_numberOfDecimalPlaces;
  uint64_t m_coin;

  uint64_t m_minimumFee;

  size_t m_minMixin;
  size_t m_maxMixin;

  uint64_t m_defaultDustThreshold;

  uint64_t m_difficultyTarget;
  size_t m_difficultyWindow;
  size_t m_difficultyLag;
  size_t m_difficultyCut;

  size_t m_maxBlockSizeInitial;
  uint64_t m_maxBlockSizeGrowthSpeedNumerator;
  uint64_t m_maxBlockSizeGrowthSpeedDenominator;

  uint64_t m_lockedTxAllowedDeltaSeconds;
  size_t m_lockedTxAllowedDeltaBlocks;

  uint64_t m_mempoolTxLiveTime;
  uint64_t m_mempoolTxFromAltBlockLiveTime;
  uint64_t m_numberOfPeriodsToForgetTxDeletedFromPool;

  size_t m_fusionTxMaxSize;
  size_t m_fusionTxMinInputCount;
  size_t m_fusionTxMinInOutCountRatio;

  uint32_t m_upgradeHeightV2;
  uint32_t m_upgradeHeightV3;
  uint32_t m_upgradeHeightV4;
  uint32_t m_upgradeHeightV5;
  unsigned int m_upgradeVotingThreshold;
  uint32_t m_upgradeVotingWindow;
  uint32_t m_upgradeWindow;

  std::string m_blocksFileName;
  std::string m_blocksCacheFileName;
  std::string m_blockIndexesFileName;
  std::string m_txPoolFileName;
  std::string m_blockchainIndicesFileName;

  bool m_testnet;

  Block m_genesisBlock;
  Crypto::Hash m_genesisBlockHash;

  Logging::LoggerRef logger;

  friend class CurrencyBuilder;
};

class CurrencyBuilder : boost::noncopyable {
public:
  CurrencyBuilder(Logging::ILogger& log);

  Currency currency() {
    if (!m_currency.init()) {
      throw std::runtime_error("Failed to initialize currency object");
    }
    return m_currency;
  }

  Transaction generateGenesisTransaction();
  CurrencyBuilder& maxBlockNumber(uint64_t val) { m_currency.m_maxBlockHeight = val; return *this; }
  CurrencyBuilder& maxBlockBlobSize(size_t val) { m_currency.m_maxBlockBlobSize = val; return *this; }
  CurrencyBuilder& maxTxSize(size_t val) { m_currency.m_maxTxSize = val; return *this; }
  CurrencyBuilder& publicAddressBase58Prefix(uint64_t val) { m_currency.m_publicAddressBase58Prefix = val; return *this; }
  CurrencyBuilder& minedMoneyUnlockWindow(size_t val) { m_currency.m_minedMoneyUnlockWindow = val; return *this; }
  CurrencyBuilder& transactionSpendableAge(size_t val) { m_currency.m_transactionSpendableAge = val; return *this; }
  CurrencyBuilder& expectedNumberOfBlocksPerDay(size_t val) { m_currency.m_expectedNumberOfBlocksPerDay = val; return *this; }

  CurrencyBuilder& timestampCheckWindow(size_t val) { m_currency.m_timestampCheckWindow = val; return *this; }
  CurrencyBuilder& timestampCheckWindow_v1(size_t val) { m_currency.m_timestampCheckWindow_v1 = val; return *this; }
  CurrencyBuilder& blockFutureTimeLimit(uint64_t val) { m_currency.m_blockFutureTimeLimit = val; return *this; }
  CurrencyBuilder& blockFutureTimeLimit_v1(uint64_t val) { m_currency.m_blockFutureTimeLimit_v1 = val; return *this; }

  CurrencyBuilder& moneySupply(uint64_t val) { m_currency.m_moneySupply = val; return *this; }
  CurrencyBuilder& emissionSpeedFactor(unsigned int val);
  CurrencyBuilder& cryptonoteCoinVersion(size_t val) { m_currency.m_cryptonoteCoinVersion = val; return *this; }

  CurrencyBuilder& rewardBlocksWindow(size_t val) { m_currency.m_rewardBlocksWindow = val; return *this; }
  CurrencyBuilder& blockGrantedFullRewardZone(size_t val) { m_currency.m_blockGrantedFullRewardZone = val; return *this; }
  CurrencyBuilder& minerTxBlobReservedSize(size_t val) { m_currency.m_minerTxBlobReservedSize = val; return *this; }
  CurrencyBuilder& maxTransactionSizeLimit(uint64_t val) { m_currency.m_maxTransactionSizeLimit = val; return *this; }

  CurrencyBuilder& minMixin(size_t val) { m_currency.m_minMixin = val; return *this; }
  CurrencyBuilder& maxMixin(size_t val) { m_currency.m_maxMixin = val; return *this; }

  CurrencyBuilder& numberOfDecimalPlaces(size_t val);

  CurrencyBuilder& minimumFee(uint64_t val) { m_currency.m_minimumFee = val; return *this; }
  CurrencyBuilder& defaultDustThreshold(uint64_t val) { m_currency.m_defaultDustThreshold = val; return *this; }

  CurrencyBuilder& difficultyTarget(uint64_t val) { m_currency.m_difficultyTarget = val; return *this; }
  CurrencyBuilder& difficultyWindow(size_t val);
  CurrencyBuilder& difficultyLag(size_t val) { m_currency.m_difficultyLag = val; return *this; }
  CurrencyBuilder& difficultyCut(size_t val) { m_currency.m_difficultyCut = val; return *this; }

  CurrencyBuilder& maxBlockSizeInitial(size_t val) { m_currency.m_maxBlockSizeInitial = val; return *this; }
  CurrencyBuilder& maxBlockSizeGrowthSpeedNumerator(uint64_t val) { m_currency.m_maxBlockSizeGrowthSpeedNumerator = val; return *this; }
  CurrencyBuilder& maxBlockSizeGrowthSpeedDenominator(uint64_t val) { m_currency.m_maxBlockSizeGrowthSpeedDenominator = val; return *this; }

  CurrencyBuilder& lockedTxAllowedDeltaSeconds(uint64_t val) { m_currency.m_lockedTxAllowedDeltaSeconds = val; return *this; }
  CurrencyBuilder& lockedTxAllowedDeltaBlocks(size_t val) { m_currency.m_lockedTxAllowedDeltaBlocks = val; return *this; }

  CurrencyBuilder& mempoolTxLiveTime(uint64_t val) { m_currency.m_mempoolTxLiveTime = val; return *this; }
  CurrencyBuilder& mempoolTxFromAltBlockLiveTime(uint64_t val) { m_currency.m_mempoolTxFromAltBlockLiveTime = val; return *this; }
  CurrencyBuilder& numberOfPeriodsToForgetTxDeletedFromPool(uint64_t val) { m_currency.m_numberOfPeriodsToForgetTxDeletedFromPool = val; return *this; }

  CurrencyBuilder& fusionTxMaxSize(size_t val) { m_currency.m_fusionTxMaxSize = val; return *this; }
  CurrencyBuilder& fusionTxMinInputCount(size_t val) { m_currency.m_fusionTxMinInputCount = val; return *this; }
  CurrencyBuilder& fusionTxMinInOutCountRatio(size_t val) { m_currency.m_fusionTxMinInOutCountRatio = val; return *this; }

  CurrencyBuilder& upgradeHeightV2(uint64_t val) { m_currency.m_upgradeHeightV2 = static_cast<uint32_t>(val); return *this; }
  CurrencyBuilder& upgradeHeightV3(uint64_t val) { m_currency.m_upgradeHeightV3 = static_cast<uint32_t>(val); return *this; }
  CurrencyBuilder& upgradeHeightV4(uint64_t val) { m_currency.m_upgradeHeightV4 = static_cast<uint32_t>(val); return *this; }
  CurrencyBuilder& upgradeHeightV5(uint64_t val) { m_currency.m_upgradeHeightV5 = static_cast<uint32_t>(val); return *this; }
  CurrencyBuilder& upgradeVotingThreshold(unsigned int val);
  CurrencyBuilder& upgradeVotingWindow(size_t val) { m_currency.m_upgradeVotingWindow = static_cast<uint32_t>(val); return *this; }
  CurrencyBuilder& upgradeWindow(size_t val);

  CurrencyBuilder& blocksFileName(const std::string& val) { m_currency.m_blocksFileName = val; return *this; }
  CurrencyBuilder& blocksCacheFileName(const std::string& val) { m_currency.m_blocksCacheFileName = val; return *this; }
  CurrencyBuilder& blockIndexesFileName(const std::string& val) { m_currency.m_blockIndexesFileName = val; return *this; }
  CurrencyBuilder& txPoolFileName(const std::string& val) { m_currency.m_txPoolFileName = val; return *this; }
  CurrencyBuilder& blockchainIndicesFileName(const std::string& val) { m_currency.m_blockchainIndicesFileName = val; return *this; }
  
  CurrencyBuilder& testnet(bool val) { m_currency.m_testnet = val; return *this; }

private:
  Currency m_currency;
};

}
