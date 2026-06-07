// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers, The Monero developers
// Copyright (c) 2016-2026, The Karbo developers
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

#include "Blockchain.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <numeric>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <boost/foreach.hpp>
#include "Common/Math.h"
#include "Common/int-util.h"
#include "Common/ShuffleGenerator.h"
#include "Common/StdInputStream.h"
#include "Common/StdOutputStream.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"
#include "Serialization/BinarySerializationTools.h"
#include "CryptoNoteTools.h"
#include "TransactionExtra.h"

#include "../crypto/hash.h"

using namespace Logging;
using namespace Common;

namespace {

std::string appendPath(const std::string& path, const std::string& fileName) {
  std::string result = path;
  if (!result.empty()) {
    result += '/';
  }
  result += fileName;
  return result;
}

} // anonymous namespace

namespace std {
bool operator<(const Crypto::Hash& hash1, const Crypto::Hash& hash2) {
  return memcmp(&hash1, &hash2, Crypto::HASH_SIZE) < 0;
}

bool operator<(const Crypto::KeyImage& keyImage1, const Crypto::KeyImage& keyImage2) {
  return memcmp(&keyImage1, &keyImage2, 32) < 0;
}
} // namespace std

namespace CryptoNote {

namespace {

BlockStatsEntry makeBlockStatsEntry(uint32_t height, const DbBlockMeta& meta, const DbBlockMeta* prevMeta) {
  BlockStatsEntry entry{};
  entry.height = height;
  entry.blockSize = meta.blockCumulativeSize;
  entry.alreadyGeneratedCoins = meta.alreadyGeneratedCoins;
  entry.timestamp = meta.timestamp;
  entry.transactionsCount = meta.txCount > 0 ? meta.txCount - 1 : 0;

  if (prevMeta == nullptr) {
    entry.difficulty = meta.cumulativeDifficulty;
    entry.reward = meta.alreadyGeneratedCoins;
  } else {
    entry.difficulty = meta.cumulativeDifficulty - prevMeta->cumulativeDifficulty;
    entry.reward = meta.alreadyGeneratedCoins - prevMeta->alreadyGeneratedCoins;
  }

  return entry;
}

} // anonymous namespace

// ─── Constructor ─────────────────────────────────────────────────────────────

Blockchain::Blockchain(const Currency& currency, tx_memory_pool& tx_pool,
                       ILogger& logger, uint32_t rejectDeepReorgDepth, bool noBlobs)
  : logger(logger, "Blockchain"),
    m_currency(currency),
    m_tx_pool(tx_pool),
    m_blockView(m_db),
    m_upgradeDetectorV2(currency, m_blockView, BLOCK_MAJOR_VERSION_2, logger),
    m_upgradeDetectorV3(currency, m_blockView, BLOCK_MAJOR_VERSION_3, logger),
    m_upgradeDetectorV4(currency, m_blockView, BLOCK_MAJOR_VERSION_4, logger),
    m_upgradeDetectorV5(currency, m_blockView, BLOCK_MAJOR_VERSION_5, logger),
    m_upgradeDetectorV6(currency, m_blockView, BLOCK_MAJOR_VERSION_6, logger),
    m_checkpoints(logger, rejectDeepReorgDepth),
    m_no_blobs(noBlobs)
{
}

// ─── Observer management ─────────────────────────────────────────────────────

bool Blockchain::addObserver(IBlockchainStorageObserver* observer) {
  return m_observerManager.add(observer);
}

bool Blockchain::removeObserver(IBlockchainStorageObserver* observer) {
  return m_observerManager.remove(observer);
}

// ─── ITransactionValidator ───────────────────────────────────────────────────

bool Blockchain::checkTransactionInputs(const CryptoNote::Transaction& tx, BlockInfo& maxUsedBlock) {
  return checkTransactionInputs(tx, maxUsedBlock.height, maxUsedBlock.id);
}

bool Blockchain::checkTransactionInputs(const CryptoNote::Transaction& tx, BlockInfo& maxUsedBlock, BlockInfo& lastFailed) {
  BlockInfo tail;

  if (maxUsedBlock.empty()) {
    if (!lastFailed.empty() && getCurrentBlockchainHeight() > lastFailed.height &&
        getBlockIdByHeight(lastFailed.height) == lastFailed.id) {
      return false;
    }
    if (!checkTransactionInputs(tx, maxUsedBlock.height, maxUsedBlock.id, &tail)) {
      lastFailed = tail;
      return false;
    }
  } else {
    if (maxUsedBlock.height >= getCurrentBlockchainHeight()) {
      return false;
    }
    if (getBlockIdByHeight(maxUsedBlock.height) != maxUsedBlock.id) {
      if (lastFailed.id == getBlockIdByHeight(lastFailed.height)) {
        return false;
      }
    }
    if (!checkTransactionInputs(tx, maxUsedBlock.height, maxUsedBlock.id, &tail)) {
      lastFailed = tail;
      return false;
    }
  }
  return true;
}

bool Blockchain::haveSpentKeyImages(const CryptoNote::Transaction& tx) {
  return haveTransactionKeyImagesAsSpent(tx);
}

bool Blockchain::checkTransactionSize(size_t blobSize) {
  if (blobSize > getCurrentCumulativeBlocksizeLimit() - m_currency.minerTxBlobReservedSize()) {
    logger(ERROR) << "transaction is too big " << blobSize
                  << ", maximum allowed size is "
                  << (getCurrentCumulativeBlocksizeLimit() - m_currency.minerTxBlobReservedSize());
    return false;
  }
  return true;
}

// ─── Query helpers ───────────────────────────────────────────────────────────

bool Blockchain::haveTransaction(const Crypto::Hash& id) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t block; uint16_t txSlot;
  return m_db.getTxIndex(id, block, txSlot);
}

bool Blockchain::have_tx_keyimg_as_spent(const Crypto::KeyImage& key_im) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return m_db.hasSpentKey(key_im);
}

bool Blockchain::checkIfSpent(const Crypto::KeyImage& keyImage, uint32_t blockIndex) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t spentHeight = 0;
  if (!m_db.getSpentKeyHeight(keyImage, spentHeight)) return false;
  return spentHeight <= blockIndex;
}

bool Blockchain::checkIfSpent(const Crypto::KeyImage& keyImage) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return m_db.hasSpentKey(keyImage);
}

uint32_t Blockchain::getCurrentBlockchainHeight() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return m_db.getChainHeight();
}

// ─── init / deinit ───────────────────────────────────────────────────────────

bool Blockchain::init(const std::string& config_folder, bool load_existing) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  if (!config_folder.empty() && !Tools::create_directories_if_necessary(config_folder)) {
    logger(ERROR, BRIGHT_RED) << "Failed to create data directory: " << config_folder;
    return false;
  }
  m_config_folder = config_folder;

  // Open (or create) the LMDB environment directory
  std::string lmdbPath = appendPath(config_folder, "blockchain.lmdb");
  if (!Tools::create_directories_if_necessary(lmdbPath)) {
    logger(ERROR, BRIGHT_RED) << "Failed to create LMDB directory: " << lmdbPath;
    return false;
  }

  if (!m_db.open(lmdbPath)) {
    namespace fs = std::filesystem;

    // Check if another process already holds the exclusive lock
    if (LMDBBlockchainDB::isLocked(lmdbPath)) {
      logger(ERROR, BRIGHT_RED)
          << "The blockchain database at " << lmdbPath
          << " is already in use by another process. "
          << "Only one process can access the database at a time. "
          << "Please close the other instance and try again.";
      return false;
    }

    logger(WARNING, BRIGHT_YELLOW)
        << "Failed to open LMDB database at " << lmdbPath
        << ", attempting recovery...";

    // --- 1. Try clearing stale readers via a fresh raw env ---
    // Do NOT use m_db.getEnv() here — the open failed, the handle is invalid.
    {
      MDB_env* rawEnv = nullptr;
      if (mdb_env_create(&rawEnv) == MDB_SUCCESS) {
        // Open read-only just enough to call mdb_reader_check
        if (mdb_env_open(rawEnv, lmdbPath.c_str(), MDB_RDONLY, 0664) == MDB_SUCCESS) {
          int dead = 0;
          if (mdb_reader_check(rawEnv, &dead) == MDB_SUCCESS) {
            logger(INFO) << "Cleared " << dead << " stale LMDB reader(s)";
          } else {
            logger(WARNING) << "mdb_reader_check returned error";
          }
        } else {
          logger(WARNING) << "Could not open LMDB read-only for reader check";
        }
        mdb_env_close(rawEnv);
      } else {
        logger(WARNING) << "mdb_env_create failed during reader check";
      }
    }

    // --- 2. Retry open after reader cleanup ---
    if (m_db.open(lmdbPath)) {
      logger(INFO) << "LMDB opened successfully after stale reader cleanup";
    } else {
      // --- 3. Attempt salvage copy ---
      bool recovered = false;

      logger(WARNING) << "Attempting LMDB salvage copy...";

      fs::path origPath    = lmdbPath;
      fs::path salvagePath = fs::path(lmdbPath).parent_path() / "blockchain.lmdb.salvage";

      // Remove any leftover salvage directory from a prior crashed attempt,
      // then create a fresh empty directory — mdb_env_copy2 requires it to exist.
      bool salvageDirReady = false;
      try {
        if (fs::exists(salvagePath)) {
          fs::remove_all(salvagePath);
          logger(WARNING) << "Removed leftover salvage directory: " << salvagePath;
        }
        fs::create_directories(salvagePath);
        salvageDirReady = true;
      } catch (const std::exception& e) {
        logger(WARNING) << "Could not prepare salvage directory: " << e.what();
      }

      // Perform the salvage copy — env is created, used, and closed exactly once
      bool salvageCopyOk = false;
      {
        MDB_env* env = nullptr;
        do {
          if (!salvageDirReady) {
            logger(WARNING) << "Skipping salvage copy: destination directory unavailable";
            break;
          }
          if (mdb_env_create(&env) != MDB_SUCCESS) {
            logger(WARNING) << "mdb_env_create failed during salvage";
            break;
          }
          // Match your production schema's named-DB count
          if (mdb_env_set_maxdbs(env, 16) != MDB_SUCCESS) {
            logger(WARNING) << "mdb_env_set_maxdbs failed during salvage";
            mdb_env_close(env);
            env = nullptr;
            break;
          }
          if (mdb_env_open(env, lmdbPath.c_str(), MDB_RDONLY, 0664) != MDB_SUCCESS) {
            logger(WARNING) << "mdb_env_open (read-only for salvage) failed";
            mdb_env_close(env);
            env = nullptr;
            break;
          }
          int rc = mdb_env_copy2(env, salvagePath.string().c_str(), MDB_CP_COMPACT);
          if (rc != MDB_SUCCESS) {
            logger(WARNING) << "mdb_env_copy2 failed: " << mdb_strerror(rc);
          } else {
            logger(INFO) << "Salvage copy written to: " << salvagePath;
            salvageCopyOk = true;
          }
          // Always close exactly once here — no other close for this env
          mdb_env_close(env);
          env = nullptr;
        } while (false);

        // Safety net: if we broke out with env still open
        if (env) {
          mdb_env_close(env);
        }
      }

      // Rotate files and attempt to open the salvaged copy
      if (salvageCopyOk) {
        try {
          // Build a unique .corrupt name so we never overwrite a prior backup
          fs::path corruptPath = fs::path(lmdbPath + ".corrupt");
          if (fs::exists(corruptPath)) {
            // Append a counter suffix to avoid collision
            int suffix = 2;
            fs::path candidate;
            do {
              candidate = fs::path(lmdbPath + ".corrupt." + std::to_string(suffix++));
            } while (fs::exists(candidate) && suffix < 100);
            corruptPath = candidate;
          }

          fs::rename(origPath, corruptPath);
          logger(WARNING) << "Original (corrupt) DB moved to: " << corruptPath;

          fs::rename(salvagePath, origPath);
          logger(INFO) << "Salvage copy promoted to: " << origPath;

          if (m_db.open(lmdbPath)) {
            logger(INFO) << "LMDB recovered successfully from salvage copy";
            recovered = true;
          } else {
            logger(WARNING) << "Could not open salvaged DB — will proceed to rebuild";
          }
        } catch (const std::exception& e) {
          logger(WARNING) << "File rotation during salvage failed: " << e.what();
        }
      }

      // --- 4. Full rebuild — wipe and start fresh ---
      if (!recovered) {
        logger(ERROR, BRIGHT_RED)
            << "LMDB unrecoverable. Rebuilding database from scratch. "
               "A full blockchain resync will be required.";
        try {
          // Move whatever is left to a unique .corrupt path
          if (fs::exists(lmdbPath)) {
            fs::path corruptPath = fs::path(lmdbPath + ".corrupt");
            if (fs::exists(corruptPath)) {
              int suffix = 2;
              fs::path candidate;
              do {
                candidate = fs::path(lmdbPath + ".corrupt." + std::to_string(suffix++));
              } while (fs::exists(candidate) && suffix < 100);
              corruptPath = candidate;
            }
            fs::rename(lmdbPath, corruptPath);
            logger(WARNING) << "Unrecoverable DB moved to: " << corruptPath;
          }

          // Re-create an empty directory for a fresh environment
          fs::create_directories(lmdbPath);

          if (!m_db.open(lmdbPath)) {
            logger(ERROR, BRIGHT_RED)
                << "Failed to create fresh LMDB environment after rebuild";
            return false;
          }

          logger(WARNING) << "Fresh LMDB environment created. Blockchain resync required.";
        } catch (const std::exception& e) {
          logger(ERROR, BRIGHT_RED) << "Rebuild failed: " << e.what();
          return false;
        }
      }
    }
  }

  // Migration from legacy SwappedVector files.
  // Always checked when old block files exist so an interrupted migration
  // (LMDB non-empty but incomplete) is automatically resumed.
  {
    std::string blocksFile = appendPath(config_folder, m_currency.blocksFileName());
    FILE* f = fopen(blocksFile.c_str(), "rb");
    if (f) {
      fclose(f);
      logger(INFO, BRIGHT_WHITE) << "Old block data detected, checking migration status...";
      if (!migrateFromSwappedVector(config_folder)) {
        logger(WARNING, BRIGHT_YELLOW) << "Migration failed, continuing with current LMDB state.";
      }
    }
  }

  uint32_t chainHeight = m_db.getChainHeight();

  if (load_existing && chainHeight > 0) {
    // Verify genesis
    DbBlockMeta genMeta{};
    m_db.getBlockMeta(0, genMeta);
    Crypto::Hash firstBlockHash;
    memcpy(firstBlockHash.data, genMeta.hash, 32);
    if (firstBlockHash != m_currency.genesisBlockHash()) {
      logger(ERROR, BRIGHT_RED) << "Failed to init: genesis block mismatch. "
        "Probably you set --testnet flag with data dir with non-test blockchain or another network.";
      return false;
    }

    logger(INFO, BRIGHT_WHITE) << "Loading blockchain...";

    // Always clear first: if migration just ran it will have already populated
    // m_blobs as a side effect of calling the inner pushBlock(), and we must
    // not double-load them - duplicate entries corrupt the index used by
    // getBlockLongHash() for v5+ PoW, causing validation failures on new blocks
    // after ~minedMoneyUnlockWindow live blocks are received post-migration.
    if (!m_no_blobs) {
      m_blobs.clear();
      m_blobs.reserve(chainHeight);
      for (uint32_t h = 0; h < chainHeight; ++h) {
        if (h % 50000 == 0 && h > 0) {
          logger(DEBUGGING, BRIGHT_WHITE) << "Loading blobs: height " << h << " of " << chainHeight;
        }
        std::vector<uint8_t> blobData;
        if (!m_db.getHashingBlob(h, blobData)) {
          logger(ERROR, BRIGHT_RED) << "Cannot read hashing blob at height " << h;
          return false;
        }
        m_blobs.push_back(BinaryArray(blobData.begin(), blobData.end()));
      }
    }
  }

  chainHeight = m_db.getChainHeight();

  if (chainHeight == 0) {
    logger(INFO, BRIGHT_WHITE) << "Blockchain not loaded, generating genesis block.";
    block_verification_context bvc = boost::value_initialized<block_verification_context>();
    pushBlock(m_currency.genesisBlock(), get_block_hash(m_currency.genesisBlock()), bvc);
    if (bvc.m_verification_failed) {
      logger(ERROR, BRIGHT_RED) << "Failed to add genesis block to blockchain";
      return false;
    }
    chainHeight = m_db.getChainHeight();
  }

  uint32_t lastValidCheckpointHeight = 0;
  if (!checkCheckpoints(lastValidCheckpointHeight)) {
    logger(WARNING, BRIGHT_YELLOW) << "Invalid checkpoint found. Rollback blockchain to height=" << lastValidCheckpointHeight;
    rollbackBlockchainTo(lastValidCheckpointHeight);
    chainHeight = m_db.getChainHeight();
  }

  if (!m_upgradeDetectorV2.init() || !m_upgradeDetectorV3.init() ||
      !m_upgradeDetectorV4.init() || !m_upgradeDetectorV5.init() || !m_upgradeDetectorV6.init()) {
    logger(ERROR, BRIGHT_RED) << "Failed to initialize upgrade detector.";
  }

  bool reinitUpgradeDetectors = false;
  auto checkAndRollback = [&](UpgradeDetector& ud) {
    if (!checkUpgradeHeight(ud)) {
      uint32_t upgradeHeight = ud.upgradeHeight();
      assert(upgradeHeight != UpgradeDetectorBase::UNDEF_HEIGHT);
      DbBlockMeta badMeta{};
      m_db.getBlockMeta(upgradeHeight + 1, badMeta);
      logger(WARNING, BRIGHT_YELLOW)
        << "Invalid block version at " << upgradeHeight + 1
        << ": real=" << static_cast<int>(badMeta.majorVersion)
        << " expected=" << static_cast<int>(ud.targetVersion())
        << ". Rollback blockchain to height=" << upgradeHeight;
      rollbackBlockchainTo(upgradeHeight);
      reinitUpgradeDetectors = true;
      return true;
    }
    return false;
  };

  if (checkAndRollback(m_upgradeDetectorV2)) {}
  else if (checkAndRollback(m_upgradeDetectorV3)) {}
  else if (checkAndRollback(m_upgradeDetectorV4)) {}
  else if (checkAndRollback(m_upgradeDetectorV5)) {}
  else if (checkAndRollback(m_upgradeDetectorV6)) {}

  if (reinitUpgradeDetectors &&
      (!m_upgradeDetectorV2.init() || !m_upgradeDetectorV3.init() ||
       !m_upgradeDetectorV4.init() || !m_upgradeDetectorV5.init() || !m_upgradeDetectorV6.init())) {
    logger(ERROR, BRIGHT_RED) << "Failed to initialize upgrade detector";
    return false;
  }

  update_next_cumulative_size_limit();

  // One-time backfill of account registration index for blocks that predate it.
  // Scans backwards from chain tip down to the first known registration.
  // If the first registration (1264265, 1) is already indexed, skip entirely.
  {
    static const uint32_t ACCT_REG_FIRST_HEIGHT = 1264265;
    static const uint16_t ACCT_REG_FIRST_TX     = 1;
    chainHeight = m_db.getChainHeight();

    if (chainHeight > ACCT_REG_FIRST_HEIGHT) {
      uint8_t sk[32], vk[32];
      if (!m_db.getAccountRegistration(ACCT_REG_FIRST_HEIGHT, ACCT_REG_FIRST_TX, sk, vk)) {
        logger(INFO, BRIGHT_WHITE) << "Backfilling account registration index from block "
          << (chainHeight - 1) << " down to " << ACCT_REG_FIRST_HEIGHT << " ...";

        uint32_t indexed = 0;
        static const uint32_t BATCH = 1000;
        uint32_t batchCount = 0;

        for (uint32_t h = chainHeight - 1; h >= ACCT_REG_FIRST_HEIGHT; --h) {
          if (batchCount == 0) {
            m_db.growMapIfNeeded();
            m_db.beginWriteTxn();
          }

          DbBlockMeta meta{};
          m_db.getBlockMeta(h, meta);

          for (uint16_t t = 1; t < meta.txCount; ++t) {
            uint8_t s[32], v[32];
            if (m_db.getAccountRegistration(h, t, s, v)) {
              continue;
            }

            try {
              TransactionEntry te = transactionByIndex({ h, t });
                TransactionExtraAccountRegistration reg;
              if (getAccountRegistrationFromExtra(te.tx.extra, reg) &&
                  isWellFormedAccountRegistration(te.tx.extra)) {
                m_db.putAccountRegistration(h, t, reg.spendPublicKey.data, reg.viewPublicKey.data);
                ++indexed;
              }
            } catch (...) {
            }
          }

          ++batchCount;
          if (batchCount >= BATCH || h == ACCT_REG_FIRST_HEIGHT) {
            m_db.commitTxn();
            batchCount = 0;
          }
        }

        if (indexed > 0) {
          logger(INFO, BRIGHT_GREEN) << "Account registration backfill complete: " << indexed << " registrations indexed.";
        } else {
          logger(INFO) << "Account registration backfill complete: no new registrations found.";
        }
      }
    }
  }

  chainHeight = m_db.getChainHeight();
  DbBlockMeta tailMeta{};
  m_db.getBlockMeta(chainHeight - 1, tailMeta);
  uint64_t timestamp_diff = time(NULL) - tailMeta.timestamp;
  if (!tailMeta.timestamp) {
    timestamp_diff = time(NULL) - 1341378000;
  }

  logger(INFO, BRIGHT_GREEN)
    << "Blockchain initialized. last block: " << chainHeight - 1 << ", "
    << Common::timeIntervalToString(timestamp_diff)
    << " time ago, current difficulty: " << getDifficultyForNextBlock(getTailId());

  return true;
}

