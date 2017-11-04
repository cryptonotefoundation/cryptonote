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

#include "BlockValidation.h"
#include "TestGenerator.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "Common/StringTools.h"

using namespace Common;
using namespace Crypto;
using namespace CryptoNote;

#define BLOCK_VALIDATION_INIT_GENERATE()                                                                               \
  GENERATE_ACCOUNT(miner_account);                                                                                     \
  MAKE_GENESIS_BLOCK(events, blk_0, miner_account, 1338224400);

namespace {
bool lift_up_difficulty(const CryptoNote::Currency& currency, std::vector<test_event_entry>& events,
                        std::vector<uint64_t>& timestamps,
                        std::vector<CryptoNote::Difficulty>& cummulative_difficulties, test_generator& generator,
                        size_t new_block_count, const CryptoNote::BlockTemplate blk_last,
                        const CryptoNote::AccountBase& miner_account, uint8_t block_major_version) {
  CryptoNote::Difficulty commulative_diffic = cummulative_difficulties.empty() ? 0 : cummulative_difficulties.back();
  CryptoNote::BlockTemplate blk_prev = blk_last;
  for (size_t i = 0; i < new_block_count; ++i) {
    CryptoNote::BlockTemplate blk_next;
    CryptoNote::Difficulty diffic = currency.nextDifficulty(timestamps, cummulative_difficulties);
    if (!generator.constructBlockManually(blk_next, blk_prev, miner_account,
                                          test_generator::bf_major_ver | test_generator::bf_timestamp |
                                              test_generator::bf_diffic,
                                          block_major_version, 0, blk_prev.timestamp, Crypto::Hash(), diffic)) {
      return false;
    }

    commulative_diffic += diffic;
    if (timestamps.size() == currency.difficultyWindow()) {
      timestamps.erase(timestamps.begin());
      cummulative_difficulties.erase(cummulative_difficulties.begin());
    }
    timestamps.push_back(blk_next.timestamp);
    cummulative_difficulties.push_back(commulative_diffic);

    events.push_back(blk_next);
    blk_prev = blk_next;
  }

  return true;
}

bool getParentBlockSize(const CryptoNote::BlockTemplate& block, size_t& size) {
  auto serializer = CryptoNote::makeParentBlockSerializer(block, false, false);
  if (!CryptoNote::getObjectBinarySize(serializer, size)) {
    LOG_ERROR("Failed to get size of parent block");
    return false;
  }
  return true;
}

bool adjustParentBlockSize(CryptoNote::BlockTemplate& block, size_t targetSize) {
  size_t parentBlockSize;
  if (!getParentBlockSize(block, parentBlockSize)) {
    return false;
  }

  if (parentBlockSize > targetSize) {
    LOG_ERROR("Parent block size is " << parentBlockSize << " bytes that is already greater than target size "
                                      << targetSize << " bytes");
    return false;
  }

  block.parentBlock.baseTransaction.extra.resize(block.parentBlock.baseTransaction.extra.size() +
                                                 (targetSize - parentBlockSize));

  if (!getParentBlockSize(block, parentBlockSize)) {
    return false;
  }

  if (parentBlockSize > targetSize) {
    if (block.parentBlock.baseTransaction.extra.size() < parentBlockSize - targetSize) {
      LOG_ERROR("Failed to adjust parent block size to " << targetSize);
      return false;
    }

    block.parentBlock.baseTransaction.extra.resize(block.parentBlock.baseTransaction.extra.size() -
                                                   (parentBlockSize - targetSize));

    if (!getParentBlockSize(block, parentBlockSize)) {
      return false;
    }

    if (parentBlockSize + 1 == targetSize) {
      block.timestamp = std::max(block.timestamp, UINT64_C(1)) << 7;
      if (!getParentBlockSize(block, parentBlockSize)) {
        return false;
      }
    }
  }

  if (parentBlockSize != targetSize) {
    LOG_ERROR("Failed to adjust parent block size to " << targetSize);
    return false;
  }

  return true;
}

void clearTransaction(CryptoNote::Transaction& tx) {
  tx.version = 0;
  tx.unlockTime = 0;
  tx.inputs.clear();
  tx.outputs.clear();
  tx.extra.clear();
  tx.signatures.clear();
}
}

