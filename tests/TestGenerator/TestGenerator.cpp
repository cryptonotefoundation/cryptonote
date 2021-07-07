// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
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

#include "TestGenerator.h"

#include <Common/Math.h>
#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/Miner.h"
#include "CryptoNoteCore/TransactionExtra.h"
#include "CryptoNoteCore/CryptoNoteTools.h"

using namespace std;
using namespace CryptoNote;

#ifndef CHECK_AND_ASSERT_MES
#define CHECK_AND_ASSERT_MES(expr, fail_ret_val, message)   do{if(!(expr)) {std::cerr << message << std::endl; return fail_ret_val;};}while(0)
#endif


void test_generator::getBlockchain(std::vector<BlockInfo>& blockchain, const Crypto::Hash& head, size_t n) const {
  Crypto::Hash curr = head;
  while (curr != NULL_HASH && blockchain.size() < n) {
    auto it = m_blocksInfo.find(curr);
    if (m_blocksInfo.end() == it) {
      throw std::runtime_error("block hash wasn't found");
    }

    blockchain.push_back(it->second);
    curr = it->second.previousBlockHash;
  }

  std::reverse(blockchain.begin(), blockchain.end());
}

void test_generator::getLastNBlockSizes(std::vector<size_t>& blockSizes, const Crypto::Hash& head, size_t n) const {
  std::vector<BlockInfo> blockchain;
  getBlockchain(blockchain, head, n);
  for (auto& bi : blockchain) {
    blockSizes.push_back(bi.blockSize);
  }
}

uint64_t test_generator::getAlreadyGeneratedCoins(const Crypto::Hash& blockId) const {
  auto it = m_blocksInfo.find(blockId);
  if (it == m_blocksInfo.end()) {
    throw std::runtime_error("block hash wasn't found");
  }

  return it->second.alreadyGeneratedCoins;
}

uint64_t test_generator::getAlreadyGeneratedCoins(const CryptoNote::Block& blk) const {
  Crypto::Hash blkHash;
  get_block_hash(blk, blkHash);
  return getAlreadyGeneratedCoins(blkHash);
}

void test_generator::addBlock(const CryptoNote::Block& blk, size_t tsxSize, uint64_t fee,
                              std::vector<size_t>& blockSizes, uint64_t alreadyGeneratedCoins) {
  const size_t blockSize = tsxSize + getObjectBinarySize(blk.baseTransaction);
  int64_t emissionChange;
  uint64_t blockReward;
  m_currency.getBlockReward(blk.majorVersion, Common::medianValue(blockSizes), blockSize, alreadyGeneratedCoins, fee, blockReward, emissionChange);
  m_blocksInfo[get_block_hash(blk)] = BlockInfo(blk.previousBlockHash, alreadyGeneratedCoins + emissionChange, blockSize);
}