bool Blockchain::deinit() {
  assert(m_messageQueueList.empty());
  flushBatch();  // commit any pending IBD write txn before closing
  return true;
}

bool Blockchain::flushBatch() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (m_batchCount == 0) return true;
  try {
    m_db.commitTxn();
    m_batchCount = 0;
  } catch (const std::exception& e) {
    m_db.abortTxn();
    m_batchCount = 0;
    if (m_batchFastMode) {
      m_db.setFastSyncMode(false);  // disable stale MDB_NOSYNC flag
      m_batchFastMode = false;
    }
    logger(ERROR, BRIGHT_RED) << "flushBatch: " << e.what();
    return false;
  }
  if (m_batchFastMode) {
    m_db.setFastSyncMode(false);  // disable MDB_NOSYNC + force flush to disk
    m_batchFastMode = false;
  }
  return true;
}

bool Blockchain::isSyncing() const {
  uint32_t h = m_db.getChainHeight();
  if (h == 0) return false;
  DbBlockMeta meta{};
  if (!m_db.getBlockMeta(h - 1, meta)) return false;
  auto now = static_cast<uint64_t>(time(nullptr));
  return now > meta.timestamp && (now - meta.timestamp) > 3600u;  // >1 hour behind
}

void Blockchain::beginBatchIfNeeded() {
  if (m_batchCount == 0) {
    m_db.growMapIfNeeded();  // preemptively resize if >=80% full
    bool shouldBeFast = isSyncing();
    // Reconcile MDB_NOSYNC state with the current sync status.
    // This also handles the case where a previous fast-mode batch was aborted
    // (via an early return) without disabling MDB_NOSYNC: the stale flag is
    // detected here and cleaned up before the new batch begins.
    if (m_batchFastMode && !shouldBeFast) {
      m_db.setFastSyncMode(false);  // disable MDB_NOSYNC + force flush
    } else if (!m_batchFastMode && shouldBeFast) {
      m_db.setFastSyncMode(true);
    }
    m_batchFastMode = shouldBeFast;
    m_db.beginWriteTxn();
  }
}

void Blockchain::commitBatchOrBlock(bool forceSingle) {
  ++m_batchCount;
  bool syncing = isSyncing();
  if (forceSingle || !syncing || m_batchCount >= BATCH_SIZE) {
    m_db.commitTxn();
    m_batchCount = 0;
    if (m_batchFastMode) {
      if (!syncing) {
        // Caught up to chain tip: disable fast mode and force a full flush
        // so the next live-block commit is fully durable.
        m_db.setFastSyncMode(false);
        m_batchFastMode = false;
      } else {
        // Still syncing but batch is full: checkpoint flush.
        // MDB_NOSYNC stays active; next batch continues in fast mode.
        // This ensures a crash causes a clean rollback to this height
        // rather than leaving the database in a corrupted state.
        m_db.syncToDisk();
      }
    }
  }
  // else: leave write txn open; next block reuses it
}

bool Blockchain::resetAndSetGenesisBlock(const Block& b) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (m_db.hasActiveTxn()) {
    m_db.abortTxn();
    m_batchCount = 0;
    if (m_batchFastMode) {
      m_db.setFastSyncMode(false);
      m_batchFastMode = false;
    }
  }

  m_db.clear();
  m_alternative_chains.clear();
  m_orphanBlocksIndex.clear();
  m_blobs.clear();

  block_verification_context bvc = boost::value_initialized<block_verification_context>();
  addNewBlock(b, bvc);
  return bvc.m_added_to_main_chain && !bvc.m_verification_failed;
}

// ─── Tail / chain state ──────────────────────────────────────────────────────

Crypto::Hash Blockchain::getTailId(uint32_t& height) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  height = m_db.getChainHeight() - 1;
  return getTailId();
}

Crypto::Hash Blockchain::getTailId() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t h = m_db.getChainHeight();
  if (h == 0) return NULL_HASH;
  DbBlockMeta meta{};
  m_db.getBlockMeta(h - 1, meta);
  Crypto::Hash hash;
  memcpy(hash.data, meta.hash, 32);
  return hash;
}

Crypto::Hash Blockchain::getBlockIdByHeight(uint32_t height) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  DbBlockMeta meta{};
  if (!m_db.getBlockMeta(height, meta)) return NULL_HASH;
  Crypto::Hash hash;
  memcpy(hash.data, meta.hash, 32);
  return hash;
}

bool Blockchain::getBlockByHash(const Crypto::Hash& blockHash, Block& b) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t height = 0;
  if (m_db.getHashHeight(blockHash, height)) {
    std::vector<uint8_t> bdata;
    if (!m_db.getBlockData(height, bdata)) return false;
    return fromBinaryArray(b, bdata);
  }
  auto it = m_alternative_chains.find(blockHash);
  if (it != m_alternative_chains.end()) {
    b = it->second.bl;
    return true;
  }
  return false;
}

bool Blockchain::getBlockHeight(const Crypto::Hash& blockId, uint32_t& blockHeight) {
  std::lock_guard<decltype(m_blockchain_lock)> lock(m_blockchain_lock);
  return m_db.getHashHeight(blockId, blockHeight);
}

bool Blockchain::getTransactionHeight(const Crypto::Hash& txId, uint32_t& blockHeight) {
  std::lock_guard<decltype(m_blockchain_lock)> bcLock(m_blockchain_lock);
  uint32_t block; uint16_t txSlot;
  if (!m_db.getTxIndex(txId, block, txSlot)) return false;
  blockHeight = block;
  return true;
}

uint64_t Blockchain::getBlockTimestamp(uint32_t height) {
  DbBlockMeta meta{};
  m_db.getBlockMeta(height, meta);
  return meta.timestamp;
}

uint64_t Blockchain::getCoinsInCirculation() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t h = m_db.getChainHeight();
  if (h == 0) return 0;
  DbBlockMeta meta{};
  m_db.getBlockMeta(h - 1, meta);
  return meta.alreadyGeneratedCoins;
}

uint64_t Blockchain::getCoinsInCirculation(uint32_t height) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  DbBlockMeta meta{};
  m_db.getBlockMeta(height, meta);
  return meta.alreadyGeneratedCoins;
}

uint8_t Blockchain::getBlockMajorVersionForHeight(uint32_t height) const {
  if (height > m_upgradeDetectorV6.upgradeHeight()) {
    return m_upgradeDetectorV6.targetVersion();
  } else if (height > m_upgradeDetectorV5.upgradeHeight()) {
    return m_upgradeDetectorV5.targetVersion();
  } else if (height > m_upgradeDetectorV4.upgradeHeight()) {
    return m_upgradeDetectorV4.targetVersion();
  } else if (height > m_upgradeDetectorV3.upgradeHeight()) {
    return m_upgradeDetectorV3.targetVersion();
  } else if (height > m_upgradeDetectorV2.upgradeHeight()) {
    return m_upgradeDetectorV2.targetVersion();
  } else {
    return BLOCK_MAJOR_VERSION_1;
  }
}

// ─── Difficulty ──────────────────────────────────────────────────────────────

difficulty_type Blockchain::getDifficultyForNextBlock(const Crypto::Hash& prevHash) {
  if (prevHash == NULL_HASH) return 1;

  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  uint32_t chainHeight = m_db.getChainHeight();
  uint8_t  BlockMajorVersion = getBlockMajorVersionForHeight(chainHeight);
  uint32_t difficultyBlocksCount = std::min<uint32_t>(
    std::max<uint32_t>(chainHeight > 0 ? chainHeight - 1 : 1, 1),
    static_cast<uint32_t>(m_currency.difficultyBlocksCountByBlockVersion(BlockMajorVersion)));

  std::vector<uint64_t>       timestamps;
  std::vector<difficulty_type> cumulative_difficulties;

  // ── Fast path: prevHash is the main-chain tip (normal sync) ──────────────
  // Instead of walking backwards by hash link (N separate LMDB txns), read
  // all N block_meta records in a single cursor scan.
  uint32_t prevHeight = 0;
  if (m_db.getHashHeight(prevHash, prevHeight)) {
    uint32_t fromHeight = (prevHeight + 1 >= difficultyBlocksCount)
                          ? prevHeight + 1 - difficultyBlocksCount : 0;

    std::vector<DbBlockMeta> metas;
    m_db.getBlockMetaRange(fromHeight, prevHeight, metas);

    timestamps.reserve(metas.size());
    cumulative_difficulties.reserve(metas.size());
    for (const auto& m : metas) {
      timestamps.push_back(m.timestamp);
      cumulative_difficulties.push_back(m.cumulativeDifficulty);
    }

    return m_currency.nextDifficulty(chainHeight, BlockMajorVersion,
                                      timestamps, cumulative_difficulties);
  }

  // ── Slow path: prevHash is on an alternative chain ───────────────────────
  // Walk backwards through the alt-chain then the main chain by hash links.
  uint32_t processed = 0;
  Crypto::Hash h = prevHash;
  while (processed < difficultyBlocksCount && h != NULL_HASH) {
    uint64_t       ts      = 0;
    difficulty_type cumDiff = 0;
    Crypto::Hash   prevH{};

    auto it = m_alternative_chains.find(h);
    if (it != m_alternative_chains.end()) {
      const BlockEntry& b = it->second;
      ts      = b.bl.timestamp;
      cumDiff = b.cumulative_difficulty;
      prevH   = b.bl.previousBlockHash;
    } else {
      uint32_t bh = 0;
      if (!m_db.getHashHeight(h, bh)) {
        logger(ERROR) << "Can't find block " << h << " for difficulty calculation";
        return 0;
      }
      DbBlockMeta meta{};
      m_db.getBlockMeta(bh, meta);
      ts      = meta.timestamp;
      cumDiff = meta.cumulativeDifficulty;
      memcpy(prevH.data, meta.prevHash, 32);
    }

    timestamps.push_back(ts);
    cumulative_difficulties.push_back(cumDiff);
    ++processed;
    h = prevH;
  }

  std::reverse(timestamps.begin(), timestamps.end());
  std::reverse(cumulative_difficulties.begin(), cumulative_difficulties.end());

  return m_currency.nextDifficulty(chainHeight, BlockMajorVersion,
                                    timestamps, cumulative_difficulties);
}

// ─── Block size tracking ──────────────────────────────────────────────────────

bool Blockchain::getBackwardBlocksSize(size_t from_height, std::vector<size_t>& sz, size_t count) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t chainHeight = m_db.getChainHeight();
  if (!(from_height < chainHeight)) {
    logger(ERROR, BRIGHT_RED) << "getBackwardBlocksSize called with from_height="
      << from_height << ", blockchain height = " << chainHeight;
    return false;
  }
  uint32_t start_offset = static_cast<uint32_t>(
    (from_height + 1) - std::min((from_height + 1), count));

  // Read the range in a single cursor scan instead of one txn per block.
  std::vector<DbBlockMeta> metas;
  m_db.getBlockMetaRange(start_offset, static_cast<uint32_t>(from_height), metas);
  for (const auto& m : metas) sz.push_back(m.blockCumulativeSize);
  return true;
}

bool Blockchain::get_last_n_blocks_sizes(std::vector<size_t>& sz, size_t count) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t chainHeight = m_db.getChainHeight();
  if (chainHeight == 0) return true;
  return getBackwardBlocksSize(chainHeight - 1, sz, count);
}

uint64_t Blockchain::getCurrentCumulativeBlocksizeLimit() {
  return m_current_block_cumul_sz_limit;
}

// ─── Hashing blobs ───────────────────────────────────────────────────────────

bool Blockchain::getHashingBlob(const uint32_t height, BinaryArray& blob) {
  if (!m_no_blobs) {
    std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
    if (height < m_blobs.size()) {
      blob = m_blobs[height];
      return true;
    }
  }

  std::vector<uint8_t> blobData;
  if (m_db.getHashingBlob(height, blobData)) {
    blob = BinaryArray(blobData.begin(), blobData.end());
    return true;
  }
  // Fallback: compute from stored block data
  std::vector<uint8_t> blockData;
  if (!m_db.getBlockData(height, blockData)) return false;
  Block blk;
  if (!fromBinaryArray(blk, blockData)) return false;
  return get_block_hashing_blob(blk, blob);
}

// ─── Proof of Work ───────────────────────────────────────────────────────────

bool Blockchain::checkProofOfWork(Crypto::cn_context& context, const Block& block,
                                   difficulty_type currentDiffic, Crypto::Hash& proofOfWork) {
  std::list<Crypto::Hash> dummy_alt_chain;
  return checkProofOfWork(context, block, currentDiffic, proofOfWork, dummy_alt_chain, m_no_blobs);
}

bool Blockchain::checkProofOfWork(Crypto::cn_context& context, const Block& block,
                                   difficulty_type currentDiffic, Crypto::Hash& proofOfWork,
                                   const std::list<Crypto::Hash>& alt_chain, bool no_blobs) {
  if (block.majorVersion < CryptoNote::BLOCK_MAJOR_VERSION_5)
    return m_currency.checkProofOfWork(context, block, currentDiffic, proofOfWork);
  if (!getBlockLongHash(context, block, proofOfWork, alt_chain, no_blobs))
    return false;
  if (!check_hash(proofOfWork, currentDiffic))
    return false;
  return true;
}