bool TestBlockMajorVersionAccepted::generate(std::vector<test_event_entry>& events) const {
  TestGenerator bg(*m_currency, events);
  bg.generateBlocks(1, m_blockMajorVersion);
  DO_CALLBACK(events, "check_block_accepted");
  return true;
}

bool TestBlockMajorVersionRejected::generate(std::vector<test_event_entry>& events) const {
  TestGenerator bg(*m_currency, events);
  bg.generateBlocks(1, m_blockGeneratedVersion);
  DO_CALLBACK(events, "check_block_purged");
  return true;
}

bool TestBlockBigMinorVersion::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  CryptoNote::BlockTemplate blk_1;
  generator.constructBlockManually(blk_1, blk_0, miner_account,
                                   test_generator::bf_major_ver | test_generator::bf_minor_ver, m_blockMajorVersion,
                                   BLOCK_MINOR_VERSION_0 + 1);

  events.push_back(blk_1);

  DO_CALLBACK(events, "check_block_accepted");

  return true;
}

bool gen_block_ts_not_checked::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  generator.defaultMajorVersion = m_blockMajorVersion;

  REWIND_BLOCKS_N(events, blk_0r, blk_0, miner_account, m_currency->timestampCheckWindow() - 2);

  BlockTemplate blk_1;
  generator.constructBlockManually(blk_1, blk_0r, miner_account,
                                   test_generator::bf_major_ver | test_generator::bf_timestamp, m_blockMajorVersion, 0,
                                   blk_0.timestamp - 60 * 60);
  events.push_back(blk_1);

  DO_CALLBACK(events, "check_block_accepted");

  return true;
}

bool gen_block_ts_in_past::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  generator.defaultMajorVersion = m_blockMajorVersion;

  REWIND_BLOCKS_N(events, blk_0r, blk_0, miner_account, m_currency->timestampCheckWindow() - 1);

  uint64_t ts_below_median = boost::get<BlockTemplate>(events[m_currency->timestampCheckWindow() / 2 - 1]).timestamp;
  BlockTemplate blk_1;
  generator.constructBlockManually(blk_1, blk_0r, miner_account,
                                   test_generator::bf_major_ver | test_generator::bf_timestamp, m_blockMajorVersion, 0,
                                   ts_below_median);
  events.push_back(blk_1);

  DO_CALLBACK(events, "check_block_purged");

  return true;
}

bool gen_block_ts_in_future_rejected::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  BlockTemplate blk_1;
  generator.constructBlockManually(blk_1, blk_0, miner_account,
                                   test_generator::bf_major_ver | test_generator::bf_timestamp, m_blockMajorVersion, 0,
                                   time(NULL) + 60 * 60 + m_currency->blockFutureTimeLimit());
  events.push_back(blk_1);

  DO_CALLBACK(events, "check_block_purged");

  return true;
}

bool gen_block_ts_in_future_accepted::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  BlockTemplate blk_1;
  generator.constructBlockManually(blk_1, blk_0, miner_account,
                                   test_generator::bf_major_ver | test_generator::bf_timestamp, m_blockMajorVersion, 0,
                                   time(NULL) - 60 + m_currency->blockFutureTimeLimit());
  events.push_back(blk_1);

  DO_CALLBACK(events, "check_block_accepted");

  return true;
}