bool test_generator::constructBlock(CryptoNote::Block& blk, uint32_t height, const Crypto::Hash& previousBlockHash,
                                    const CryptoNote::AccountBase& minerAcc, uint64_t timestamp, uint64_t alreadyGeneratedCoins,
                                    std::vector<size_t>& blockSizes, const std::list<CryptoNote::Transaction>& txList) {
  blk.majorVersion = defaultMajorVersion;
  blk.minorVersion = defaultMinorVersion;
  blk.timestamp = timestamp;
  blk.previousBlockHash = previousBlockHash;

  blk.transactionHashes.reserve(txList.size());
  for (const Transaction &tx : txList) {
    Crypto::Hash tx_hash;
    getObjectHash(tx, tx_hash);
    blk.transactionHashes.push_back(tx_hash);
  }

  uint64_t totalFee = 0;
  size_t txsSize = 0;
  for (auto& tx : txList) {
    uint64_t fee = 0;
    bool r = get_tx_fee(tx, fee);
    CHECK_AND_ASSERT_MES(r, false, "wrong transaction passed to construct_block");
    totalFee += fee;
    txsSize += getObjectBinarySize(tx);
  }

  blk.baseTransaction = boost::value_initialized<Transaction>();
  size_t targetBlockSize = txsSize + getObjectBinarySize(blk.baseTransaction);
  while (true) {
    if (!m_currency.constructMinerTx(blk.majorVersion, height, Common::medianValue(blockSizes), alreadyGeneratedCoins, targetBlockSize,
      totalFee, minerAcc.getAccountKeys().address, blk.baseTransaction, BinaryArray(), 10)) {
      return false;
    }

    size_t actualBlockSize = txsSize + getObjectBinarySize(blk.baseTransaction);
    if (targetBlockSize < actualBlockSize) {
      targetBlockSize = actualBlockSize;
    } else if (actualBlockSize < targetBlockSize) {
      size_t delta = targetBlockSize - actualBlockSize;
      blk.baseTransaction.extra.resize(blk.baseTransaction.extra.size() + delta, 0);
      actualBlockSize = txsSize + getObjectBinarySize(blk.baseTransaction);
      if (actualBlockSize == targetBlockSize) {
        break;
      } else {
        CHECK_AND_ASSERT_MES(targetBlockSize < actualBlockSize, false, "Unexpected block size");
        delta = actualBlockSize - targetBlockSize;
        blk.baseTransaction.extra.resize(blk.baseTransaction.extra.size() - delta);
        actualBlockSize = txsSize + getObjectBinarySize(blk.baseTransaction);
        if (actualBlockSize == targetBlockSize) {
          break;
        } else {
          CHECK_AND_ASSERT_MES(actualBlockSize < targetBlockSize, false, "Unexpected block size");
          blk.baseTransaction.extra.resize(blk.baseTransaction.extra.size() + delta, 0);
          targetBlockSize = txsSize + getObjectBinarySize(blk.baseTransaction);
        }
      }
    } else {
      break;
    }
  }

  if (blk.majorVersion >= BLOCK_MAJOR_VERSION_2) {
    blk.parentBlock.majorVersion = BLOCK_MAJOR_VERSION_1;
    blk.parentBlock.minorVersion = BLOCK_MINOR_VERSION_0;
    blk.parentBlock.transactionCount = 1;
    blk.parentBlock.baseTransaction.version = 0;
    blk.parentBlock.baseTransaction.unlockTime = 0;

    CryptoNote::TransactionExtraMergeMiningTag mmTag;
    mmTag.depth = 0;
    if (!CryptoNote::get_aux_block_header_hash(blk, mmTag.merkleRoot)) {
      return false;
    }

    blk.parentBlock.baseTransaction.extra.clear();
    if (!CryptoNote::appendMergeMiningTagToExtra(blk.parentBlock.baseTransaction.extra, mmTag)) {
      return false;
    }
  }

  // Nonce search...
  blk.nonce = 0;
  Crypto::cn_context context;
  while (!miner::find_nonce_for_given_block(context, blk, getTestDifficulty())) {
    blk.timestamp++;
  }

  addBlock(blk, txsSize, totalFee, blockSizes, alreadyGeneratedCoins);

  return true;
}

bool test_generator::constructBlock(CryptoNote::Block& blk, const CryptoNote::AccountBase& minerAcc, uint64_t timestamp) {
  std::vector<size_t> blockSizes;
  std::list<CryptoNote::Transaction> txList;
  return constructBlock(blk, 0, NULL_HASH, minerAcc, timestamp, 0, blockSizes, txList);
}

bool test_generator::constructBlock(CryptoNote::Block& blk, const CryptoNote::Block& blkPrev,
                                    const CryptoNote::AccountBase& minerAcc,
                                    const std::list<CryptoNote::Transaction>& txList/* = std::list<CryptoNote::Transaction>()*/) {
  uint32_t height = boost::get<BaseInput>(blkPrev.baseTransaction.inputs.front()).blockIndex + 1;
  Crypto::Hash previousBlockHash = get_block_hash(blkPrev);
  // Keep difficulty unchanged
  uint64_t timestamp = blkPrev.timestamp + m_currency.difficultyTarget();
  uint64_t alreadyGeneratedCoins = getAlreadyGeneratedCoins(previousBlockHash);
  std::vector<size_t> blockSizes;
  getLastNBlockSizes(blockSizes, previousBlockHash, m_currency.rewardBlocksWindow());

  return constructBlock(blk, height, previousBlockHash, minerAcc, timestamp, alreadyGeneratedCoins, blockSizes, txList);
}