bool Blockchain::getBlockLongHash(Crypto::cn_context& context, const Block& b, Crypto::Hash& res) {
  std::list<Crypto::Hash> dummy_alt_chain;
  return getBlockLongHash(context, b, res, dummy_alt_chain, m_no_blobs);
}

// Big-endian load — interprets byte at offset as most significant.
// Used for extracting indices from hash output (v5 convention).
static inline uint32_t load_u32_be(const uint8_t* data, size_t offset) {
  return (uint32_t(data[offset])     << 24) |
         (uint32_t(data[offset + 1]) << 16) |
         (uint32_t(data[offset + 2]) << 8)  |
         (uint32_t(data[offset + 3]));
}

// Little-endian load — matches memcpy on LE platforms (x86, ARM),
// but produces a well-defined result on any architecture.
static inline uint32_t load_u32_le(const uint8_t* data, size_t offset) {
  return (uint32_t(data[offset + 3]) << 24) |
         (uint32_t(data[offset + 2]) << 16) |
         (uint32_t(data[offset + 1]) << 8)  |
         (uint32_t(data[offset]));
}

/*
 * Computes the "long hash" of a block for Proof-of-Work.
 *
 * For blocks prior to version 5, falls back to the legacy PoW function.
 *
 * For version 5 blocks:
 *   - Starts with the block's signed hashing blob.
 *   - Iteratively mixes the blob 128 times; in each iteration, accesses
 *     8 pseudo-random previous blocks' hashing blobs (from the main chain
 *     or alternative chains) to expand and "stir" the data.
 *   - Uses cached main-chain blobs when allowed to reduce reconstruction cost.
 *
 * For version 6 and later:
 *   - Introduces a deterministic sequence value derived from the intermediate
 *     hash to create a strict dependency between iterations.
 *   - Each memory access depends on the result of the previous step,
 *     enforcing sequential memory access reducing multi-core scaling,
 *     making the algorithm latency-bound rather than throughput-bound,
 *     thus more egalitarian.
 *
 * The final mixed data is processed with yespower (y_slow_hash) to produce
 * the Proof-of-Work hash.
 */
bool Blockchain::getBlockLongHash(Crypto::cn_context& context, const Block& b, Crypto::Hash& res,
                                   const std::list<Crypto::Hash>& alt_chain, bool no_blobs) {
  if (b.majorVersion < CryptoNote::BLOCK_MAJOR_VERSION_5)
    return get_block_longhash(context, b, res);

  BinaryArray pot;
  // reserve space to reduce reallocations
  pot.reserve(80 * 1024); // ~80 KB estimated from average blob size

  if (!get_signed_block_hashing_blob(b, pot))
    return false;

  Crypto::Hash hash_1, hash_2;
  // currentHeight - 1 - unlockWindow is always <= getCurrentBlockchainHeight() - 1
  // Avoid the redundant lock acquisition on every hash iteration by computing maxHeight 
  // directly from the block template instead of getCurrentBlockchainHeight().
  const uint32_t currentHeight = boost::get<BaseInput>(b.baseTransaction.inputs[0]).blockIndex;
  const uint32_t unlockWindow = static_cast<uint32_t>(m_currency.minedMoneyUnlockWindow());
  const uint32_t maxHeight = currentHeight - 1 - unlockWindow;

#define ITER 128

  // v6 sequential state
  uint32_t seq = 0;

  for (uint32_t i = 0; i < ITER; i++) {
    cn_fast_hash(pot.data(), pot.size(), hash_1);

    // initialize seq from the first iteration's hash (same data, avoids redundant hash)
    if (b.majorVersion >= CryptoNote::BLOCK_MAJOR_VERSION_6 && i == 0) {
      const uint8_t* d = hash_1.data;
      seq = load_u32_be(d, 0) ^ load_u32_be(d, 4) ^ load_u32_be(d, 8) ^ load_u32_be(d, 12);
    }

    for (uint8_t j = 1; j <= 8; j++) {

      const uint8_t* d = hash_1.data;
      uint32_t n = load_u32_be(d, (j - 1) * 4);

      uint32_t height_j;
      if (b.majorVersion < CryptoNote::BLOCK_MAJOR_VERSION_6) {
        height_j = n % maxHeight; // modulo bias is negligible and non-exploitable
      }
      else {
        // sequential dependency
        seq ^= n;
        seq ^= seq >> 16;
        seq *= 0x7feb352d;
        seq ^= seq >> 15;
        seq *= 0x846ca68b;
        seq ^= seq >> 16;

        // bias-free mapping
        height_j = (uint64_t(seq) * maxHeight) >> 32;
      }

      bool found_alt = false; // reset for each j

      // Alt-chain lookup first
      for (const auto& ch_ent : alt_chain) {
        const Block& ab = m_alternative_chains.at(ch_ent).bl;
        uint32_t ah = boost::get<BaseInput>(ab.baseTransaction.inputs[0]).blockIndex;
        if (ah == height_j) {
          BinaryArray ba;
          if (!get_block_hashing_blob(ab, ba)) return false;
          pot.insert(pot.end(), ba.begin(), ba.end());
          found_alt = true;
          // v6: mix memory content into seq
          if (b.majorVersion >= CryptoNote::BLOCK_MAJOR_VERSION_6 && ba.size() >= 4) {
            seq ^= load_u32_le(ba.data(), 0);
          }
          break;
        }
      }

      if (!found_alt) {
        if (no_blobs) {
          std::vector<uint8_t> blockData;
          if (!m_db.getBlockData(height_j, blockData)) return false;
          Block bj;
          if (!fromBinaryArray(bj, blockData)) return false;
          BinaryArray ba;
          if (!get_block_hashing_blob(bj, ba)) return false;
          pot.insert(pot.end(), ba.begin(), ba.end());
          // v6: mix memory content into seq
          if (b.majorVersion >= CryptoNote::BLOCK_MAJOR_VERSION_6 && ba.size() >= 4) {
            seq ^= load_u32_le(ba.data(), 0);
          }
        } else {
          BinaryArray ba;
          {
            std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
            if (height_j < m_blobs.size()) {
              ba = m_blobs[height_j];
            }
          }

          if (!ba.empty()) {
            pot.insert(pot.end(), ba.begin(), ba.end());
            // v6: mix memory content into seq
            if (b.majorVersion >= CryptoNote::BLOCK_MAJOR_VERSION_6 && ba.size() >= 4) {
              seq ^= load_u32_le(ba.data(), 0);
            }
          } else {
            std::vector<uint8_t> blobData;
            if (!m_db.getHashingBlob(height_j, blobData)) return false;
            pot.insert(pot.end(), blobData.begin(), blobData.end());
            // v6: mix memory content into seq
            if (b.majorVersion >= CryptoNote::BLOCK_MAJOR_VERSION_6 && blobData.size() >= 4) {
              seq ^= load_u32_le(blobData.data(), 0);
            }
          }
        }
      }
    }
  }
#undef ITER

  if (!Crypto::y_slow_hash(pot.data(), pot.size(), hash_1, hash_2))
    return false;

  res = hash_2;
  return true;
}

// ─── Timestamp checks ────────────────────────────────────────────────────────

bool Blockchain::complete_timestamps_vector(uint8_t blockMajorVersion,
                                             uint64_t start_top_height,
                                             std::vector<uint64_t>& timestamps) {
  if (timestamps.size() >= m_currency.timestampCheckWindow(blockMajorVersion)) return true;

  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t chainHeight = m_db.getChainHeight();
  size_t need_elements = m_currency.timestampCheckWindow(blockMajorVersion) - timestamps.size();
  if (!(start_top_height < chainHeight)) {
    logger(ERROR, BRIGHT_RED) << "internal error: passed start_height=" << start_top_height
                               << " not less than chainHeight=" << chainHeight;
    return false;
  }
  size_t stop_offset = start_top_height > need_elements ? start_top_height - need_elements : 0;
  do {
    DbBlockMeta meta{};
    m_db.getBlockMeta(static_cast<uint32_t>(start_top_height), meta);
    timestamps.push_back(meta.timestamp);
    if (start_top_height == 0) break;
    --start_top_height;
  } while (start_top_height != stop_offset);
  return true;
}

bool Blockchain::check_block_timestamp_main(const Block& b) {
  if (b.timestamp > get_adjusted_time() + m_currency.blockFutureTimeLimit(b.majorVersion)) {
    time_t t = static_cast<time_t>(b.timestamp);
    auto tm = *std::localtime(&t);
    logger(INFO, BRIGHT_WHITE)
      << "Timestamp of block with id: " << get_block_hash(b)
      << ", " << b.timestamp
      << " (" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << ") is too far in the future";
    return false;
  }

  uint32_t chainHeight = m_db.getChainHeight();
  uint32_t window = static_cast<uint32_t>(m_currency.timestampCheckWindow(b.majorVersion));
  uint32_t fromH  = (chainHeight > window) ? chainHeight - window : 0;

  // Read the timestamp window in a single cursor scan (one read txn).
  std::vector<DbBlockMeta> metas;
  m_db.getBlockMetaRange(fromH, chainHeight - 1, metas);

  std::vector<uint64_t> timestamps;
  timestamps.reserve(metas.size());
  for (const auto& m : metas) timestamps.push_back(m.timestamp);

  return check_block_timestamp(std::move(timestamps), b);
}

bool Blockchain::check_block_timestamp(std::vector<uint64_t> timestamps, const Block& b) {
  if (timestamps.size() < m_currency.timestampCheckWindow(b.majorVersion)) return true;
  uint64_t median_ts = Common::medianValue(timestamps);
  if (b.timestamp < median_ts) {
    logger(INFO, BRIGHT_WHITE)
      << "Timestamp of block with id " << get_block_hash(b) << ", " << b.timestamp
      << " is less than median of last " << m_currency.timestampCheckWindow(b.majorVersion)
      << " blocks, " << median_ts << ", i.e. it's too deep in the past";
    return false;
  }
  return true;
}

uint64_t Blockchain::get_adjusted_time() {
  return time(NULL);
}

// ─── Block validation helpers ────────────────────────────────────────────────

bool Blockchain::checkBlockVersion(const Block& b) {
  uint32_t height = get_block_height(b);
  const uint8_t expectedBlockVersion = getBlockMajorVersionForHeight(height);
  if (b.majorVersion != expectedBlockVersion) {
    logger(TRACE) << "Block " << get_block_hash(b)
      << " has wrong major version: " << static_cast<int>(b.majorVersion)
      << ", at height " << height << " expected version is "
      << static_cast<int>(expectedBlockVersion);
    return false;
  }
  return true;
}

bool Blockchain::checkParentBlockSize(const Block& b, const Crypto::Hash& blockHash) {
  if (b.majorVersion == BLOCK_MAJOR_VERSION_2 || b.majorVersion == BLOCK_MAJOR_VERSION_3) {
    auto serializer = makeParentBlockSerializer(b, false, false);
    size_t parentBlockSize;
    if (!getObjectBinarySize(serializer, parentBlockSize)) {
      logger(ERROR, BRIGHT_RED) << "Block " << blockHash << ": failed to determine parent block size";
      return false;
    }
    if (parentBlockSize > 2 * 1024) {
      logger(INFO, BRIGHT_WHITE) << "Block " << blockHash
        << " contains too big parent block: " << parentBlockSize
        << " bytes, expected no more than " << 2 * 1024 << " bytes";
      return false;
    }
  }
  return true;
}

bool Blockchain::checkCumulativeBlockSize(const Crypto::Hash& blockId, size_t cumulativeBlockSize,
                                           uint64_t height) {
  size_t maxBlockCumulativeSize = m_currency.maxBlockCumulativeSize(height);
  if (cumulativeBlockSize > maxBlockCumulativeSize) {
    logger(INFO, BRIGHT_WHITE) << "Block " << blockId
      << " is too big: " << cumulativeBlockSize << " bytes, "
      << "expected no more than " << maxBlockCumulativeSize << " bytes";
    return false;
  }
  return true;
}

bool Blockchain::getBlockCumulativeSize(const Block& block, size_t& cumulativeSize) {
  std::vector<Transaction> blockTxs;
  std::vector<Crypto::Hash> missedTxs;
  getTransactions(block.transactionHashes, blockTxs, missedTxs, true);
  cumulativeSize = getObjectBinarySize(block.baseTransaction);
  for (const Transaction& tx : blockTxs) {
    cumulativeSize += getObjectBinarySize(tx);
  }
  return missedTxs.empty();
}

bool Blockchain::update_next_cumulative_size_limit() {
  uint8_t nextBlockMajorVersion = getBlockMajorVersionForHeight(m_db.getChainHeight());
  size_t nextBlockGrantedFullRewardZone =
    m_currency.blockGrantedFullRewardZoneByBlockVersion(nextBlockMajorVersion);

  std::vector<size_t> sz;
  get_last_n_blocks_sizes(sz, m_currency.rewardBlocksWindow());

  uint64_t median = Common::medianValue(sz);
  if (median <= nextBlockGrantedFullRewardZone) {
    median = nextBlockGrantedFullRewardZone;
  }
  m_current_block_cumul_sz_limit = median * 2;
  return true;
}

// ─── Miner transaction validation ────────────────────────────────────────────

bool Blockchain::prevalidate_miner_transaction(const Block& b, uint32_t height) {
  if (!(b.baseTransaction.inputs.size() == 1)) {
    logger(ERROR, BRIGHT_RED) << "Coinbase transaction in the block has no inputs";
    return false;
  }
  if (b.majorVersion >= CryptoNote::BLOCK_MAJOR_VERSION_5) {
    if (!(b.baseTransaction.outputs.size() == 1)) {
      logger(ERROR, BRIGHT_RED) << "Only 1 output in coinbase transaction allowed";
      return false;
    }
    if (!(b.baseTransaction.outputs[0].target.type() == typeid(KeyOutput))) {
      logger(ERROR, BRIGHT_RED) << "Coinbase transaction in the block has the wrong output type";
      return false;
    }
  }
  if (!(b.baseTransaction.signatures.empty())) {
    logger(ERROR, BRIGHT_RED) << "Coinbase transaction in the block shouldn't have signatures";
    return false;
  }
  if (!(b.baseTransaction.inputs[0].type() == typeid(BaseInput))) {
    logger(ERROR, BRIGHT_RED) << "Coinbase transaction in the block has the wrong input type";
    return false;
  }
  if (boost::get<BaseInput>(b.baseTransaction.inputs[0]).blockIndex != height) {
    logger(INFO, BRIGHT_RED) << "The miner transaction in block has invalid height: "
      << boost::get<BaseInput>(b.baseTransaction.inputs[0]).blockIndex
      << ", expected: " << height;
    return false;
  }
  if (!(b.baseTransaction.unlockTime == height + m_currency.minedMoneyUnlockWindow())) {
    logger(ERROR, BRIGHT_RED) << "Coinbase transaction has wrong unlock time="
      << b.baseTransaction.unlockTime
      << ", expected " << (height + m_currency.minedMoneyUnlockWindow());
    return false;
  }
  if (!check_outs_overflow(b.baseTransaction)) {
    logger(ERROR, BRIGHT_RED) << "The miner transaction has money overflow in block "
      << get_block_hash(b);
    return false;
  }
  uint64_t extraSize = (uint64_t)b.baseTransaction.extra.size();
  if (height > CryptoNote::parameters::UPGRADE_HEIGHT_V4_2 &&
      extraSize > CryptoNote::parameters::MAX_EXTRA_SIZE) {
    logger(ERROR, BRIGHT_RED) << "The miner transaction extra is too large in block "
      << get_block_hash(b) << ". Allowed: " << CryptoNote::parameters::MAX_EXTRA_SIZE
      << ", actual: " << extraSize;
    return false;
  }
  return true;
}

bool Blockchain::validate_miner_transaction(const Block& b, uint32_t height,
                                             size_t cumulativeBlockSize,
                                             uint64_t alreadyGeneratedCoins,
                                             uint64_t fee, uint64_t& reward,
                                             int64_t& emissionChange) {
  uint64_t minerReward = 0;
  for (auto& o : b.baseTransaction.outputs) {
    minerReward += o.amount;
  }
  std::vector<size_t> lastBlocksSizes;
  get_last_n_blocks_sizes(lastBlocksSizes, m_currency.rewardBlocksWindow());
  size_t blocksSizeMedian = Common::medianValue(lastBlocksSizes);

  auto blockMajorVersion = getBlockMajorVersionForHeight(height);
  if (!m_currency.getBlockReward(blockMajorVersion, blocksSizeMedian, cumulativeBlockSize,
                                  alreadyGeneratedCoins, fee, reward, emissionChange)) {
    logger(INFO, BRIGHT_WHITE) << "block size " << cumulativeBlockSize
      << " is bigger than allowed for this blockchain";
    return false;
  }
  if (minerReward > reward) {
    logger(ERROR, BRIGHT_RED) << "Coinbase transaction spend too much money: "
      << m_currency.formatAmount(minerReward)
      << ", block reward is " << m_currency.formatAmount(reward);
    return false;
  } else if (minerReward < reward) {
    logger(ERROR, BRIGHT_RED) << "Coinbase transaction doesn't use full amount of block reward: spent "
      << m_currency.formatAmount(minerReward)
      << ", block reward is " << m_currency.formatAmount(reward);
    return false;
  }
  return true;
}