bool gen_block_invalid_prev_id::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  BlockTemplate blk_1;
  Crypto::Hash prev_id = getBlockHash(blk_0);
  reinterpret_cast<char&>(prev_id) ^= 1;
  generator.constructBlockManually(blk_1, blk_0, miner_account,
                                   test_generator::bf_major_ver | test_generator::bf_prev_id, m_blockMajorVersion, 0, 0,
                                   prev_id);
  events.push_back(blk_1);

  DO_CALLBACK(events, "check_block_purged");

  return true;
}

bool gen_block_invalid_prev_id::check_block_verification_context(std::error_code bve, size_t event_idx,
                                                                 const CryptoNote::BlockTemplate& /*blk*/) {
  if (1 == event_idx)
    return bve == CryptoNote::error::AddBlockErrorCode::REJECTED_AS_ORPHANED;
  else
    return bve == CryptoNote::error::AddBlockErrorCode::ADDED_TO_MAIN;
}

bool gen_block_invalid_nonce::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  std::vector<uint64_t> timestamps;
  std::vector<Difficulty> commulative_difficulties;
  if (!lift_up_difficulty(*m_currency, events, timestamps, commulative_difficulties, generator, 2, blk_0, miner_account,
                          m_blockMajorVersion)) {
    return false;
  }

  // Create invalid nonce
  Difficulty diffic = m_currency->nextDifficulty(timestamps, commulative_difficulties);
  assert(1 < diffic);
  const BlockTemplate& blk_last = boost::get<BlockTemplate>(events.back());
  uint64_t timestamp = blk_last.timestamp;
  BlockTemplate blk_3;
  do {
    ++timestamp;
    clearTransaction(blk_3.baseTransaction);
    if (!generator.constructBlockManually(blk_3, blk_last, miner_account,
                                          test_generator::bf_major_ver | test_generator::bf_diffic |
                                              test_generator::bf_timestamp,
                                          m_blockMajorVersion, 0, timestamp, Crypto::Hash(), diffic))
      return false;
  } while (0 == blk_3.nonce);
  --blk_3.nonce;
  events.push_back(blk_3);

  return true;
}

bool gen_block_no_miner_tx::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  Transaction miner_tx;
  clearTransaction(miner_tx);

  BlockTemplate blk_1;
  generator.constructBlockManually(blk_1, blk_0, miner_account,
                                   test_generator::bf_major_ver | test_generator::bf_miner_tx, m_blockMajorVersion, 0,
                                   0, Crypto::Hash(), 0, miner_tx);
  events.push_back(blk_1);

  DO_CALLBACK(events, "check_block_purged");

  return true;
}

bool gen_block_unlock_time_is_low::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  MAKE_MINER_TX_MANUALLY(miner_tx, blk_0);
  --miner_tx.unlockTime;

  BlockTemplate blk_1;
  generator.constructBlockManually(blk_1, blk_0, miner_account,
                                   test_generator::bf_major_ver | test_generator::bf_miner_tx, m_blockMajorVersion, 0,
                                   0, Crypto::Hash(), 0, miner_tx);
  events.push_back(blk_1);

  DO_CALLBACK(events, "check_block_purged");

  return true;
}

bool gen_block_unlock_time_is_high::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  MAKE_MINER_TX_MANUALLY(miner_tx, blk_0);
  ++miner_tx.unlockTime;

  BlockTemplate blk_1;
  generator.constructBlockManually(blk_1, blk_0, miner_account,
                                   test_generator::bf_major_ver | test_generator::bf_miner_tx, m_blockMajorVersion, 0,
                                   0, Crypto::Hash(), 0, miner_tx);
  events.push_back(blk_1);

  DO_CALLBACK(events, "check_block_purged");

  return true;
}

bool gen_block_unlock_time_is_timestamp_in_past::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  MAKE_MINER_TX_MANUALLY(miner_tx, blk_0);
  miner_tx.unlockTime = blk_0.timestamp - 10 * 60;

  BlockTemplate blk_1;
  generator.constructBlockManually(blk_1, blk_0, miner_account,
                                   test_generator::bf_major_ver | test_generator::bf_miner_tx, m_blockMajorVersion, 0,
                                   0, Crypto::Hash(), 0, miner_tx);
  events.push_back(blk_1);

  DO_CALLBACK(events, "check_block_purged");

  return true;
}