bool test_generator::constructBlockManually(Block& blk, const Block& prevBlock, const AccountBase& minerAcc,
                                            int actualParams/* = bf_none*/, uint8_t majorVer/* = 0*/,
                                            uint8_t minorVer/* = 0*/, uint64_t timestamp/* = 0*/,
                                            const Crypto::Hash& previousBlockHash/* = Crypto::Hash()*/, const difficulty_type& diffic/* = 1*/,
                                            const Transaction& baseTransaction/* = transaction()*/,
                                            const std::vector<Crypto::Hash>& transactionHashes/* = std::vector<Crypto::Hash>()*/,
                                            size_t txsSizes/* = 0*/, uint64_t fee/* = 0*/) {
  blk.majorVersion = actualParams & bf_major_ver ? majorVer  : defaultMajorVersion;
  blk.minorVersion = actualParams & bf_minor_ver ? minorVer  : defaultMinorVersion;
  blk.timestamp    = actualParams & bf_timestamp ? timestamp : prevBlock.timestamp + m_currency.difficultyTarget(); // Keep difficulty unchanged
  blk.previousBlockHash       = actualParams & bf_prev_id   ? previousBlockHash    : get_block_hash(prevBlock);
  blk.transactionHashes     = actualParams & bf_tx_hashes ? transactionHashes  : std::vector<Crypto::Hash>();
  
  blk.parentBlock.baseTransaction.version = 0;
  blk.parentBlock.baseTransaction.unlockTime = 0;

  uint32_t height = get_block_height(prevBlock) + 1;
  uint64_t alreadyGeneratedCoins = getAlreadyGeneratedCoins(prevBlock);
  std::vector<size_t> blockSizes;
  getLastNBlockSizes(blockSizes, get_block_hash(prevBlock), m_currency.rewardBlocksWindow());
  if (actualParams & bf_miner_tx) {
    blk.baseTransaction = baseTransaction;
  } else {
    blk.baseTransaction = boost::value_initialized<Transaction>();
    size_t currentBlockSize = txsSizes + getObjectBinarySize(blk.baseTransaction);
    // TODO: This will work, until size of constructed block is less then m_currency.blockGrantedFullRewardZone()
    if (!m_currency.constructMinerTx(blk.majorVersion, height, Common::medianValue(blockSizes), alreadyGeneratedCoins, currentBlockSize, 0,
        minerAcc.getAccountKeys().address, blk.baseTransaction, BinaryArray(), 1)) {
      return false;
    }
  }

  if (blk.majorVersion >= BLOCK_MAJOR_VERSION_2) {
    blk.parentBlock.majorVersion = BLOCK_MAJOR_VERSION_1;
    blk.parentBlock.minorVersion = BLOCK_MINOR_VERSION_0;
    blk.parentBlock.transactionCount = 1;

    CryptoNote::TransactionExtraMergeMiningTag mmTag;
    mmTag.depth = 0;
    if (!CryptoNote::get_aux_block_header_hash(blk, mmTag.merkleRoot)) {
      return false;
    }

    blk.parentBlock.baseTransaction.extra.clear();
    if (!CryptoNote::appendMergeMiningTagToExtra(blk.parentBlock.baseTransaction.extra, mmTag)) {
      return false;
    }
  }

  difficulty_type aDiffic = actualParams & bf_diffic ? diffic : getTestDifficulty();
  if (1 < aDiffic) {
    fillNonce(blk, aDiffic);
  }

  addBlock(blk, txsSizes, fee, blockSizes, alreadyGeneratedCoins);

  return true;
}

bool test_generator::constructBlockManuallyTx(CryptoNote::Block& blk, const CryptoNote::Block& prevBlock,
                                              const CryptoNote::AccountBase& minerAcc,
                                              const std::vector<Crypto::Hash>& transactionHashes, size_t txsSize) {
  return constructBlockManually(blk, prevBlock, minerAcc, bf_tx_hashes, 0, 0, 0, Crypto::Hash(), 0, Transaction(),
    transactionHashes, txsSize);
}