bool Blockchain::validate_block_signature(const Block& b, const Crypto::Hash& id, uint32_t height) {
  if (b.majorVersion >= CryptoNote::BLOCK_MAJOR_VERSION_5) {
    BinaryArray ba;
    if (!get_block_hashing_blob(b, ba)) {
      logger(ERROR, BRIGHT_RED) << "Failed to get_block_hashing_blob of block " << id
                                 << " at height " << height;
      return false;
    }
    Crypto::Hash sigHash = Crypto::cn_fast_hash(ba.data(), ba.size());
    Crypto::PublicKey ephPubKey = boost::get<KeyOutput>(b.baseTransaction.outputs[0].target).key;
    if (!Crypto::check_signature(sigHash, ephPubKey, b.signature)) {
      logger(Logging::ERROR, Logging::BRIGHT_RED) << "Signature mismatch in block "
        << id << " at height " << height;
      return false;
    }
  }
  return true;
}

// ─── Rollback / chain switching ──────────────────────────────────────────────

bool Blockchain::rollback_blockchain_switching(std::list<Block>& original_chain,
                                                size_t rollback_height) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  flushBatch();  // ensure no pending batch before popping blocks
  while (m_db.getChainHeight() > rollback_height) {
    popBlock();
  }
  for (auto& bl : original_chain) {
    block_verification_context bvc = boost::value_initialized<block_verification_context>();
    bool r = pushBlock(bl, get_block_hash(bl), bvc);
    if (!(r && bvc.m_added_to_main_chain)) {
      logger(ERROR, BRIGHT_RED) << "PANIC!!! failed to add (again) block while chain switching during rollback!";
      return false;
    }
  }
  logger(INFO, BRIGHT_WHITE) << "Rollback success.";
  return true;
}

bool Blockchain::switch_to_alternative_blockchain(const std::list<Crypto::Hash>& alt_chain,
                                                   bool discard_disconnected_chain) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  // Flush any pending IBD batch before disconnecting blocks.
  // removeLastBlock() needs its own write txn for each block removal;
  // a stale batch txn would cause a nested-write-txn deadlock.
  flushBatch();

  if (alt_chain.empty()) {
    logger(ERROR, BRIGHT_RED) << "switch_to_alternative_blockchain: empty chain passed";
    return false;
  }

  size_t split_height = static_cast<size_t>(m_alternative_chains[alt_chain.front()].height);
  uint32_t chainHeight = m_db.getChainHeight();

  if (!(chainHeight > split_height)) {
    logger(ERROR, BRIGHT_RED) << "switch_to_alternative_blockchain: blockchain size is lower than split height";
    return false;
  }

  // Check block versions
  for (const auto& hash : alt_chain) {
    const Block& b = m_alternative_chains[hash].bl;
    if (!checkBlockVersion(b)) {
      logger(ERROR, BRIGHT_RED) << "switch_to_alternative_blockchain: wrong major version of block " << hash;
      return false;
    }
  }

  // Disconnect old chain
  std::list<Block> disconnected_chain;
  chainHeight = m_db.getChainHeight();
  for (int i = (int)chainHeight - 1; i >= (int)split_height; i--) {
    std::vector<uint8_t> bdata;
    m_db.getBlockData((uint32_t)i, bdata);
    Block b;
    fromBinaryArray(b, bdata);
    popBlock();
    disconnected_chain.push_front(b);
  }

  // Connect new alternative chain
  for (auto alt_ch_iter = alt_chain.begin(); alt_ch_iter != alt_chain.end(); alt_ch_iter++) {
    const auto& ch_ent_h = *alt_ch_iter;
    block_verification_context bvc = boost::value_initialized<block_verification_context>();
    const Block& b = m_alternative_chains[ch_ent_h].bl;
    bool r = pushBlock(b, get_block_hash(b), bvc);
    if (!r || !bvc.m_added_to_main_chain) {
      logger(INFO, BRIGHT_WHITE) << "Failed to switch to alternative blockchain";
      rollback_blockchain_switching(disconnected_chain, split_height);
      logger(INFO, BRIGHT_WHITE) << "The block was inserted as invalid while connecting new alternative chain, block_id: " << ch_ent_h;
      {
        auto range = m_orphanBlocksIndex.equal_range(get_block_height(b));
        for (auto it = range.first; it != range.second; ) {
          if (it->second == ch_ent_h) it = m_orphanBlocksIndex.erase(it);
          else ++it;
        }
      }
      m_alternative_chains.erase(ch_ent_h);
      try {
        for (auto it2 = ++alt_ch_iter; it2 != alt_chain.end(); it2++) {
          const auto& ch_ent_hh = *it2;
          const Block& bb = m_alternative_chains[ch_ent_hh].bl;
          uint32_t bbh = get_block_height(bb);
          auto range2 = m_orphanBlocksIndex.equal_range(bbh);
          for (auto it3 = range2.first; it3 != range2.second; ) {
            if (it3->second == ch_ent_hh) it3 = m_orphanBlocksIndex.erase(it3);
            else ++it3;
          }
          m_alternative_chains.erase(ch_ent_hh);
        }
      } catch (std::exception& e) {
        logger(ERROR) << "removing alt_chain entries while connecting new alternative chain failed: " << e.what();
      }
      return false;
    }
  }

  if (!discard_disconnected_chain) {
    for (const auto& old_ch_ent : disconnected_chain) {
      block_verification_context bvc = boost::value_initialized<block_verification_context>();
      bool r = handle_alternative_block(old_ch_ent, get_block_hash(old_ch_ent), bvc, false);
      if (!r) {
        logger(WARNING, BRIGHT_YELLOW) << "Failed to push ex-main chain blocks to alternative chain";
        break;
      }
    }
  }

  std::vector<Crypto::Hash> blocksFromCommonRoot;
  blocksFromCommonRoot.reserve(alt_chain.size() + 1);
  const Block& b_front = m_alternative_chains[alt_chain.front()].bl;
  blocksFromCommonRoot.push_back(b_front.previousBlockHash);

  try {
    for (const auto& ch_ent : alt_chain) {
      const Block& bl = m_alternative_chains[ch_ent].bl;
      blocksFromCommonRoot.push_back(get_block_hash(bl));
      uint32_t blh = get_block_height(bl);
      auto range = m_orphanBlocksIndex.equal_range(blh);
      for (auto it = range.first; it != range.second; ) {
        if (it->second == ch_ent) it = m_orphanBlocksIndex.erase(it);
        else ++it;
      }
      m_alternative_chains.erase(ch_ent);
    }
  } catch (std::exception& e) {
    logger(ERROR) << "removing alt_chain entries from alternative chain failed: " << e.what();
  }

  sendMessage(BlockchainMessage(ChainSwitchMessage(std::move(blocksFromCommonRoot))));

  logger(INFO, BRIGHT_GREEN) << "REORGANIZE SUCCESS! on height: " << split_height
    << ", new blockchain size: " << m_db.getChainHeight();
  return true;
}

// ─── handle_alternative_block ────────────────────────────────────────────────

bool Blockchain::handle_alternative_block(const Block& b, const Crypto::Hash& id,
                                           block_verification_context& bvc,
                                           bool sendNewAlternativeBlockMessage) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  auto block_height = get_block_height(b);
  if (block_height == 0) {
    logger(ERROR, BRIGHT_RED) << "Block with id: " << Common::podToHex(id)
      << " (as alternative) have wrong miner transaction";
    bvc.m_verification_failed = true;
    return false;
  }

  if (!m_checkpoints.is_alternative_block_allowed(getCurrentBlockchainHeight(), block_height)) {
    logger(TRACE) << "Block with id: " << id << "\n can't be accepted for alternative chain, "
      << "block height: " << block_height << "\n blockchain height: " << getCurrentBlockchainHeight();
    bvc.m_verification_failed = true;
    return false;
  }

  if (!checkBlockVersion(b)) {
    bvc.m_verification_failed = true;
    return false;
  }

  if (!checkParentBlockSize(b, id)) {
    bvc.m_verification_failed = true;
    return false;
  }

  size_t cumulativeSize;
  if (!getBlockCumulativeSize(b, cumulativeSize)) {
    logger(TRACE) << "Block with id: " << id << " has at least one unknown transaction. "
      << "Cumulative size is calculated imprecisely";
  }

  if (!checkCumulativeBlockSize(id, cumulativeSize, block_height)) {
    bvc.m_verification_failed = true;
    return false;
  }

  uint32_t mainPrevHeight = 0;
  const bool mainPrev = m_db.getHashHeight(b.previousBlockHash, mainPrevHeight);
  const auto it_prev = m_alternative_chains.find(b.previousBlockHash);

  if (it_prev != m_alternative_chains.end() || mainPrev) {
    blocks_ext_by_hash::iterator alt_it = it_prev;
    std::list<Crypto::Hash> alt_chain;
    std::vector<uint64_t> timestamps;
    while (alt_it != m_alternative_chains.end()) {
      alt_chain.push_front(alt_it->first);
      timestamps.push_back(alt_it->second.bl.timestamp);
      alt_it = m_alternative_chains.find(alt_it->second.bl.previousBlockHash);
    }

    if (alt_chain.size()) {
      const BlockEntry& bei = m_alternative_chains[alt_chain.front()];
      if (!(m_db.getChainHeight() > bei.height)) {
        logger(ERROR, BRIGHT_RED) << "main blockchain wrong height";
        return false;
      }
      Crypto::Hash h = NULL_HASH;
      std::vector<uint8_t> bdata;
      m_db.getBlockData(bei.height - 1, bdata);
      Block prevBlk;
      fromBinaryArray(prevBlk, bdata);
      get_block_hash(prevBlk, h);
      if (!(h == bei.bl.previousBlockHash)) {
        logger(ERROR, BRIGHT_RED) << "alternative chain have wrong connection to main chain";
        return false;
      }
      complete_timestamps_vector(b.majorVersion, bei.height - 1, timestamps);
    } else {
      if (!mainPrev) {
        logger(ERROR, BRIGHT_RED) << "internal error: broken imperative condition "
          "it_main_prev != m_blocks_index.end()";
        return false;
      }
      complete_timestamps_vector(b.majorVersion, mainPrevHeight, timestamps);
    }

    if (!check_block_timestamp(timestamps, b)) {
      logger(INFO, BRIGHT_RED) << "Block with id: " << id << "\n"
        << " for alternative chain, have invalid timestamp: " << b.timestamp;
      bvc.m_verification_failed = true;
      return false;
    }

    BlockEntry bei = boost::value_initialized<BlockEntry>();
    bei.bl = b;
    bei.height = alt_chain.size() ? it_prev->second.height + 1 : mainPrevHeight + 1;

    bool is_a_checkpoint;
    if (!m_checkpoints.check_block(bei.height, id, is_a_checkpoint)) {
      logger(ERROR, BRIGHT_RED) << "CHECKPOINT VALIDATION FAILED";
      bvc.m_verification_failed = true;
      return false;
    }

    // Disable merged mining
    if (bei.bl.majorVersion >= CryptoNote::BLOCK_MAJOR_VERSION_5) {
      TransactionExtraMergeMiningTag mmTag;
      if (getMergeMiningTagFromExtra(bei.bl.baseTransaction.extra, mmTag)) {
        logger(ERROR, BRIGHT_RED) << "Merge mining tag was found in extra of miner transaction";
        return false;
      }
    }

    difficulty_type current_diff = getDifficultyForNextBlock(bei.bl.previousBlockHash);
    if (!current_diff) {
      logger(ERROR, BRIGHT_RED) << "!!!!!!! DIFFICULTY OVERHEAD !!!!!!!";
      return false;
    }

    Crypto::Hash proof_of_work = NULL_HASH;
    if (!checkProofOfWork(m_cn_context, bei.bl, current_diff, proof_of_work, alt_chain, true)) {
      logger(INFO, BRIGHT_RED) << "Block with id: " << Common::podToHex(id)
        << "\n for alternative chain, has not enough proof of work: " << proof_of_work
        << "\n expected difficulty: " << current_diff;
      bvc.m_verification_failed = true;
      return false;
    }

    if (!prevalidate_miner_transaction(b, bei.height)) {
      logger(INFO, BRIGHT_RED) << "Block with id: " << Common::podToHex(id)
        << " (as alternative) has wrong miner transaction.";
      bvc.m_verification_failed = true;
      return false;
    }

    if (!validate_block_signature(b, id, bei.height)) {
      logger(INFO, BRIGHT_RED) << "Block with id: " << Common::podToHex(id)
        << " (as alternative) has wrong miner signature.";
      bvc.m_verification_failed = true;
      return false;
    }

    if (alt_chain.size()) {
      bei.cumulative_difficulty = it_prev->second.cumulative_difficulty;
    } else {
      DbBlockMeta prevMeta{};
      m_db.getBlockMeta(mainPrevHeight, prevMeta);
      bei.cumulative_difficulty = prevMeta.cumulativeDifficulty;
    }
    bei.cumulative_difficulty += current_diff;

    auto i_res = m_alternative_chains.insert(blocks_ext_by_hash::value_type(id, bei));
    if (!i_res.second) {
      logger(ERROR, BRIGHT_RED) << "insertion of new alternative block returned as it already exist";
      return false;
    }

    m_orphanBlocksIndex.insert({bei.height, id});

    alt_chain.push_back(i_res.first->first);

    if (is_a_checkpoint) {
      logger(INFO, BRIGHT_GREEN)
        << "###### REORGANIZE on height: " << m_alternative_chains[alt_chain.front()].height
        << " of " << m_db.getChainHeight() - 1
        << ", checkpoint is found in alternative chain on height " << bei.height;
      bool r = switch_to_alternative_blockchain(alt_chain, true);
      if (r) {
        bvc.m_added_to_main_chain = true;
        bvc.m_switched_to_alt_chain = true;
      } else {
        bvc.m_verification_failed = true;
      }
      return r;
    } else {
      // Get last block meta for cumulative difficulty comparison
      uint32_t ch = m_db.getChainHeight();
      DbBlockMeta tailMeta{};
      m_db.getBlockMeta(ch - 1, tailMeta);

      if (tailMeta.cumulativeDifficulty < bei.cumulative_difficulty) {
        logger(INFO, BRIGHT_GREEN)
          << "###### REORGANIZE on height: " << m_alternative_chains[alt_chain.front()].height
          << " of " << ch - 1 << " with cumulative difficulty " << tailMeta.cumulativeDifficulty
          << "\n alternative blockchain size: " << alt_chain.size()
          << " with cumulative difficulty " << bei.cumulative_difficulty;
        bool r = switch_to_alternative_blockchain(alt_chain, false);
        if (r) {
          bvc.m_added_to_main_chain = true;
          bvc.m_switched_to_alt_chain = true;
        } else {
          bvc.m_verification_failed = true;
        }
        return r;
      } else {
        logger(INFO, BRIGHT_BLUE)
          << "----- BLOCK ADDED AS ALTERNATIVE ON HEIGHT " << bei.height
          << "\nid:         " << id
          << "\nPoW:        " << proof_of_work
          << "\ndifficulty: " << current_diff;
        if (sendNewAlternativeBlockMessage) {
          sendMessage(BlockchainMessage(NewAlternativeBlockMessage(id)));
        }
        return true;
      }
    }
  } else {
    bvc.m_marked_as_orphaned = true;
    logger(INFO, BRIGHT_RED) << "Block recognized as orphaned and rejected, id = " << id;
  }
  return true;
}

// ─── getBlocks ───────────────────────────────────────────────────────────────

bool Blockchain::getBlocks(uint32_t start_offset, uint32_t count,
                            std::list<Block>& blocks, std::list<Transaction>& txs) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t chainHeight = m_db.getChainHeight();
  if (start_offset >= chainHeight) return false;
  for (uint32_t i = start_offset; i < start_offset + count && i < chainHeight; i++) {
    std::vector<uint8_t> bdata;
    m_db.getBlockData(i, bdata);
    Block blk;
    fromBinaryArray(blk, bdata);
    blocks.push_back(blk);
    std::list<Crypto::Hash> missed_ids;
    getTransactions(blk.transactionHashes, txs, missed_ids);
    if (!missed_ids.empty()) {
      logger(ERROR, BRIGHT_RED) << "have missed transactions in own block in main blockchain";
      return false;
    }
  }
  return true;
}

bool Blockchain::getBlocks(uint32_t start_offset, uint32_t count, std::list<Block>& blocks) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t chainHeight = m_db.getChainHeight();
  if (start_offset >= chainHeight) return false;
  for (uint32_t i = start_offset; i < start_offset + count && i < chainHeight; i++) {
    std::vector<uint8_t> bdata;
    m_db.getBlockData(i, bdata);
    Block blk;
    fromBinaryArray(blk, bdata);
    blocks.push_back(blk);
  }
  return true;
}

bool Blockchain::getTransactionsWithOutputGlobalIndexes(
    const std::vector<Crypto::Hash>& txs_ids,
    std::list<Crypto::Hash>& missed_txs,
    std::vector<std::pair<Transaction, std::vector<uint32_t>>>& txs) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  for (const auto& tx_id : txs_ids) {
    uint32_t block; uint16_t txSlot;
    if (!m_db.getTxIndex(tx_id, block, txSlot)) {
      missed_txs.push_back(tx_id);
    } else {
      TransactionEntry te = transactionByIndex({block, txSlot});
      if (te.m_global_output_indexes.empty()) {
        logger(ERROR, BRIGHT_RED) << "internal error: global indexes for transaction "
          << tx_id << " is empty";
        return false;
      }
      txs.push_back({te.tx, te.m_global_output_indexes});
    }
  }
  return true;
}