bool gen_block_unlock_time_is_timestamp_in_future::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  MAKE_MINER_TX_MANUALLY(miner_tx, blk_0);
  miner_tx.unlockTime = blk_0.timestamp + 3 * m_currency->minedMoneyUnlockWindow() * m_currency->difficultyTarget();

  BlockTemplate blk_1;
  generator.constructBlockManually(blk_1, blk_0, miner_account,
                                   test_generator::bf_major_ver | test_generator::bf_miner_tx, m_blockMajorVersion, 0,
                                   0, Crypto::Hash(), 0, miner_tx);
  events.push_back(blk_1);

  DO_CALLBACK(events, "check_block_purged");

  return true;
}

bool gen_block_height_is_low::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  MAKE_MINER_TX_MANUALLY(miner_tx, blk_0);
  boost::get<BaseInput>(miner_tx.inputs[0]).blockIndex--;

  BlockTemplate blk_1;
  generator.constructBlockManually(blk_1, blk_0, miner_account,
                                   test_generator::bf_major_ver | test_generator::bf_miner_tx, m_blockMajorVersion, 0,
                                   0, Crypto::Hash(), 0, miner_tx);
  events.push_back(blk_1);

  DO_CALLBACK(events, "check_block_purged");

  return true;
}

bool gen_block_height_is_high::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  MAKE_MINER_TX_MANUALLY(miner_tx, blk_0);
  boost::get<BaseInput>(miner_tx.inputs[0]).blockIndex++;

  BlockTemplate blk_1;
  generator.constructBlockManually(blk_1, blk_0, miner_account,
                                   test_generator::bf_major_ver | test_generator::bf_miner_tx, m_blockMajorVersion, 0,
                                   0, Crypto::Hash(), 0, miner_tx);
  events.push_back(blk_1);

  DO_CALLBACK(events, "check_block_purged");

  return true;
}

bool gen_block_miner_tx_has_2_tx_gen_in::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();
  generator.defaultMajorVersion = m_blockMajorVersion;
  MAKE_NEXT_BLOCK(events, blk_0f, blk_0, miner_account);

  MAKE_MINER_TX_MANUALLY(miner_tx, blk_0f);

  BaseInput in;
  in.blockIndex = CachedBlock(blk_0f).getBlockIndex() + 1;
  miner_tx.inputs.push_back(in);

  BlockTemplate blk_1;
  generator.constructBlockManually(blk_1, blk_0f, miner_account,
                                   test_generator::bf_major_ver | test_generator::bf_miner_tx, m_blockMajorVersion, 0,
                                   0, Crypto::Hash(), 0, miner_tx);
  events.push_back(blk_1);

  DO_CALLBACK(events, "check_block_purged");

  return true;
}

bool gen_block_miner_tx_has_2_in::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();
  generator.defaultMajorVersion = m_blockMajorVersion;

  MAKE_NEXT_BLOCK(events, blk_0f, blk_0, miner_account);

  REWIND_BLOCKS(events, blk_0r, blk_0f, miner_account);

  GENERATE_ACCOUNT(alice);

  TransactionSourceEntry se;
  se.amount = blk_0f.baseTransaction.outputs[0].amount;
  se.outputs.push_back(std::make_pair(0, boost::get<KeyOutput>(blk_0f.baseTransaction.outputs[0].target).key));
  se.realOutput = 0;
  se.realTransactionPublicKey = getTransactionPublicKeyFromExtra(blk_0f.baseTransaction.extra);
  se.realOutputIndexInTransaction = 0;
  std::vector<TransactionSourceEntry> sources;
  sources.push_back(se);

  TransactionDestinationEntry de;
  de.addr = miner_account.getAccountKeys().address;
  de.amount = se.amount;
  std::vector<TransactionDestinationEntry> destinations;
  destinations.push_back(de);

  Transaction tmp_tx;
  if (!constructTransaction(miner_account.getAccountKeys(), sources, destinations, std::vector<uint8_t>(), tmp_tx, 0,
                            m_logger))
    return false;

  MAKE_MINER_TX_MANUALLY(miner_tx, blk_0f);
  miner_tx.inputs.push_back(tmp_tx.inputs[0]);

  BlockTemplate blk_1;
  generator.constructBlockManually(blk_1, blk_0r, miner_account,
                                   test_generator::bf_major_ver | test_generator::bf_miner_tx, m_blockMajorVersion, 0,
                                   0, Crypto::Hash(), 0, miner_tx);
  events.push_back(blk_1);

  DO_CALLBACK(events, "check_block_purged");

  return true;
}