bool test_generator::constructMaxSizeBlock(CryptoNote::Block& blk, const CryptoNote::Block& blkPrev,
                                           const CryptoNote::AccountBase& minerAccount,
                                           size_t medianBlockCount/* = 0*/,
                                           const std::list<CryptoNote::Transaction>& txList/* = std::list<CryptoNote::Transaction>()*/) {
  std::vector<size_t> blockSizes;
  medianBlockCount = medianBlockCount == 0 ? m_currency.rewardBlocksWindow() : medianBlockCount;
  getLastNBlockSizes(blockSizes, get_block_hash(blkPrev), medianBlockCount);

  size_t median = Common::medianValue(blockSizes);
  size_t blockGrantedFullRewardZone = m_currency.blockGrantedFullRewardZoneByBlockVersion(defaultMajorVersion);
  median = std::max(median, blockGrantedFullRewardZone);

  uint64_t totalFee = 0;
  size_t txsSize = 0;
  std::vector<Crypto::Hash> transactionHashes;
  for (auto& tx : txList) {
    uint64_t fee = 0;
    bool r = get_tx_fee(tx, fee);
    CHECK_AND_ASSERT_MES(r, false, "wrong transaction passed to construct_max_size_block");
    totalFee += fee;
    txsSize += getObjectBinarySize(tx);
    transactionHashes.push_back(getObjectHash(tx));
  }

  Transaction baseTransaction;
  bool r = constructMinerTxBySize(m_currency, baseTransaction, defaultMajorVersion, get_block_height(blkPrev) + 1,
    getAlreadyGeneratedCoins(blkPrev), minerAccount.getAccountKeys().address, blockSizes, 2 * median - txsSize, 2 * median, totalFee);
  if (!r) {
    return false;
  }

  return constructBlockManually(blk, blkPrev, minerAccount, test_generator::bf_miner_tx | test_generator::bf_tx_hashes,
    0, 0, 0, Crypto::Hash(), 0, baseTransaction, transactionHashes, txsSize, totalFee);
}

void fillNonce(CryptoNote::Block& blk, const difficulty_type& diffic) {
  blk.nonce = 0;
  Crypto::cn_context context;
  while (!miner::find_nonce_for_given_block(context, blk, diffic)) {
    blk.timestamp++;
  }
}

bool constructMinerTxManually(const CryptoNote::Currency& currency, uint8_t blockMajorVersion, uint32_t height, uint64_t alreadyGeneratedCoins,
                              const AccountPublicAddress& minerAddress, Transaction& tx, uint64_t fee, KeyPair* pTxKey/* = 0*/) {
  KeyPair txkey = generateKeyPair();
  addTransactionPublicKeyToExtra(tx.extra, txkey.publicKey);

  if (0 != pTxKey) {
    *pTxKey = txkey;
  }

  BaseInput in;
  in.blockIndex = height;
  tx.inputs.push_back(in);

  // This will work, until size of constructed block is less then currency.blockGrantedFullRewardZone()
  int64_t emissionChange;
  uint64_t blockReward;
  if (!currency.getBlockReward(blockMajorVersion, 0, 0, alreadyGeneratedCoins, fee, blockReward, emissionChange)) {
    std::cerr << "Block is too big" << std::endl;
    return false;
  }

  Crypto::KeyDerivation derivation;
  Crypto::PublicKey outEphPublicKey;
  Crypto::generate_key_derivation(minerAddress.viewPublicKey, txkey.secretKey, derivation);
  Crypto::derive_public_key(derivation, 0, minerAddress.spendPublicKey, outEphPublicKey);

  TransactionOutput out;
  out.amount = blockReward;
  out.target = KeyOutput{outEphPublicKey};
  tx.outputs.push_back(out);

  tx.version = CURRENT_TRANSACTION_VERSION;
  tx.unlockTime = height + currency.minedMoneyUnlockWindow();

  return true;
}

bool constructMinerTxBySize(const CryptoNote::Currency& currency, CryptoNote::Transaction& baseTransaction, uint8_t blockMajorVersion, uint32_t height,
                            uint64_t alreadyGeneratedCoins, const CryptoNote::AccountPublicAddress& minerAddress,
                            std::vector<size_t>& blockSizes, size_t targetTxSize, size_t targetBlockSize, uint64_t fee/* = 0*/) {
  if (!currency.constructMinerTx(blockMajorVersion, height, Common::medianValue(blockSizes), alreadyGeneratedCoins, targetBlockSize,
      fee, minerAddress, baseTransaction, CryptoNote::BinaryArray(), 1)) {
    return false;
  }

  size_t currentSize = getObjectBinarySize(baseTransaction);
  size_t tryCount = 0;
  while (targetTxSize != currentSize) {
    ++tryCount;
    if (10 < tryCount) {
      return false;
    }

    if (targetTxSize < currentSize) {
      size_t diff = currentSize - targetTxSize;
      if (diff <= baseTransaction.extra.size()) {
        baseTransaction.extra.resize(baseTransaction.extra.size() - diff);
      } else {
        return false;
      }
    } else {
      size_t diff = targetTxSize - currentSize;
      baseTransaction.extra.resize(baseTransaction.extra.size() + diff);
    }

    currentSize = getObjectBinarySize(baseTransaction);
  }

  return true;
}