bool Blockchain::handleGetObjects(NOTIFY_REQUEST_GET_OBJECTS::request& arg,
                                   NOTIFY_RESPONSE_GET_OBJECTS::request& rsp) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  rsp.current_blockchain_height = getCurrentBlockchainHeight();
  std::list<Block> blocks;
  getBlocks(arg.blocks, blocks, rsp.missed_ids);
  for (const auto& bl : blocks) {
    std::list<Crypto::Hash> missed_tx_id;
    std::list<Transaction> txs;
    getTransactions(bl.transactionHashes, txs, missed_tx_id);
    if (!missed_tx_id.empty()) {
      logger(ERROR, BRIGHT_RED) << "Internal error: have missed missed_tx_id.size()="
        << missed_tx_id.size() << "\nfor block id = " << get_block_hash(bl);
      logger(INFO) << "Probably some blockchain indexes or cache is corrupted. "
        "Perform rebuilding cache or just resync from scratch.";
      rsp.missed_ids.insert(rsp.missed_ids.end(), missed_tx_id.begin(), missed_tx_id.end());
      return false;
    }
    rsp.blocks.push_back(block_complete_entry());
    block_complete_entry& e = rsp.blocks.back();
    e.block = asString(toBinaryArray(bl));
    for (Transaction& tx : txs) {
      e.txs.push_back(asString(toBinaryArray(tx)));
    }
  }
  std::list<Transaction> txs;
  getTransactions(arg.txs, txs, rsp.missed_ids);
  for (const auto& tx : txs) {
    rsp.txs.push_back(asString(toBinaryArray(tx)));
  }
  return true;
}

bool Blockchain::getAlternativeBlocks(std::list<Block>& blocks) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  for (auto& alt_bl : m_alternative_chains) {
    blocks.push_back(alt_bl.second.bl);
  }
  return true;
}

uint32_t Blockchain::getAlternativeBlocksCount() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return static_cast<uint32_t>(m_alternative_chains.size());
}

// ─── Random outputs ──────────────────────────────────────────────────────────

bool Blockchain::add_out_to_get_random_outs(uint64_t amount, size_t globalIdx,
    COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount& result_outs) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  uint32_t block; uint16_t txSlot, outIdx;
  if (!m_db.getKeyOutput(amount, static_cast<uint32_t>(globalIdx), block, txSlot, outIdx)) {
    logger(ERROR, BRIGHT_RED) << "internal error: getKeyOutput failed for amount="
      << amount << " globalIdx=" << globalIdx;
    return false;
  }

  TransactionEntry te = transactionByIndex({block, txSlot});
  if (outIdx >= te.tx.outputs.size()) {
    logger(ERROR, BRIGHT_RED) << "internal error: in global outs index, transaction out index="
      << outIdx << " more than transaction outputs = " << te.tx.outputs.size();
    return false;
  }
  if (!(te.tx.outputs[outIdx].target.type() == typeid(KeyOutput))) {
    logger(ERROR, BRIGHT_RED) << "unknown tx out type";
    return false;
  }
  if (!is_tx_spendtime_unlocked(te.tx.unlockTime)) {
    return false;
  }

  COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry& oen =
    *result_outs.outs.insert(result_outs.outs.end(),
                              COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::out_entry());
  oen.global_amount_index = static_cast<uint32_t>(globalIdx);
  oen.out_key = boost::get<KeyOutput>(te.tx.outputs[outIdx].target).key;
  return true;
}

size_t Blockchain::find_end_of_allowed_index(uint64_t amount) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t count = m_db.getKeyOutputCount(amount);
  if (count == 0) return 0;

  uint32_t chainHeight = m_db.getChainHeight();
  size_t i = count;
  do {
    --i;
    uint32_t block; uint16_t txSlot, outIdx;
    if (!m_db.getKeyOutput(amount, static_cast<uint32_t>(i), block, txSlot, outIdx)) {
      continue;
    }
    if (block + m_currency.minedMoneyUnlockWindow() <= chainHeight) {
      return i + 1;
    }
  } while (i != 0);
  return 0;
}

bool Blockchain::getRandomOutsByAmount(
    const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request& req,
    COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response& res) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  for (uint64_t amount : req.amounts) {
    COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount& result_outs =
      *res.outs.insert(res.outs.end(),
                       COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount());
    result_outs.amount = amount;

    uint32_t outputCount = m_db.getKeyOutputCount(amount);
    if (outputCount == 0) {
      logger(ERROR, BRIGHT_RED)
        << "COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS: not outs for amount " << amount
        << ", wallet should use some real outs when it lookup for some mix";
      continue;
    }

    size_t up_index_limit = find_end_of_allowed_index(amount);
    if (!(up_index_limit <= outputCount)) {
      logger(ERROR, BRIGHT_RED) << "internal error: find_end_of_allowed_index returned wrong index="
        << up_index_limit << ", with outputCount=" << outputCount;
      return false;
    }

    if (outputCount > req.outs_count) {
      std::set<size_t> used;
      size_t try_count = 0;
      for (uint64_t j = 0; j != req.outs_count && try_count < up_index_limit;) {
        uint64_t r = Random::randomValue<size_t>() % ((uint64_t)1 << 53);
        double frac = std::sqrt((double)r / ((uint64_t)1 << 53));
        size_t idx = (size_t)(frac * up_index_limit);
        if (used.count(idx)) continue;
        bool added = add_out_to_get_random_outs(amount, idx, result_outs);
        used.insert(idx);
        if (added) ++j;
        ++try_count;
      }
    } else {
      for (size_t i = 0; i < up_index_limit; ++i) {
        add_out_to_get_random_outs(amount, i, result_outs);
      }
    }
  }
  return true;
}

// ─── findBlockchainSupplement ────────────────────────────────────────────────

uint32_t Blockchain::findBlockchainSupplement(const std::vector<Crypto::Hash>& qblock_ids) {
  assert(!qblock_ids.empty());
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  for (const auto& id : qblock_ids) {
    uint32_t height = 0;
    if (m_db.getHashHeight(id, height)) {
      return height;
    }
  }
  return 0;
}

std::vector<Crypto::Hash> Blockchain::findBlockchainSupplement(
    const std::vector<Crypto::Hash>& remoteBlockIds, size_t maxCount,
    uint32_t& totalBlockCount, uint32_t& startBlockIndex) {
  assert(!remoteBlockIds.empty());
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  totalBlockCount = getCurrentBlockchainHeight();
  startBlockIndex = findBlockchainSupplement(remoteBlockIds);
  return getBlockIds(startBlockIndex, static_cast<uint32_t>(maxCount));
}

std::vector<Crypto::Hash> Blockchain::buildSparseChain() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t h = m_db.getChainHeight();
  assert(h != 0);
  DbBlockMeta meta{};
  m_db.getBlockMeta(h - 1, meta);
  Crypto::Hash tailHash;
  memcpy(tailHash.data, meta.hash, 32);
  return doBuildSparseChain(tailHash);
}

std::vector<Crypto::Hash> Blockchain::buildSparseChain(const Crypto::Hash& startBlockId) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  assert(haveBlock(startBlockId));
  return doBuildSparseChain(startBlockId);
}

std::vector<Crypto::Hash> Blockchain::doBuildSparseChain(const Crypto::Hash& startBlockId) const {
  uint32_t startBlockHeight = 0;
  bool isInMain = m_db.getHashHeight(startBlockId, startBlockHeight);

  if (isInMain) {
    std::vector<Crypto::Hash> result;
    size_t sparseChainEnd = static_cast<size_t>(startBlockHeight + 1);
    for (size_t i = 1; i <= sparseChainEnd; i *= 2) {
      DbBlockMeta meta{};
      m_db.getBlockMeta(static_cast<uint32_t>(sparseChainEnd - i), meta);
      Crypto::Hash h;
      memcpy(h.data, meta.hash, 32);
      result.emplace_back(h);
    }
    DbBlockMeta genMeta{};
    m_db.getBlockMeta(0, genMeta);
    Crypto::Hash genesisHash;
    memcpy(genesisHash.data, genMeta.hash, 32);
    if (result.back() != genesisHash) {
      result.emplace_back(genesisHash);
    }
    return result;
  } else {
    assert(m_alternative_chains.count(startBlockId) > 0);
    std::vector<Crypto::Hash> alternativeChain;
    Crypto::Hash blockchainAncestor;
    for (auto it = m_alternative_chains.find(startBlockId);
         it != m_alternative_chains.end();
         it = m_alternative_chains.find(blockchainAncestor)) {
      alternativeChain.emplace_back(it->first);
      blockchainAncestor = it->second.bl.previousBlockHash;
    }
    std::vector<Crypto::Hash> sparseChain;
    for (size_t i = 1; i <= alternativeChain.size(); i *= 2) {
      sparseChain.emplace_back(alternativeChain[i - 1]);
    }
    assert(!sparseChain.empty());
    std::vector<Crypto::Hash> sparseMainChain = doBuildSparseChain(blockchainAncestor);
    sparseChain.reserve(sparseChain.size() + sparseMainChain.size());
    std::copy(sparseMainChain.begin(), sparseMainChain.end(), std::back_inserter(sparseChain));
    return sparseChain;
  }
}

bool Blockchain::haveBlock(const Crypto::Hash& id) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t h = 0;
  if (m_db.getHashHeight(id, h)) return true;
  return m_alternative_chains.count(id) > 0;
}

size_t Blockchain::getTotalTransactions() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t h = m_db.getChainHeight();
  if (h == 0) return 0;
  uint64_t count = 0;
  m_db.getGeneratedTxCount(h - 1, count);
  return static_cast<size_t>(count);
}

std::vector<Crypto::Hash> Blockchain::getBlockIds(uint32_t startHeight, uint32_t maxCount) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  std::vector<Crypto::Hash> result;
  uint32_t chainHeight = m_db.getChainHeight();
  for (uint32_t h = startHeight; h < chainHeight && result.size() < maxCount; ++h) {
    DbBlockMeta meta{};
    m_db.getBlockMeta(h, meta);
    Crypto::Hash hash;
    memcpy(hash.data, meta.hash, 32);
    result.push_back(hash);
  }
  return result;
}

bool Blockchain::getTransactionOutputGlobalIndexes(const Crypto::Hash& tx_id,
                                                    std::vector<uint32_t>& indexs) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t block; uint16_t txSlot;
  if (!m_db.getTxIndex(tx_id, block, txSlot)) {
    logger(WARNING, YELLOW) << "warning: get_tx_outputs_gindexs failed to find transaction with id = " << tx_id;
    return false;
  }
  TransactionEntry te = transactionByIndex({block, txSlot});
  if (te.m_global_output_indexes.empty()) {
    logger(ERROR, BRIGHT_RED) << "internal error: global indexes for transaction " << tx_id << " is empty";
    return false;
  }
  indexs.resize(te.m_global_output_indexes.size());
  for (size_t i = 0; i < te.m_global_output_indexes.size(); ++i) {
    indexs[i] = te.m_global_output_indexes[i];
  }
  return true;
}

// ─── Transaction input validation ────────────────────────────────────────────

bool Blockchain::checkTransactionInputs(const Transaction& tx, uint32_t& max_used_block_height,
                                         Crypto::Hash& max_used_block_id, BlockInfo* tail) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (tail) tail->id = getTailId(tail->height);

  bool res = checkTransactionInputs(tx, &max_used_block_height);
  if (!res) return false;

  uint32_t chainHeight = m_db.getChainHeight();
  if (!(max_used_block_height < chainHeight)) {
    logger(ERROR, BRIGHT_RED) << "internal error: max used block index=" << max_used_block_height
      << " is not less than blockchain size = " << chainHeight;
    return false;
  }
  DbBlockMeta meta{};
  m_db.getBlockMeta(max_used_block_height, meta);
  memcpy(max_used_block_id.data, meta.hash, 32);
  return true;
}

bool Blockchain::haveTransactionKeyImagesAsSpent(const Transaction& tx) {
  for (const auto& in : tx.inputs) {
    if (in.type() == typeid(KeyInput)) {
      if (have_tx_keyimg_as_spent(boost::get<KeyInput>(in).keyImage)) {
        return true;
      }
    }
  }
  return false;
}

bool Blockchain::checkTransactionInputs(const Transaction& tx, uint32_t* pmax_used_block_height) {
  Crypto::Hash tx_prefix_hash = getObjectHash(*static_cast<const TransactionPrefix*>(&tx));
  return checkTransactionInputs(tx, tx_prefix_hash, pmax_used_block_height);
}

bool Blockchain::checkTransactionInputs(const Transaction& tx, const Crypto::Hash& tx_prefix_hash,
                                         uint32_t* pmax_used_block_height) {
  size_t inputIndex = 0;
  if (pmax_used_block_height) *pmax_used_block_height = 0;

  Crypto::Hash transactionHash = getObjectHash(tx);
  for (const auto& txin : tx.inputs) {
    assert(inputIndex < tx.signatures.size());
    if (txin.type() == typeid(KeyInput)) {
      const KeyInput& in_to_key = boost::get<KeyInput>(txin);
      if (!(!in_to_key.outputIndexes.empty())) {
        logger(ERROR, BRIGHT_RED) << "empty in_to_key.outputIndexes in transaction with id " << getObjectHash(tx);
        return false;
      }
      if (have_tx_keyimg_as_spent(in_to_key.keyImage)) {
        logger(DEBUGGING) << "Key image already spent in blockchain: " << Common::podToHex(in_to_key.keyImage);
        return false;
      }
      if (!isInCheckpointZone(getCurrentBlockchainHeight())) {
        if (!check_tx_input(in_to_key, tx_prefix_hash, tx.signatures[inputIndex], pmax_used_block_height)) {
          logger(INFO, BRIGHT_WHITE) << "Failed to check input in transaction " << transactionHash;
          return false;
        }
      }
      ++inputIndex;
    } else {
      logger(INFO, BRIGHT_WHITE) << "Transaction << " << transactionHash
        << " contains input of unsupported type.";
      return false;
    }
  }
  return true;
}

bool Blockchain::is_tx_spendtime_unlocked(uint64_t unlock_time) {
  if (unlock_time < m_currency.maxBlockHeight()) {
    if (getCurrentBlockchainHeight() - 1 + m_currency.lockedTxAllowedDeltaBlocks() >= unlock_time)
      return true;
    else
      return false;
  } else {
    const uint64_t lastBlockTimestamp = getBlockTimestamp(getCurrentBlockchainHeight() - 1);
    if (lastBlockTimestamp + m_currency.lockedTxAllowedDeltaSeconds() >= unlock_time)
      return true;
    else
      return false;
  }
  return false;
}

bool Blockchain::is_tx_spendtime_unlocked(uint64_t unlock_time, uint32_t height) {
  if (unlock_time < m_currency.maxBlockHeight()) {
    if (height - 1 + m_currency.lockedTxAllowedDeltaBlocks() >= unlock_time)
      return true;
  }
  return false;
}

bool Blockchain::check_tx_input(const KeyInput& txin, const Crypto::Hash& tx_prefix_hash,
                                  const std::vector<Crypto::Signature>& sig,
                                  uint32_t* pmax_related_block_height) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  struct outputs_visitor {
    // Key values are OWNED here so that pointers into m_results_collector remain
    // valid after each loop iteration in scanOutputKeysForIndexes destroys the
    // temporary TransactionEntry.  reserve() prevents reallocation (which would
    // invalidate already-stored back-pointers).
    std::vector<Crypto::PublicKey> m_key_storage;
    std::vector<const Crypto::PublicKey*>& m_results_collector;
    Blockchain& m_bch;
    LoggerRef logger;
    outputs_visitor(std::vector<const Crypto::PublicKey*>& results_collector,
                    Blockchain& bch, ILogger& logger, size_t ringSize)
      : m_results_collector(results_collector), m_bch(bch), logger(logger, "outputs_visitor") {
        m_key_storage.reserve(ringSize);
    }

    bool handle_output(const Transaction& tx, const TransactionOutput& out,
                       size_t transactionOutputIndex) {
      if (!m_bch.is_tx_spendtime_unlocked(tx.unlockTime)) {
        logger(INFO, BRIGHT_WHITE) << "One of outputs for one of inputs have wrong tx.unlockTime = "
          << tx.unlockTime;
        return false;
      }
      if (out.target.type() != typeid(KeyOutput)) {
        logger(INFO, BRIGHT_WHITE) << "Output have wrong type id, which=" << out.target.which();
        return false;
      }
      m_key_storage.push_back(boost::get<KeyOutput>(out.target).key);  // copy key value
      m_results_collector.push_back(&m_key_storage.back());             // stable pointer
      return true;
    }
  };

  if (!(scalarmultKey(txin.keyImage, Crypto::EllipticCurveScalar2KeyImage(Crypto::L))
        == Crypto::EllipticCurveScalar2KeyImage(Crypto::I))) {
    logger(ERROR) << "Transaction uses key image not in the valid domain";
    return false;
  }

  std::vector<const Crypto::PublicKey*> output_keys;
  outputs_visitor vi(output_keys, *this, logger.getLogger(), txin.outputIndexes.size());
  if (!scanOutputKeysForIndexes(txin, vi, pmax_related_block_height)) {
    logger(INFO, BRIGHT_WHITE) << "Failed to get output keys for tx with amount = "
      << m_currency.formatAmount(txin.amount)
      << " and count indexes " << txin.outputIndexes.size();
    return false;
  }

  if (txin.outputIndexes.size() != output_keys.size()) {
    logger(INFO, BRIGHT_WHITE) << "Output keys for tx with amount = " << txin.amount
      << " and count indexes " << txin.outputIndexes.size()
      << " returned wrong keys count " << output_keys.size();
    return false;
  }

  if (!(sig.size() == output_keys.size())) {
    logger(ERROR, BRIGHT_RED) << "internal error: tx signatures count=" << sig.size()
      << " mismatch with outputs keys count for inputs=" << output_keys.size();
    return false;
  }

  if (isInCheckpointZone(getCurrentBlockchainHeight())) return true;

  bool check_tx_ring_signature = Crypto::check_ring_signature(
    tx_prefix_hash, txin.keyImage, output_keys, sig.data());
  if (!check_tx_ring_signature) {
    logger(ERROR) << "Failed to check ring signature for keyImage: " << txin.keyImage;
  }
  return check_tx_ring_signature;
}