bool gen_block_miner_tx_with_txin_to_key::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  generator.defaultMajorVersion = m_blockMajorVersion;

  // This block has only one output
  BlockTemplate blk_1;
  generator.constructBlockManually(blk_1, blk_0, miner_account, test_generator::bf_none);
  events.push_back(blk_1);

  REWIND_BLOCKS(events, blk_1r, blk_1, miner_account);

  TransactionSourceEntry se;
  se.amount = blk_1.baseTransaction.outputs[0].amount;
  se.outputs.push_back(std::make_pair(0, boost::get<KeyOutput>(blk_1.baseTransaction.outputs[0].target).key));
  se.realOutput = 0;
  se.realTransactionPublicKey = getTransactionPublicKeyFromExtra(blk_1.baseTransaction.extra);
  se.realOutputIndexInTransaction = 0;
  std::vector<TransactionSourceEntry> sources;
  sources.push_back(se);

  TransactionDestinationEntry de;
  de.addr = miner_account.getAccountKeys().address;
  de.amount = se.amount;
  std::vector<TransactionDestinationEntry> destinations;
  destinations.push_back(de);

  Transaction tmp_tx;
  if (!constructTransaction(miner_account.getAccountKeys(), sources, destinations, std::vector<uint8_t>(), tmp_tx, 0,
                            m_logger))
    return false;

  MAKE_MINER_TX_MANUALLY(miner_tx, blk_1);
  miner_tx.inputs[0] = tmp_tx.inputs[0];

  BlockTemplate blk_2;
  generator.constructBlockManually(blk_2, blk_1r, miner_account,
                                   test_generator::bf_major_ver | test_generator::bf_miner_tx, m_blockMajorVersion, 0,
                                   0, Crypto::Hash(), 0, miner_tx);
  events.push_back(blk_2);

  DO_CALLBACK(events, "check_block_purged");

  return true;
}

bool gen_block_miner_tx_out_is_small::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  MAKE_MINER_TX_MANUALLY(miner_tx, blk_0);
  miner_tx.outputs[0].amount /= 2;

  BlockTemplate blk_1;
  generator.constructBlockManually(blk_1, blk_0, miner_account,
                                   test_generator::bf_major_ver | test_generator::bf_miner_tx, m_blockMajorVersion, 0,
                                   0, Crypto::Hash(), 0, miner_tx);
  events.push_back(blk_1);

  DO_CALLBACK(events, "check_block_purged");

  return true;
}

bool gen_block_miner_tx_out_is_big::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  MAKE_MINER_TX_MANUALLY(miner_tx, blk_0);
  miner_tx.outputs[0].amount *= 2;

  BlockTemplate blk_1;
  generator.constructBlockManually(blk_1, blk_0, miner_account,
                                   test_generator::bf_major_ver | test_generator::bf_miner_tx, m_blockMajorVersion, 0,
                                   0, Crypto::Hash(), 0, miner_tx);
  events.push_back(blk_1);

  DO_CALLBACK(events, "check_block_purged");

  return true;
}