// ─── addNewBlock / pushBlock / popBlock ──────────────────────────────────────

bool Blockchain::addNewBlock(const Block& bl, block_verification_context& bvc) {
  Crypto::Hash id;
  if (!get_block_hash(bl, id)) {
    logger(ERROR, BRIGHT_RED) << "Failed to get block hash, possible block has invalid format";
    bvc.m_verification_failed = true;
    return false;
  }

  bool add_result;
  {
    std::lock_guard<decltype(m_tx_pool)> poolLock(m_tx_pool);
    std::lock_guard<decltype(m_blockchain_lock)> bcLock(m_blockchain_lock);

    if (haveBlock(id)) {
      logger(TRACE) << "block with id = " << id << " already exists";
      bvc.m_already_exists = true;
      return false;
    }

    if (!(bl.previousBlockHash == getTailId())) {
      uint32_t blockHeight = 0;
      if (!bl.baseTransaction.inputs.empty()) {
        if (const BaseInput* baseInput = boost::get<BaseInput>(&bl.baseTransaction.inputs.front())) {
          blockHeight = baseInput->blockIndex;
        }
      }
      logger(DEBUGGING) << "handling alternative block " << Common::podToHex(id)
        << " at height " << blockHeight
        << " as it doesn't refer to chain tail " << Common::podToHex(getTailId())
        << ", its prev. block hash: " << Common::podToHex(bl.previousBlockHash);
      bvc.m_added_to_main_chain = false;
      add_result = handle_alternative_block(bl, id, bvc);
    } else {
      add_result = pushBlock(bl, id, bvc);
      if (add_result) {
        sendMessage(BlockchainMessage(NewBlockMessage(id)));
      }
    }
  }

  if (add_result && bvc.m_added_to_main_chain) {
    m_observerManager.notify(&IBlockchainStorageObserver::blockchainUpdated);
  }
  return add_result;
}

Blockchain::TransactionEntry Blockchain::transactionByIndex(TransactionIndex idx) {
  std::vector<uint8_t> raw;
  if (!m_db.getTxEntry(idx.block, idx.transaction, raw)) {
    throw std::runtime_error("transactionByIndex: entry not found at block=" +
                             std::to_string(idx.block) + " tx=" + std::to_string(idx.transaction));
  }
  if (raw.size() < sizeof(uint32_t))
    throw std::runtime_error("transactionByIndex: corrupt entry (too small for tx_size)");

  const uint8_t* p = raw.data();
  uint32_t txSize;
  memcpy(&txSize, p, sizeof(uint32_t)); p += sizeof(uint32_t);

  if (raw.size() < sizeof(uint32_t) + txSize + sizeof(uint32_t))
    throw std::runtime_error("transactionByIndex: corrupt entry (truncated)");

  TransactionEntry te;
  if (!fromBinaryArray(te.tx, BinaryArray(p, p + txSize)))
    throw std::runtime_error("transactionByIndex: tx deserialize failed");
  p += txSize;

  uint32_t numGidx;
  memcpy(&numGidx, p, sizeof(uint32_t)); p += sizeof(uint32_t);

  te.m_global_output_indexes.resize(numGidx);
  for (uint32_t i = 0; i < numGidx; ++i) {
    memcpy(&te.m_global_output_indexes[i], p, sizeof(uint32_t));
    p += sizeof(uint32_t);
  }
  return te;
}

bool Blockchain::pushBlock(const Block& blockData, const Crypto::Hash& id,
                            block_verification_context& bvc) {
  std::vector<Transaction> transactions;
  if (!loadTransactions(blockData, transactions)) {
    bvc.m_verification_failed = true;
    return false;
  }
  if (!pushBlock(blockData, transactions, id, bvc)) {
    saveTransactions(transactions);
    return false;
  }
  return true;
}

bool Blockchain::pushBlock(const Block& blockData, const std::vector<Transaction>& transactions,
                            const Crypto::Hash& blockHash, block_verification_context& bvc) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  auto blockProcessingStart = std::chrono::steady_clock::now();

  {
    uint32_t h = 0;
    if (m_db.getHashHeight(blockHash, h)) {
      logger(ERROR, BRIGHT_RED) << "Block " << blockHash << " already exists in blockchain.";
      bvc.m_verification_failed = true;
      return false;
    }
  }

  if (!checkBlockVersion(blockData)) {
    bvc.m_verification_failed = true;
    return false;
  }
  if (!checkParentBlockSize(blockData, blockHash)) {
    bvc.m_verification_failed = true;
    return false;
  }
  if (blockData.majorVersion >= CryptoNote::BLOCK_MAJOR_VERSION_5) {
    TransactionExtraMergeMiningTag mmTag;
    if (getMergeMiningTagFromExtra(blockData.baseTransaction.extra, mmTag)) {
      logger(ERROR, BRIGHT_RED) << "Merge mining tag was found in extra of miner transaction";
      return false;
    }
  }
  if (blockData.previousBlockHash != getTailId()) {
    logger(INFO, BRIGHT_WHITE) << "Block " << blockHash << " has wrong previousBlockHash: "
      << blockData.previousBlockHash << ", expected: " << getTailId();
    bvc.m_verification_failed = true;
    return false;
  }
  if (!check_block_timestamp_main(blockData)) {
    logger(INFO, BRIGHT_WHITE) << "Block " << blockHash
      << " has invalid timestamp: " << blockData.timestamp;
    bvc.m_verification_failed = true;
    return false;
  }

  auto targetTimeStart = std::chrono::steady_clock::now();
  difficulty_type currentDifficulty = getDifficultyForNextBlock(blockData.previousBlockHash);
  auto target_calculating_time = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now() - targetTimeStart).count();

  if (!currentDifficulty) {
    logger(ERROR, BRIGHT_RED) << "!!!!!!!!! difficulty overhead !!!!!!!!!";
    return false;
  }

  // getChainHeight() uses activeTxn() so it correctly accounts for any blocks
// already written into an open batch write txn from previous calls.
  uint32_t newHeight = m_db.getChainHeight();

  auto longhashTimeStart = std::chrono::steady_clock::now();
  Crypto::Hash proof_of_work = NULL_HASH;
  
  const bool inCheckpoint = m_checkpoints.is_in_checkpoint_zone(newHeight);
  if (inCheckpoint) {
    if (!m_checkpoints.check_block(newHeight, blockHash)) {
      logger(ERROR, BRIGHT_RED) << "CHECKPOINT VALIDATION FAILED";
      bvc.m_verification_failed = true;
      return false;
    }
  }
  else {
    if (!checkProofOfWork(m_cn_context, blockData, currentDifficulty, proof_of_work)) {
      logger(INFO, BRIGHT_WHITE) << "Block " << blockHash
        << ", has too weak proof of work: " << proof_of_work
        << ", expected difficulty: " << currentDifficulty;
      bvc.m_verification_failed = true;
      return false;
    }
  }

  auto longhash_calculating_time = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now() - longhashTimeStart).count();

  if (!prevalidate_miner_transaction(blockData, newHeight)) {
    logger(INFO, BRIGHT_WHITE) << "Block " << blockHash << " failed to pass prevalidation";
    bvc.m_verification_failed = true;
    return false;
  }
  if (!validate_block_signature(blockData, blockHash, newHeight)) {
    logger(INFO, BRIGHT_RED) << "Block with id: " << Common::podToHex(blockHash)
      << " has wrong miner signature.";
    bvc.m_verification_failed = true;
    return false;
  }

  // Pre-compute values that are stable across map-full retries.
  // These read from already-committed blocks so they work outside the write txn.
  const Crypto::Hash minerTransactionHash = getObjectHash(blockData.baseTransaction);
  const size_t coinbase_blob_size = getObjectBinarySize(blockData.baseTransaction);

  uint64_t already_generated_coins = 0;
  difficulty_type prevCumulativeDifficulty = 0;
  if (newHeight > 0) {
    DbBlockMeta prevMeta{};
    m_db.getBlockMeta(newHeight - 1, prevMeta);
    already_generated_coins   = prevMeta.alreadyGeneratedCoins;
    prevCumulativeDifficulty  = prevMeta.cumulativeDifficulty;
  }

  // Outputs that must survive past the write-txn block for the log below.
  BlockEntry block;
  block.bl     = blockData;
  block.height = newHeight;
  size_t   cumulative_block_size = coinbase_blob_size;
  uint64_t fee_summary           = 0;
  int64_t  emissionChange        = 0;
  uint64_t reward                = 0;

  // ── Write section ────────────────────────────────────────────────────────
  // beginBatchIfNeeded() opens a new write txn (+ enables MDB_NOSYNC) only
  // when no batch is already in flight; otherwise the existing txn is reused.
  // commitBatchOrBlock() decides whether to commit immediately (normal live
  // operation) or to defer until BATCH_SIZE blocks have accumulated (sync).
  //
  // On map-full the whole batch is aborted and the chain reverts to the last
  // committed height; the protocol handler re-syncs from there.
  block.transactions.resize(1);
  block.transactions[0].tx = blockData.baseTransaction;
  TransactionIndex transactionIndex = {newHeight, 0};
  bool blockTxnActive = false;

  auto trimBlobsToActiveChain = [&]() {
    if (!m_no_blobs) {
      const uint32_t chainHeight = m_db.getChainHeight();
      if (m_blobs.size() > chainHeight) {
        m_blobs.resize(chainHeight);
      }
    }
  };

  auto abortCurrentBlockTxn = [&]() {
    if (blockTxnActive && m_db.hasNestedTxn()) {
      m_db.abortNestedWriteTxn();
      blockTxnActive = false;
    } else if (m_db.hasActiveTxn() && m_batchCount == 0) {
      m_db.abortTxn();
    }
    trimBlobsToActiveChain();
  };

  auto abortEntireBatchTxn = [&]() {
    if (blockTxnActive && m_db.hasNestedTxn()) {
      m_db.abortNestedWriteTxn();
      blockTxnActive = false;
    }
    m_db.abortTxn();
    m_batchCount = 0;
    trimBlobsToActiveChain();
  };

  try {
    beginBatchIfNeeded();
    if (m_batchCount > 0) {
      m_db.beginNestedWriteTxn();
      blockTxnActive = true;
    }

    if (!pushTransaction(block, minerTransactionHash, transactionIndex)) {
      abortCurrentBlockTxn();
      bvc.m_verification_failed = true;
      return false;
    }

    for (size_t i = 0; i < transactions.size(); ++i) {
      const Crypto::Hash& tx_id = blockData.transactionHashes[i];
      block.transactions.resize(block.transactions.size() + 1);
      block.transactions.back().tx = transactions[i];

      size_t blob_size = toBinaryArray(block.transactions.back().tx).size();
      uint64_t fee = getInputAmount(block.transactions.back().tx) -
                     getOutputAmount(block.transactions.back().tx);

      // Under a confirmed checkpoint the block hash has already been verified by
      // the network. Skip the expensive per-input validation (key-image domain
      // check, output-key LMDB scans) - pushTransaction still records everything.
      if (!inCheckpoint && !checkTransactionInputs(block.transactions.back().tx)) {
        logger(INFO, BRIGHT_WHITE) << "Block " << blockHash
          << " has at least one transaction with wrong inputs: " << tx_id;
        bvc.m_verification_failed = true;
        abortCurrentBlockTxn();
        return false;
      }

      ++transactionIndex.transaction;
      if (!pushTransaction(block, tx_id, transactionIndex)) {
        abortCurrentBlockTxn();
        bvc.m_verification_failed = true;
        return false;
      }

      cumulative_block_size += blob_size;
      fee_summary += fee;
    }

    if (!checkCumulativeBlockSize(blockHash, cumulative_block_size, newHeight)) {
      bvc.m_verification_failed = true;
      abortCurrentBlockTxn();
      return false;
    }

    if (!validate_miner_transaction(blockData, newHeight, cumulative_block_size,
                                     already_generated_coins, fee_summary, reward, emissionChange)) {
      logger(INFO, BRIGHT_WHITE) << "Block " << blockHash << " has invalid miner transaction";
      bvc.m_verification_failed = true;
      abortCurrentBlockTxn();
      return false;
    }

    block.block_cumulative_size   = cumulative_block_size;
    block.already_generated_coins = already_generated_coins + emissionChange;
    block.cumulative_difficulty   = currentDifficulty + prevCumulativeDifficulty;

    pushBlock(block, blockHash);  // writes block-level LMDB data
    if (blockTxnActive) {
      m_db.commitNestedWriteTxn();
      blockTxnActive = false;
    }

    commitBatchOrBlock(newHeight == 0);  // commits now if live, defers if syncing

  } catch (const LMDBMapFullException&) {
    // Batch (possibly spanning many blocks) is aborted.  Chain reverts to the
    // last committed height; the protocol handler will re-sync from there.
    abortEntireBatchTxn();
    if (m_batchFastMode) {
      m_db.setFastSyncMode(false);
      m_batchFastMode = false;
    }
    logger(WARNING, BRIGHT_YELLOW) << "LMDB map full at height " << newHeight
      << "; batch aborted. Re-syncing from height " << m_db.getChainHeight()
      << ". Map will be doubled on next block.";
    m_db.resizeMap();
    return false;
  } catch (const std::exception& e) {
    // Any other LMDB error (e.g. MDB_CORRUPTED): abort and clean up batch state.
    abortEntireBatchTxn();
    if (m_batchFastMode) {
      m_db.setFastSyncMode(false);
      m_batchFastMode = false;
    }
    logger(ERROR, BRIGHT_RED) << "Exception adding block " << blockHash << ": " << e.what();
    return false;
  }

  auto block_processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now() - blockProcessingStart).count();

  if (block.height % 1000 == 0) {
    logger(INFO) << "Blockchain loaded to height: " << block.height;
  }

  logger(DEBUGGING)
    << "+++++ BLOCK SUCCESSFULLY ADDED\nid:\t" << blockHash
    << "\nPoW:\t" << proof_of_work
    << "\nHEIGHT " << block.height << ", difficulty:\t" << currentDifficulty
    << "\nblock reward: " << m_currency.formatAmount(reward)
    << ", fee = " << m_currency.formatAmount(fee_summary)
    << ", coinbase_blob_size: " << coinbase_blob_size
    << ", cumulative size: " << cumulative_block_size
    << ", " << block_processing_time
    << "(" << target_calculating_time << "/" << longhash_calculating_time << ")ms";

  bvc.m_added_to_main_chain = true;

  m_upgradeDetectorV2.blockPushed();
  m_upgradeDetectorV3.blockPushed();
  m_upgradeDetectorV4.blockPushed();
  m_upgradeDetectorV5.blockPushed();
  m_upgradeDetectorV6.blockPushed();

  update_next_cumulative_size_limit();
  return true;
}

// Inner pushBlock - writes block-level data within the active write txn.
bool Blockchain::pushBlock(BlockEntry& block, const Crypto::Hash& blockHash) {
  uint32_t height = block.height;

  DbBlockMeta meta{};
  memcpy(meta.hash, blockHash.data, 32);
  if (height > 0) {
    memcpy(meta.prevHash, block.bl.previousBlockHash.data, 32);
  }
  meta.timestamp            = block.bl.timestamp;
  meta.cumulativeDifficulty = block.cumulative_difficulty;
  meta.alreadyGeneratedCoins = block.already_generated_coins;
  meta.blockCumulativeSize  = static_cast<uint32_t>(block.block_cumulative_size);
  meta.height               = height;
  meta.txCount              = static_cast<uint16_t>(block.transactions.size());
  meta.majorVersion         = block.bl.majorVersion;
  meta.minorVersion         = block.bl.minorVersion;

  m_db.putBlockMeta(height, meta);

  BinaryArray blockBlob = toBinaryArray(block.bl);
  m_db.putBlockData(height, blockBlob.data(), blockBlob.size());

  m_db.putHashHeight(blockHash, height);

  BinaryArray hashBlob;
  if (!get_block_hashing_blob(block.bl, hashBlob)) {
    logger(ERROR, BRIGHT_RED) << "Failed to get_block_hashing_blob of block " << blockHash;
  }
  m_db.putHashingBlob(height, hashBlob.data(), hashBlob.size());
  if (!m_no_blobs) {
    m_blobs.push_back(hashBlob);
  }

  m_db.putTimestamp(block.bl.timestamp, blockHash);

  uint64_t prevGenTx = 0;
  if (height > 0) {
    m_db.getGeneratedTxCount(height - 1, prevGenTx);
  }
  uint64_t newGenTx = prevGenTx + block.bl.transactionHashes.size() + 1;
  m_db.putGeneratedTxCount(height, newGenTx);

  return true;
}

void Blockchain::popBlock() {
  uint32_t chainHeight = m_db.getChainHeight();
  if (chainHeight == 0) {
    logger(ERROR, BRIGHT_RED) << "Attempt to pop block from empty blockchain.";
    return;
  }
  uint32_t height = chainHeight - 1;

  DbBlockMeta meta{};
  m_db.getBlockMeta(height, meta);

  // Read non-coinbase transactions before removing
  std::vector<uint8_t> bdata;
  m_db.getBlockData(height, bdata);
  Block blk;
  fromBinaryArray(blk, bdata);

  std::vector<Transaction> transactions;
  for (uint16_t t = 1; t < meta.txCount; ++t) {
    TransactionEntry te = transactionByIndex({height, t});
    transactions.push_back(te.tx);
  }
  saveTransactions(transactions);

  removeLastBlock();

  m_upgradeDetectorV2.blockPopped();
  m_upgradeDetectorV3.blockPopped();
  m_upgradeDetectorV4.blockPopped();
  m_upgradeDetectorV5.blockPopped();
  m_upgradeDetectorV6.blockPopped();
}

bool Blockchain::pushTransaction(BlockEntry& block, const Crypto::Hash& transactionHash,
                                  TransactionIndex transactionIndex) {
  // Check for duplicate
  {
    uint32_t existBlock; uint16_t existSlot;
    if (m_db.getTxIndex(transactionHash, existBlock, existSlot)) {
      logger(ERROR, BRIGHT_RED) << "Duplicate transaction was pushed to blockchain.";
      return false;
    }
  }

  Transaction& tx = block.transactions[transactionIndex.transaction].tx;

  // Record spent key images; detect double-spends within this write txn
  for (size_t i = 0; i < tx.inputs.size(); ++i) {
    if (tx.inputs[i].type() == typeid(KeyInput)) {
      const auto& ki = boost::get<KeyInput>(tx.inputs[i]).keyImage;
      if (m_db.hasSpentKey(ki)) {
        logger(ERROR, BRIGHT_RED) << "Double spending transaction was pushed to blockchain.";
        // Roll back keys already written in this tx
        for (size_t j = 0; j < i; ++j) {
          if (tx.inputs[j].type() == typeid(KeyInput)) {
            m_db.removeSpentKey(boost::get<KeyInput>(tx.inputs[j]).keyImage);
          }
        }
        return false;
      }
      m_db.putSpentKey(ki, block.height);
    }
  }

  // Record key outputs and fill global output indexes
  auto& gidx = block.transactions[transactionIndex.transaction].m_global_output_indexes;
  gidx.resize(tx.outputs.size(), 0);
  for (uint16_t o = 0; o < static_cast<uint16_t>(tx.outputs.size()); ++o) {
    if (tx.outputs[o].target.type() == typeid(KeyOutput)) {
      uint32_t globalIdx = m_db.getKeyOutputCount(tx.outputs[o].amount);
      m_db.putKeyOutput(tx.outputs[o].amount, globalIdx,
                        block.height, transactionIndex.transaction, o);
      gidx[o] = globalIdx;
    }
  }

  // Record tx index
  m_db.putTxIndex(transactionHash, block.height, transactionIndex.transaction);

  // Record payment ID
  {
    Crypto::Hash paymentId;
    if (getPaymentIdFromTxExtra(tx.extra, paymentId)) {
      m_db.putPaymentId(paymentId, transactionHash);
    }
  }

  // Record account registration (non-coinbase only, well-formed only)
  if (transactionIndex.transaction != 0) {
    TransactionExtraAccountRegistration reg;
    if (getAccountRegistrationFromExtra(tx.extra, reg) &&
        isWellFormedAccountRegistration(tx.extra)) {
      m_db.putAccountRegistration(block.height, transactionIndex.transaction,
                                  reg.spendPublicKey.data, reg.viewPublicKey.data);
      invalidateAccountRegistrationsCountCache();
    }
  }

  // Serialize and store tx entry: [u32 tx_size][tx_blob][u32 num_gidx][u32 gidx...]
  BinaryArray txBlob = toBinaryArray(tx);
  uint32_t txSize  = static_cast<uint32_t>(txBlob.size());
  uint32_t numGidx = static_cast<uint32_t>(gidx.size());

  std::vector<uint8_t> entry;
  entry.resize(sizeof(uint32_t) + txSize + sizeof(uint32_t) + numGidx * sizeof(uint32_t));
  uint8_t* p = entry.data();
  memcpy(p, &txSize, sizeof(uint32_t));   p += sizeof(uint32_t);
  memcpy(p, txBlob.data(), txSize);       p += txSize;
  memcpy(p, &numGidx, sizeof(uint32_t));  p += sizeof(uint32_t);
  for (uint32_t gi : gidx) {
    memcpy(p, &gi, sizeof(uint32_t));
    p += sizeof(uint32_t);
  }
  m_db.putTxEntry(block.height, transactionIndex.transaction, entry.data(), entry.size());

  return true;
}

void Blockchain::popTransaction(const Transaction& transaction,
                                 const Crypto::Hash& transactionHash,
                                 uint32_t blockHeight) {
  // Remove key outputs in reverse output order
  for (int o = static_cast<int>(transaction.outputs.size()) - 1; o >= 0; --o) {
    const auto& out = transaction.outputs[o];
    if (out.target.type() == typeid(KeyOutput)) {
      if (!m_db.removeLastKeyOutput(out.amount)) {
        logger(ERROR, BRIGHT_RED) << "Blockchain consistency broken - removeLastKeyOutput failed for amount=" << out.amount;
      }
    }
  }

  // Remove spent keys
  for (const auto& input : transaction.inputs) {
    if (input.type() == typeid(KeyInput)) {
      const auto& ki = boost::get<KeyInput>(input).keyImage;
      if (!m_db.removeSpentKey(ki)) {
        logger(ERROR, BRIGHT_RED) << "Blockchain consistency broken - removeSpentKey failed";
      }
    }
  }

  // Remove payment ID
  {
    Crypto::Hash paymentId;
    if (getPaymentIdFromTxExtra(transaction.extra, paymentId)) {
      m_db.removePaymentId(paymentId, transactionHash);
    }
  }

  // Remove account registration
  {
    TransactionExtraAccountRegistration reg;
    if (getAccountRegistrationFromExtra(transaction.extra, reg)) {
      uint32_t block; uint16_t txSlot;
      if (m_db.getTxIndex(transactionHash, block, txSlot)) {
        m_db.removeAccountRegistration(block, txSlot);
        invalidateAccountRegistrationsCountCache();
      }
    }
  }

  // Remove tx index
  if (!m_db.removeTxIndex(transactionHash)) {
    logger(ERROR, BRIGHT_RED) << "Blockchain consistency broken - removeTxIndex failed";
  }
}

void Blockchain::popTransactions(const BlockEntry& block, const Crypto::Hash& minerTransactionHash,
                                  uint32_t blockHeight) {
  for (size_t i = 0; i < block.transactions.size() - 1; ++i) {
    size_t ri = block.transactions.size() - 1 - i;
    popTransaction(block.transactions[ri].tx,
                   block.bl.transactionHashes[ri - 1],
                   blockHeight);
  }
  popTransaction(block.bl.baseTransaction, minerTransactionHash, blockHeight);
}

void Blockchain::removeLastBlock() {
  uint32_t chainHeight = m_db.getChainHeight();
  if (chainHeight == 0) {
    logger(ERROR, BRIGHT_RED) << "Attempt to pop block from empty blockchain.";
    return;
  }
  uint32_t height = chainHeight - 1;

  DbBlockMeta meta{};
  if (!m_db.getBlockMeta(height, meta)) {
    logger(ERROR, BRIGHT_RED) << "removeLastBlock: cannot read block meta at height " << height;
    return;
  }

  logger(DEBUGGING) << "Removing last block with height " << height;

  // Phase 1: Read all tx data (reads work through the active write txn if present)
  std::vector<uint8_t> bdata;
  if (!m_db.getBlockData(height, bdata)) {
    logger(ERROR, BRIGHT_RED) << "removeLastBlock: cannot read block data";
    return;
  }
  Block blk;
  if (!fromBinaryArray(blk, bdata)) {
    logger(ERROR, BRIGHT_RED) << "removeLastBlock: cannot deserialize block";
    return;
  }

  uint16_t txCount = meta.txCount;
  std::vector<std::pair<Crypto::Hash, Transaction>> txToRemove;
  txToRemove.reserve(txCount);

  for (uint16_t t = 0; t < txCount; ++t) {
    TransactionEntry te = transactionByIndex({height, t});
    Crypto::Hash txHash;
    if (t == 0) {
      txHash = getObjectHash(blk.baseTransaction);
    } else {
      txHash = blk.transactionHashes[t - 1];
    }
    txToRemove.push_back({txHash, te.tx});
  }

  Crypto::Hash blockHash;
  memcpy(blockHash.data, meta.hash, 32);

  // Phase 2: Atomic removal.
  // If a write txn is already active (caller manages it), reuse it;
  // otherwise open and commit our own.
  const bool ownTxn = !m_db.hasActiveTxn();
  if (ownTxn) {
    m_db.beginWriteTxn();
  }

  // Pop transactions in reverse (last non-coinbase first, coinbase last)
  for (int i = static_cast<int>(txToRemove.size()) - 1; i >= 0; --i) {
    popTransaction(txToRemove[i].second, txToRemove[i].first, height);
  }

  m_db.removeTxEntriesForBlock(height, txCount);
  m_db.removeBlockData(height);
  m_db.removeHashHeight(blockHash);
  m_db.removeHashingBlob(height);
  m_db.removeGeneratedTxCount(height);

  m_db.removeTimestamp(meta.timestamp, blockHash);

  m_db.removeLastBlockMeta();

  if (ownTxn) {
    m_db.commitTxn();
  }

  if (!m_no_blobs && !m_blobs.empty()) {
    m_blobs.pop_back();
  }
}

bool Blockchain::checkCheckpoints(uint32_t& lastValidCheckpointHeight) {
  std::vector<uint32_t> checkpointHeights = m_checkpoints.getCheckpointHeights();
  for (const auto& checkpointHeight : checkpointHeights) {
    if (m_db.getChainHeight() <= checkpointHeight) {
      return true;
    }
    if (m_checkpoints.check_block(checkpointHeight, getBlockIdByHeight(checkpointHeight))) {
      lastValidCheckpointHeight = checkpointHeight;
    } else {
      return false;
    }
  }
  return true;
}

void Blockchain::rollbackBlockchainTo(uint32_t height) {
  flushBatch();  // ensure no pending batch before removing blocks
  while (height + 1 < m_db.getChainHeight()) {
    try {
      removeLastBlock();
    } catch (const LMDBMapFullException&) {
      // Each removeLastBlock opens its own write txn; LMDB's copy-on-write
      // accumulates free pages inside the txn that aren't reclaimable until
      // commit, so deep rewinds can spill the map. Abort the partial txn,
      // double the map, and retry the same block — the push path uses the
      // same dance for IBD map-full.
      m_db.abortTxn();
      logger(WARNING, BRIGHT_YELLOW) << "LMDB map full during rollback at height "
                                     << (m_db.getChainHeight() - 1)
                                     << "; doubling map and retrying.";
      m_db.resizeMap();
      removeLastBlock();
    }
  }
}

bool Blockchain::checkUpgradeHeight(const UpgradeDetector& upgradeDetector) {
  uint32_t upgradeHeight = upgradeDetector.upgradeHeight();
  if (upgradeHeight != UpgradeDetectorBase::UNDEF_HEIGHT &&
      upgradeHeight + 1 < m_db.getChainHeight()) {
    DbBlockMeta meta{};
    if (!m_db.getBlockMeta(upgradeHeight + 1, meta)) return false;
    if (meta.majorVersion != upgradeDetector.targetVersion()) {
      return false;
    }
  }
  return true;
}

bool Blockchain::getLowerBound(uint64_t timestamp, uint64_t startOffset, uint32_t& height) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t chainHeight = m_db.getChainHeight();
  assert(startOffset < chainHeight);

  uint64_t threshold = timestamp > m_currency.blockFutureTimeLimit()
                         ? timestamp - m_currency.blockFutureTimeLimit()
                         : 0;
  DbBlockMeta meta{};
  for (uint32_t h = static_cast<uint32_t>(startOffset); h < chainHeight; ++h) {
    m_db.getBlockMeta(h, meta);
    if (meta.timestamp >= threshold) {
      height = h;
      return true;
    }
  }
  return false;
}

bool Blockchain::getBlockContainingTransaction(const Crypto::Hash& txId, Crypto::Hash& blockId,
                                                uint32_t& blockHeight) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t block; uint16_t txSlot;
  if (!m_db.getTxIndex(txId, block, txSlot)) return false;
  blockHeight = block;
  DbBlockMeta meta{};
  m_db.getBlockMeta(block, meta);
  memcpy(blockId.data, meta.hash, 32);
  return true;
}

bool Blockchain::getAlreadyGeneratedCoins(const Crypto::Hash& hash, uint64_t& generatedCoins) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t height = 0;
  if (m_db.getHashHeight(hash, height)) {
    DbBlockMeta meta{};
    m_db.getBlockMeta(height, meta);
    generatedCoins = meta.alreadyGeneratedCoins;
    return true;
  }
  auto it = m_alternative_chains.find(hash);
  if (it != m_alternative_chains.end()) {
    generatedCoins = it->second.already_generated_coins;
    return true;
  }
  logger(DEBUGGING) << "Can't find block with hash " << hash
    << " to get already generated coins.";
  return false;
}

bool Blockchain::getBlockSize(const Crypto::Hash& hash, size_t& size) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t height = 0;
  if (m_db.getHashHeight(hash, height)) {
    DbBlockMeta meta{};
    m_db.getBlockMeta(height, meta);
    size = meta.blockCumulativeSize;
    return true;
  }
  auto it = m_alternative_chains.find(hash);
  if (it != m_alternative_chains.end()) {
    size = it->second.block_cumulative_size;
    return true;
  }
  logger(DEBUGGING) << "Can't find block with hash " << hash << " to get block size.";
  return false;
}

bool Blockchain::getGeneratedTransactionsNumber(uint32_t height, uint64_t& generatedTransactions) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return m_db.getGeneratedTxCount(height, generatedTransactions);
}

bool Blockchain::getOrphanBlockIdsByHeight(uint32_t height, std::vector<Crypto::Hash>& blockHashes) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  auto range = m_orphanBlocksIndex.equal_range(height);
  for (auto it = range.first; it != range.second; ++it) {
    blockHashes.push_back(it->second);
  }
  return true;
}

bool Blockchain::getBlockIdsByTimestamp(uint64_t timestampBegin, uint64_t timestampEnd,
                                         uint32_t blocksNumberLimit,
                                         std::vector<Crypto::Hash>& hashes,
                                         uint32_t& blocksNumberWithinTimestamps) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return m_db.getBlockHashesByTimestampRange(timestampBegin, timestampEnd,
                                              blocksNumberLimit, hashes, blocksNumberWithinTimestamps);
}

bool Blockchain::getTransactionIdsByPaymentId(const Crypto::Hash& paymentId,
                                               std::vector<Crypto::Hash>& transactionHashes) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  return m_db.getPaymentIdTxHashes(paymentId, transactionHashes);
}

bool Blockchain::isBlockInMainChain(const Crypto::Hash& blockId) {
  uint32_t h = 0;
  return m_db.getHashHeight(blockId, h);
}

bool Blockchain::isInCheckpointZone(const uint32_t height) {
  return m_checkpoints.is_in_checkpoint_zone(height);
}

// ─── Account number lookups ──────────────────────────────────────────────────

bool Blockchain::resolveAccountNumber(uint32_t blockHeight, uint32_t txIndex,
                                      AccountPublicAddress& address) {
  std::lock_guard<std::recursive_mutex> lk(m_blockchain_lock);
  const uint32_t chainHeight = m_db.getChainHeight();
  if (blockHeight >= chainHeight || txIndex == 0 ||
      txIndex > std::numeric_limits<uint16_t>::max()) {
    return false;
  }
  uint8_t spendKey[32], viewKey[32];
  if (m_db.getAccountRegistration(blockHeight, txIndex, spendKey, viewKey)) {
    memcpy(address.spendPublicKey.data, spendKey, 32);
    memcpy(address.viewPublicKey.data, viewKey, 32);
    return true;
  }

  // Fallback: registration may predate the index — fetch the tx directly
  try {
    TransactionEntry te = transactionByIndex({ blockHeight, static_cast<uint16_t>(txIndex) });
    TransactionExtraAccountRegistration reg;
    if (getAccountRegistrationFromExtra(te.tx.extra, reg) &&
        isWellFormedAccountRegistration(te.tx.extra)) {
      address.spendPublicKey = reg.spendPublicKey;
      address.viewPublicKey = reg.viewPublicKey;
      return true;
    }
  } catch (...) {
  }
  return false;
}