bool gen_block_miner_tx_has_no_out::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  MAKE_MINER_TX_MANUALLY(miner_tx, blk_0);
  miner_tx.outputs.clear();

  BlockTemplate blk_1;
  generator.constructBlockManually(blk_1, blk_0, miner_account,
                                   test_generator::bf_major_ver | test_generator::bf_miner_tx, m_blockMajorVersion, 0,
                                   0, Crypto::Hash(), 0, miner_tx);
  events.push_back(blk_1);

  DO_CALLBACK(events, "check_block_purged");

  return true;
}

bool gen_block_miner_tx_has_out_to_alice::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  GENERATE_ACCOUNT(alice);

  KeyPair txkey;
  MAKE_MINER_TX_AND_KEY_MANUALLY(miner_tx, blk_0, &txkey);

  Crypto::KeyDerivation derivation;
  Crypto::PublicKey out_eph_public_key;
  Crypto::generate_key_derivation(alice.getAccountKeys().address.viewPublicKey, txkey.secretKey, derivation);
  Crypto::derive_public_key(derivation, 1, alice.getAccountKeys().address.spendPublicKey, out_eph_public_key);

  TransactionOutput out_to_alice;
  out_to_alice.amount = miner_tx.outputs[0].amount / 2;
  miner_tx.outputs[0].amount -= out_to_alice.amount;
  out_to_alice.target = KeyOutput{out_eph_public_key};
  miner_tx.outputs.push_back(out_to_alice);

  BlockTemplate blk_1;
  generator.constructBlockManually(blk_1, blk_0, miner_account,
                                   test_generator::bf_major_ver | test_generator::bf_miner_tx, m_blockMajorVersion, 0,
                                   0, Crypto::Hash(), 0, miner_tx);
  events.push_back(blk_1);

  DO_CALLBACK(events, "check_block_accepted");

  return true;
}

bool gen_block_has_invalid_tx::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  generator.defaultMajorVersion = m_blockMajorVersion;

  std::vector<Crypto::Hash> tx_hashes;
  tx_hashes.push_back(Crypto::Hash());

  BlockTemplate blk_1;
  generator.constructBlockManuallyTx(blk_1, blk_0, miner_account, tx_hashes, 0);
  CryptoNote::Transaction tx;
  tx.version = 1;
  events.push_back(populateBlock(blk_1, {tx}));

  DO_CALLBACK(events, "check_block_purged");

  return true;
}

bool gen_block_is_too_big::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();
  generator.defaultMajorVersion = m_blockMajorVersion;

  BlockTemplate blk_1;
  if (!generator.constructMaxSizeBlock(blk_1, blk_0, miner_account)) {
    return false;
  }

  blk_1.baseTransaction.extra.resize(blk_1.baseTransaction.extra.size() + 1);
  events.push_back(blk_1);

  DO_CALLBACK(events, "check_block_purged");

  return true;
}

bool TestBlockCumulativeSizeExceedsLimit::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  generator.defaultMajorVersion = m_blockMajorVersion;

  BlockTemplate prevBlock = blk_0;
  for (size_t height = 1; height < 1000; ++height) {
    BlockTemplate block;
    if (!generator.constructMaxSizeBlock(block, prevBlock, miner_account)) {
      return false;
    }

    prevBlock = block;

    if (getObjectBinarySize(block.baseTransaction) <= m_currency->maxBlockCumulativeSize(height)) {
      events.push_back(block);
    } else {
      DO_CALLBACK(events, "markInvalidBlock");
      events.push_back(block);
      return true;
    }
  }

  return false;
}

gen_block_invalid_binary_format::gen_block_invalid_binary_format(uint8_t blockMajorVersion)
    : m_corrupt_blocks_begin_idx(0), m_blockMajorVersion(blockMajorVersion) {
  CryptoNote::CurrencyBuilder currencyBuilder(m_logger);
  currencyBuilder.upgradeHeightV2(blockMajorVersion == CryptoNote::BLOCK_MAJOR_VERSION_1 ? IUpgradeDetector::UNDEF_HEIGHT : UINT32_C(0));
  m_currency.reset(new Currency(currencyBuilder.currency()));

  REGISTER_CALLBACK("check_all_blocks_purged", gen_block_invalid_binary_format::check_all_blocks_purged);
  REGISTER_CALLBACK("corrupt_blocks_boundary", gen_block_invalid_binary_format::corrupt_blocks_boundary);
}

bool gen_block_invalid_binary_format::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  generator.defaultMajorVersion = m_blockMajorVersion;

  std::vector<uint64_t> timestamps;
  std::vector<Difficulty> cummulative_difficulties;
  Difficulty cummulative_diff = 1;

  // Unlock blk_0 outputs
  BlockTemplate blk_last = blk_0;
  assert(m_currency->minedMoneyUnlockWindow() < m_currency->difficultyWindow());
  for (size_t i = 0; i < m_currency->minedMoneyUnlockWindow(); ++i) {
    MAKE_NEXT_BLOCK(events, blk_curr, blk_last, miner_account);
    timestamps.push_back(blk_curr.timestamp);
    cummulative_difficulties.push_back(++cummulative_diff);
    blk_last = blk_curr;
  }

  // Lifting up takes a while
  Difficulty diffic;
  do {
    blk_last = boost::get<BlockTemplate>(events.back());
    diffic = m_currency->nextDifficulty(timestamps, cummulative_difficulties);
    if (!lift_up_difficulty(*m_currency, events, timestamps, cummulative_difficulties, generator, 1, blk_last,
                            miner_account, m_blockMajorVersion)) {
      return false;
    }
    std::cout << "Block #" << events.size() << ", difficulty: " << diffic << std::endl;
  } while (diffic < 1500);

  blk_last = boost::get<BlockTemplate>(events.back());
  MAKE_TX(events, tx_0, miner_account, miner_account, MK_COINS(120), boost::get<BlockTemplate>(events[1]));
  DO_CALLBACK(events, "corrupt_blocks_boundary");

  BlockTemplate blk_test;
  std::vector<Crypto::Hash> tx_hashes;
  tx_hashes.push_back(getObjectHash(tx_0));
  size_t txs_size = getObjectBinarySize(tx_0);
  diffic = m_currency->nextDifficulty(timestamps, cummulative_difficulties);
  if (!generator.constructBlockManually(
          blk_test, blk_last, miner_account, test_generator::bf_major_ver | test_generator::bf_diffic |
                                                 test_generator::bf_timestamp | test_generator::bf_tx_hashes,
          m_blockMajorVersion, 0, blk_last.timestamp, Crypto::Hash(), diffic, Transaction(), tx_hashes, txs_size))
    return false;

  BinaryArray blob = toBinaryArray(blk_test);
  for (size_t i = 0; i < blob.size(); ++i) {
    for (size_t bit_idx = 0; bit_idx < sizeof(BinaryArray::value_type) * 8; ++bit_idx) {
      serialized_block sr_block(blob);
      BinaryArray::value_type& ch = sr_block.data[i];
      ch ^= 1 << bit_idx;

      events.push_back(sr_block);
    }
  }

  DO_CALLBACK(events, "check_all_blocks_purged");

  return true;
}

bool gen_block_invalid_binary_format::check_block_verification_context(std::error_code bve, size_t event_idx,
                                                                       const CryptoNote::BlockTemplate& blk) {
  using CryptoNote::error::AddBlockErrorCode;
  if (0 == m_corrupt_blocks_begin_idx || event_idx < m_corrupt_blocks_begin_idx) {
    return bve == AddBlockErrorCode::ADDED_TO_MAIN;
  } else {
    return bve == AddBlockErrorCode::ALREADY_EXISTS || bve == AddBlockErrorCode::REJECTED_AS_ORPHANED;
  }
}