bool Blockchain::getAccountNumber(const AccountPublicAddress& address,
                                  uint32_t& blockHeight, uint32_t& txIndex) {
  std::lock_guard<std::recursive_mutex> lk(m_blockchain_lock);
  return m_db.findAccountRegistrationByKeys(address.spendPublicKey.data,
                                            address.viewPublicKey.data,
                                            blockHeight, txIndex);
}

void Blockchain::invalidateAccountRegistrationsCountCache() {
  m_accountRegistrationsCountCacheTime = std::chrono::steady_clock::time_point();
}

bool Blockchain::getCanonicalAccountRegistrationsCount(uint64_t& count) {
  std::lock_guard<std::recursive_mutex> lk(m_blockchain_lock);
  const auto now = std::chrono::steady_clock::now();
  if (m_accountRegistrationsCountCacheTime != std::chrono::steady_clock::time_point() &&
      now - m_accountRegistrationsCountCacheTime < std::chrono::seconds(ACCOUNT_REGISTRATIONS_COUNT_CACHE_SECONDS)) {
    count = m_cachedCanonicalAccountRegistrationsCount;
    return true;
  }

  if (!m_db.getCanonicalAccountRegistrationsCount(count)) {
    return false;
  }

  m_cachedCanonicalAccountRegistrationsCount = count;
  m_accountRegistrationsCountCacheTime = now;
  return true;
}

// ─── blockDifficulty / blockCumulativeDifficulty / getblockEntry ─────────────

uint64_t Blockchain::blockDifficulty(size_t i) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t chainHeight = m_db.getChainHeight();
  if (!(i < chainHeight)) {
    logger(ERROR, BRIGHT_RED) << "wrong block index i = " << i
      << " at Blockchain::block_difficulty()";
    return 0;
  }
  DbBlockMeta meta{};
  m_db.getBlockMeta(static_cast<uint32_t>(i), meta);
  if (i == 0) return meta.cumulativeDifficulty;
  DbBlockMeta prevMeta{};
  m_db.getBlockMeta(static_cast<uint32_t>(i) - 1, prevMeta);
  return meta.cumulativeDifficulty - prevMeta.cumulativeDifficulty;
}

uint64_t Blockchain::blockCumulativeDifficulty(size_t i) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (!(i < m_db.getChainHeight())) {
    logger(ERROR, BRIGHT_RED) << "wrong block index i = " << i
      << " at Blockchain::blockCumulativeDifficulty()";
    return 0;
  }
  DbBlockMeta meta{};
  m_db.getBlockMeta(static_cast<uint32_t>(i), meta);
  return meta.cumulativeDifficulty;
}

bool Blockchain::getblockEntry(size_t i, uint64_t& block_cumulative_size,
                                difficulty_type& difficulty, uint64_t& already_generated_coins,
                                uint64_t& reward, uint64_t& transactions_count,
                                uint64_t& timestamp) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  if (!(i < m_db.getChainHeight())) {
    logger(ERROR, BRIGHT_RED) << "wrong block index i = " << i
      << " at Blockchain::get_block_entry()";
    return false;
  }
  DbBlockMeta meta{};
  m_db.getBlockMeta(static_cast<uint32_t>(i), meta);

  block_cumulative_size  = meta.blockCumulativeSize;
  already_generated_coins = meta.alreadyGeneratedCoins;
  timestamp              = meta.timestamp;
  transactions_count     = meta.txCount > 0 ? meta.txCount - 1 : 0; // exclude coinbase

  if (i == 0) {
    difficulty = meta.cumulativeDifficulty;
    reward     = meta.alreadyGeneratedCoins;
  } else {
    DbBlockMeta prevMeta{};
    m_db.getBlockMeta(static_cast<uint32_t>(i) - 1, prevMeta);
    difficulty = meta.cumulativeDifficulty - prevMeta.cumulativeDifficulty;
    reward     = meta.alreadyGeneratedCoins - prevMeta.alreadyGeneratedCoins;
  }
  return true;
}

bool Blockchain::getBlockStats(uint32_t startHeight, uint32_t endHeight, std::vector<BlockStatsEntry>& stats) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  const uint32_t chainHeight = m_db.getChainHeight();
  if (startHeight > endHeight || endHeight >= chainHeight) {
    logger(ERROR, BRIGHT_RED) << "wrong block range [" << startHeight << ", " << endHeight
      << "] at Blockchain::getBlockStats(), blockchain height = " << chainHeight;
    return false;
  }

  const uint32_t fromHeight = startHeight == 0 ? 0 : startHeight - 1;
  std::vector<DbBlockMeta> metas;
  if (!m_db.getBlockMetaRange(fromHeight, endHeight, metas)) {
    logger(ERROR, BRIGHT_RED) << "failed to read block meta range [" << fromHeight << ", " << endHeight
      << "] at Blockchain::getBlockStats()";
    return false;
  }

  const size_t expectedMetas = static_cast<size_t>(endHeight) - fromHeight + 1;
  if (metas.size() != expectedMetas) {
    logger(ERROR, BRIGHT_RED) << "incomplete block meta range [" << fromHeight << ", " << endHeight
      << "] at Blockchain::getBlockStats(): expected " << expectedMetas << ", got " << metas.size();
    return false;
  }

  stats.clear();
  stats.reserve(static_cast<size_t>(endHeight) - startHeight + 1);

  for (uint32_t height = startHeight; height <= endHeight; ++height) {
    const size_t metaIndex = static_cast<size_t>(height) - fromHeight;
    const DbBlockMeta& meta = metas[metaIndex];
    const DbBlockMeta* prevMeta = height == 0 ? nullptr : &metas[metaIndex - 1];
    stats.push_back(makeBlockStatsEntry(height, meta, prevMeta));
  }

  return true;
}

bool Blockchain::getBlockStats(const std::vector<uint32_t>& heights, std::vector<BlockStatsEntry>& stats) {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);

  stats.clear();
  if (heights.empty()) {
    return true;
  }

  const uint32_t chainHeight = m_db.getChainHeight();
  std::vector<uint32_t> metaHeights;
  metaHeights.reserve(heights.size() * 2);

  for (const uint32_t height : heights) {
    if (height >= chainHeight) {
      logger(ERROR, BRIGHT_RED) << "wrong block height " << height
        << " at Blockchain::getBlockStats(), blockchain height = " << chainHeight;
      return false;
    }

    if (height > 0) {
      metaHeights.push_back(height - 1);
    }
    metaHeights.push_back(height);
  }

  std::sort(metaHeights.begin(), metaHeights.end());
  metaHeights.erase(std::unique(metaHeights.begin(), metaHeights.end()), metaHeights.end());

  std::vector<DbBlockMeta> metas;
  if (!m_db.getBlockMetaForHeights(metaHeights, metas) || metas.size() != metaHeights.size()) {
    logger(ERROR, BRIGHT_RED) << "failed to read sparse block meta set at Blockchain::getBlockStats()";
    return false;
  }

  stats.reserve(heights.size());
  for (const uint32_t height : heights) {
    auto metaIt = std::lower_bound(metaHeights.begin(), metaHeights.end(), height);
    const size_t metaIndex = static_cast<size_t>(std::distance(metaHeights.begin(), metaIt));
    const DbBlockMeta& meta = metas[metaIndex];

    const DbBlockMeta* prevMeta = nullptr;
    if (height > 0) {
      auto prevMetaIt = std::lower_bound(metaHeights.begin(), metaHeights.end(), height - 1);
      const size_t prevMetaIndex = static_cast<size_t>(std::distance(metaHeights.begin(), prevMetaIt));
      prevMeta = &metas[prevMetaIndex];
    }

    stats.push_back(makeBlockStatsEntry(height, meta, prevMeta));
  }

  return true;
}

// ─── Transaction pool integration ────────────────────────────────────────────

bool Blockchain::loadTransactions(const Block& block, std::vector<Transaction>& transactions) {
  transactions.resize(block.transactionHashes.size());
  size_t transactionSize;
  uint64_t fee;
  for (size_t i = 0; i < block.transactionHashes.size(); ++i) {
    if (!m_tx_pool.take_tx(block.transactionHashes[i], transactions[i], transactionSize, fee)) {
      tx_verification_context context;
      for (size_t j = 0; j < i; ++j) {
        if (!m_tx_pool.add_tx(transactions[i - 1 - j], context, true)) {
          throw std::runtime_error("Blockchain::loadTransactions, failed to add transaction to pool");
        }
      }
      return false;
    }
  }
  return true;
}

void Blockchain::saveTransactions(const std::vector<Transaction>& transactions) {
  tx_verification_context context;
  for (size_t i = 0; i < transactions.size(); ++i) {
    if (!m_tx_pool.add_tx(transactions[transactions.size() - 1 - i], context, true)) {
      logger(WARNING, BRIGHT_YELLOW) << "Blockchain::saveTransactions, failed to add transaction to pool";
    }
  }
}

// ─── Message queue ───────────────────────────────────────────────────────────

bool Blockchain::addMessageQueue(MessageQueue<BlockchainMessage>& messageQueue) {
  return m_messageQueueList.insert(messageQueue);
}

bool Blockchain::removeMessageQueue(MessageQueue<BlockchainMessage>& messageQueue) {
  return m_messageQueueList.remove(messageQueue);
}

void Blockchain::sendMessage(const BlockchainMessage& message) {
  for (auto iter = m_messageQueueList.begin(); iter != m_messageQueueList.end(); ++iter) {
    iter->push(message);
  }
}

// ─── Debug output ────────────────────────────────────────────────────────────

void Blockchain::print_blockchain(uint64_t start_index, uint64_t end_index) {
  std::stringstream ss;
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t chainHeight = m_db.getChainHeight();
  if (start_index >= chainHeight) {
    logger(INFO, BRIGHT_WHITE) << "Wrong starter index set: " << start_index
      << ", expected max index " << chainHeight - 1;
    return;
  }
  DbBlockMeta meta{}, prevMeta{};
  for (size_t i = start_index; i != chainHeight && i != end_index; i++) {
    m_db.getBlockMeta(static_cast<uint32_t>(i), meta);
    Crypto::Hash h;
    memcpy(h.data, meta.hash, 32);
    uint64_t diff = (i == 0) ? meta.cumulativeDifficulty
                              : (m_db.getBlockMeta(static_cast<uint32_t>(i) - 1, prevMeta),
                                 meta.cumulativeDifficulty - prevMeta.cumulativeDifficulty);
    ss << "height " << i
       << ", timestamp " << meta.timestamp
       << ", cumul_dif " << meta.cumulativeDifficulty
       << ", cumul_size " << meta.blockCumulativeSize
       << "\nid\t\t" << h
       << "\ndifficulty\t\t" << diff
       << ", tx_count " << meta.txCount << "\n";
  }
  logger(INFO, BRIGHT_WHITE) << "Current blockchain:\n" << ss.str();
  logger(INFO, BRIGHT_WHITE) << "Blockchain printed";
}

void Blockchain::print_blockchain_index() {
  std::lock_guard<decltype(m_blockchain_lock)> lk(m_blockchain_lock);
  uint32_t chainHeight = m_db.getChainHeight();
  logger(INFO, BRIGHT_WHITE) << "Current blockchain index:";
  DbBlockMeta meta{};
  for (uint32_t h = 0; h < chainHeight; ++h) {
    m_db.getBlockMeta(h, meta);
    Crypto::Hash hash;
    memcpy(hash.data, meta.hash, 32);
    logger(INFO, BRIGHT_WHITE) << "id\t\t" << hash << " height " << h;
  }
}

void Blockchain::print_blockchain_outs(const std::string& file) {
  // In LMDB mode, enumerating all outputs by amount is not straightforward.
  // Write a placeholder message.
  logger(INFO, BRIGHT_WHITE) << "print_blockchain_outs: not implemented for LMDB backend; "
    << "output file: " << file;
}

// ─── Migration from legacy SwappedVector ─────────────────────────────────────

bool Blockchain::migrateFromSwappedVector(const std::string& config_folder) {
  std::string blocksFile  = appendPath(config_folder, m_currency.blocksFileName());
  std::string indexFile   = appendPath(config_folder, m_currency.blockIndexesFileName());

  uint32_t totalBlocks = 0;
  uint32_t startBlock = 0;

  {
    SwappedVector<BlockEntry> oldBlocks;

    if (!oldBlocks.open(blocksFile, indexFile, 1024)) {
      logger(WARNING, BRIGHT_YELLOW) << "Migration: failed to open old block files";
      return false;
    }

    if (oldBlocks.empty()) {
      logger(INFO) << "Migration: old block files are empty, nothing to migrate";
      return true;
    }

    totalBlocks = static_cast<uint32_t>(oldBlocks.size());
    // Resume support: skip blocks already committed to LMDB.
    startBlock = m_db.getChainHeight();

    if (startBlock < totalBlocks) {
      if (startBlock > 0) {
        logger(INFO, BRIGHT_WHITE) << "Resuming migration from block " << startBlock
          << " of " << totalBlocks << " (" << (totalBlocks - startBlock) << " remaining).";
      }
      else {
        logger(INFO, BRIGHT_WHITE) << "Migrating " << totalBlocks << " blocks from legacy storage to LMDB...";
      }

      // Write BATCH_SIZE blocks per LMDB transaction.
      // Batching reduces commit overhead from O(totalBlocks) to O(totalBlocks/BATCH_SIZE)
      // and keeps B-tree pages hot in the mmap across multiple block writes within a batch,
      // which dramatically reduces B-tree fragmentation and page-split overhead.
      // On MDB_MAP_FULL the current batch is aborted, the map is doubled, and the same
      // batch is retried from its starting block (which is re-read from SwappedVector).
      static const uint32_t BATCH_SIZE = 1000;

      for (uint32_t batchStart = startBlock; batchStart < totalBlocks; ) {
        uint32_t batchEnd = std::min(batchStart + BATCH_SIZE, totalBlocks);

        for (;;) {  // map-full retry loop for this batch
          m_db.beginWriteTxn();
          bool ok = true;
          try {
            for (uint32_t b = batchStart; b < batchEnd && ok; ++b) {
              if (b % 10000 == 0) {
                logger(INFO, BRIGHT_WHITE) << "Migration: height " << b << " of " << totalBlocks;
              }

              // Fresh read from SwappedVector gives empty m_global_output_indexes,
              // which pushTransaction will fill in correctly.
              BlockEntry block = oldBlocks[b];
              Crypto::Hash blockHash = get_block_hash(block.bl);

              for (uint16_t t = 0; t < static_cast<uint16_t>(block.transactions.size()); ++t) {
                Crypto::Hash txHash = (t == 0)
                  ? getObjectHash(block.bl.baseTransaction)
                  : block.bl.transactionHashes[t - 1];
                if (!pushTransaction(block, txHash, { b, t })) {
                  logger(ERROR, BRIGHT_RED) << "Migration: pushTransaction failed at block " << b
                    << " tx " << t;
                  ok = false;
                  break;
                }
              }
              if (ok) pushBlock(block, blockHash);
            }

            if (ok) {
              m_db.commitTxn();
              batchStart = batchEnd;  // advance to next batch
              break;
            }
            else {
              m_db.abortTxn();
              return false;
            }

          }
          catch (const LMDBMapFullException&) {
            m_db.abortTxn();
            logger(DEBUGGING, BRIGHT_YELLOW) << "Migration: LMDB map full at block " << batchStart
              << ", resizing map and retrying batch...";
            m_db.resizeMap();
            // batchStart unchanged - retry same batch from the beginning
          }
          catch (const std::exception& e) {
            // Any non-map-full LMDB/storage error (e.g. EIO during commit):
            // abort the batch transaction and return a recoverable migration
            // failure instead of crashing daemon startup.
            m_db.abortTxn();
            logger(ERROR, BRIGHT_RED)
              << "Migration: batch [" << batchStart << ", " << (batchEnd - 1)
              << "] failed: " << e.what();
            return false;
          }
        }
      }

      logger(INFO, BRIGHT_WHITE) << "Migration complete! " << totalBlocks << " blocks migrated to LMDB.";
    } else {
      logger(INFO, BRIGHT_WHITE) << "Migration already complete (" << totalBlocks << " blocks in LMDB).";
    }
  } // oldBlocks goes out of scope and is closed here

  // Remove old SwappedVector files now that they are closed and no longer needed.
  try {
    if (std::filesystem::exists(blocksFile)) {
      std::filesystem::remove(blocksFile);
      logger(INFO) << "Migration: removed old blocks file: " << blocksFile;
    }

    if (std::filesystem::exists(indexFile)) {
      std::filesystem::remove(indexFile);
      logger(INFO) << "Migration: removed old index file: " << indexFile;
    }

    std::string cacheFile = appendPath(config_folder, m_currency.blocksCacheFileName());
    if (std::filesystem::exists(cacheFile)) {
      std::filesystem::remove(cacheFile);
      logger(INFO) << "Migration: removed old cache file: " << cacheFile;
    }

    std::string indicesFileName = appendPath(config_folder, m_currency.blockchainIndicesFileName());
    if (std::filesystem::exists(indicesFileName)) {
      std::filesystem::remove(indicesFileName);
      logger(INFO) << "Migration: removed old indices file: " << indicesFileName;
    }
  }
  catch (const std::exception& e) {
    logger(WARNING, BRIGHT_YELLOW) << "Migration: failed to remove old files: " << e.what();
  }

  return true;
}

} // namespace CryptoNote