bool gen_block_invalid_binary_format::corrupt_blocks_boundary(CryptoNote::Core& c, size_t ev_index,
                                                              const std::vector<test_event_entry>& events) {
  m_corrupt_blocks_begin_idx = ev_index + 1;
  return true;
}

bool gen_block_invalid_binary_format::check_all_blocks_purged(CryptoNote::Core& c, size_t ev_index,
                                                              const std::vector<test_event_entry>& events) {
  DEFINE_TESTS_ERROR_CONTEXT("gen_block_invalid_binary_format::check_all_blocks_purged");

  CHECK_EQ(1, c.getPoolTransactionCount());
  CHECK_EQ(m_corrupt_blocks_begin_idx - 2, c.getTopBlockIndex() + 1);

  return true;
}

bool TestMaxSizeOfParentBlock::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  CryptoNote::BlockTemplate blk_1;
  generator.constructBlockManually(blk_1, blk_0, miner_account, test_generator::bf_major_ver, BLOCK_MAJOR_VERSION_2);
  if (!adjustParentBlockSize(blk_1, 2 * 1024)) {
    return false;
  }
  events.push_back(blk_1);

  DO_CALLBACK(events, "check_block_accepted");

  return true;
}

bool TestBigParentBlock::generate(std::vector<test_event_entry>& events) const {
  BLOCK_VALIDATION_INIT_GENERATE();

  CryptoNote::BlockTemplate blk_1;
  generator.constructBlockManually(blk_1, blk_0, miner_account, test_generator::bf_major_ver, BLOCK_MAJOR_VERSION_2);
  if (!adjustParentBlockSize(blk_1, 2 * 1024 + 1)) {
    return false;
  }
  events.push_back(blk_1);

  DO_CALLBACK(events, "check_block_purged");

  return true;
}

namespace {
template <typename MutateFunc>
bool GenerateAndMutateBlockV2(const CryptoNote::Currency& currency, std::vector<test_event_entry>& events,
                              const std::string& callback, MutateFunc mf) {
  TestGenerator bg(currency, events);

  CryptoNote::BlockTemplate blk_1;
  bg.generator.constructBlockManually(blk_1, bg.lastBlock, bg.minerAccount, test_generator::bf_major_ver,
                                      BLOCK_MAJOR_VERSION_2);

  mf(blk_1);

  events.push_back(blk_1);
  bg.addCallback(callback);

  return true;
}
}

bool TestBlock2ExtraEmpty::generate(std::vector<test_event_entry>& events) const {
  return GenerateAndMutateBlockV2(*m_currency, events, "check_block_purged", [](CryptoNote::BlockTemplate& blk) {
    blk.parentBlock.baseTransaction.extra.clear();
  });
}

bool TestBlock2ExtraWithoutMMTag::generate(std::vector<test_event_entry>& events) const {
  return GenerateAndMutateBlockV2(*m_currency, events, "check_block_purged", [](CryptoNote::BlockTemplate& blk) {
    blk.parentBlock.baseTransaction.extra.clear();
    CryptoNote::addExtraNonceToTransactionExtra(blk.parentBlock.baseTransaction.extra, asBinaryArray("0xdeadbeef"));
  });
}

bool TestBlock2ExtraWithGarbage::generate(std::vector<test_event_entry>& events) const {
  return GenerateAndMutateBlockV2(*m_currency, events, "check_block_accepted", [](CryptoNote::BlockTemplate& blk) {
    CryptoNote::addExtraNonceToTransactionExtra(blk.parentBlock.baseTransaction.extra, asBinaryArray("0xdeadbeef"));
    blk.parentBlock.baseTransaction.extra.push_back(0xde);
    blk.parentBlock.baseTransaction.extra.push_back(0xad);
    blk.parentBlock.baseTransaction.extra.push_back(0xbe);
    blk.parentBlock.baseTransaction.extra.push_back(0xef);
  });
}
