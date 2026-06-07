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

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/file.h>
#include <fcntl.h>
#endif

#include "crypto/hash.h"
#include "crypto/crypto.h"
#include "liblmdb/lmdb.h"

namespace CryptoNote {

// ─── On-disk block metadata ────────────────────────────────────────────────
// Stored packed, 100 bytes per record.
#pragma pack(push, 1)
struct DbBlockMeta {
  uint8_t  hash[32];
  uint8_t  prevHash[32];
  uint64_t timestamp;
  uint64_t cumulativeDifficulty;
  uint64_t alreadyGeneratedCoins;
  uint32_t blockCumulativeSize;
  uint32_t height;
  uint16_t txCount;
  uint8_t  majorVersion;
  uint8_t  minorVersion;  // was 'pad'; used for upgrade-detector voting
};
#pragma pack(pop)
static_assert(sizeof(DbBlockMeta) == 100, "DbBlockMeta must be 100 bytes");

// ─── Exception thrown when LMDB map is full ────────────────────────────────
class LMDBMapFullException : public std::runtime_error {
public:
  explicit LMDBMapFullException() : std::runtime_error("LMDB map full") {}
};

// ─── LMDB wrapper ──────────────────────────────────────────────────────────
class LMDBBlockchainDB {
public:
  explicit LMDBBlockchainDB();
  ~LMDBBlockchainDB();

  // No copy/move
  LMDBBlockchainDB(const LMDBBlockchainDB&)            = delete;
  LMDBBlockchainDB& operator=(const LMDBBlockchainDB&) = delete;

  bool open(const std::string& path);
  void close();
  void clear();   // drop and recreate all tables

  // Transaction control
  void beginWriteTxn();
  void beginNestedWriteTxn();
  void commitNestedWriteTxn();
  void abortNestedWriteTxn();
  void commitTxn();
  void abortTxn();

  // Chain height  (= number of blocks stored = last_height + 1)
  uint32_t getChainHeight() const;

  // ── block_meta ────────────────────────────────────────────────────────────
  bool putBlockMeta(uint32_t height, const DbBlockMeta& meta);
  bool getBlockMeta(uint32_t height, DbBlockMeta& meta) const;
  // Reads metas for [fromHeight, toHeight] inclusive in a single cursor scan
  // (one read txn, no per-entry open/close overhead).
  bool getBlockMetaRange(uint32_t fromHeight, uint32_t toHeight,
                         std::vector<DbBlockMeta>& out) const;
  // Reads the requested block_meta records with one read txn and keyed seeks.
  // `out` is appended in the same order as `heights`.
  bool getBlockMetaForHeights(const std::vector<uint32_t>& heights,
                              std::vector<DbBlockMeta>& out) const;
  bool removeLastBlockMeta();

  // ── block_data ────────────────────────────────────────────────────────────
  bool putBlockData(uint32_t height, const uint8_t* data, size_t size);
  bool getBlockData(uint32_t height, std::vector<uint8_t>& out) const;
  bool removeBlockData(uint32_t height);

  // ── tx_entries ────────────────────────────────────────────────────────────
  // Value format: [u32 tx_size][tx_blob][u32 num_gidx][u32 gidx...]
  bool putTxEntry(uint32_t height, uint16_t txIdx, const uint8_t* data, size_t size);
  bool getTxEntry(uint32_t height, uint16_t txIdx, std::vector<uint8_t>& out) const;
  bool removeTxEntriesForBlock(uint32_t height, uint16_t txCount);

  // ── hash_to_height ────────────────────────────────────────────────────────
  bool putHashHeight(const Crypto::Hash& hash, uint32_t height);
  bool getHashHeight(const Crypto::Hash& hash, uint32_t& height) const;
  bool removeHashHeight(const Crypto::Hash& hash);

  // ── hashing_blobs ─────────────────────────────────────────────────────────
  bool putHashingBlob(uint32_t height, const uint8_t* data, size_t size);
  bool getHashingBlob(uint32_t height, std::vector<uint8_t>& out) const;
  bool removeHashingBlob(uint32_t height);

  // ── spent_keys ────────────────────────────────────────────────────────────
  bool putSpentKey(const Crypto::KeyImage& ki, uint32_t blockHeight);
  bool hasSpentKey(const Crypto::KeyImage& ki) const;
  bool getSpentKeyHeight(const Crypto::KeyImage& ki, uint32_t& height) const;
  bool removeSpentKey(const Crypto::KeyImage& ki);

  // ── tx_indices ────────────────────────────────────────────────────────────
  bool putTxIndex(const Crypto::Hash& txHash, uint32_t block, uint16_t txSlot);
  bool getTxIndex(const Crypto::Hash& txHash, uint32_t& block, uint16_t& txSlot) const;
  bool removeTxIndex(const Crypto::Hash& txHash);

  // ── key_outputs ───────────────────────────────────────────────────────────
  bool putKeyOutput(uint64_t amount, uint32_t globalIdx,
                    uint32_t block, uint16_t txSlot, uint16_t outIdx);
  bool getKeyOutput(uint64_t amount, uint32_t globalIdx,
                    uint32_t& block, uint16_t& txSlot, uint16_t& outIdx) const;
  uint32_t getKeyOutputCount(uint64_t amount) const;
  bool removeLastKeyOutput(uint64_t amount);

  // ── payment_id_idx (DUPSORT) ──────────────────────────────────────────────
  bool putPaymentId(const Crypto::Hash& paymentId, const Crypto::Hash& txHash);
  bool getPaymentIdTxHashes(const Crypto::Hash& paymentId,
                             std::vector<Crypto::Hash>& txHashes) const;
  bool removePaymentId(const Crypto::Hash& paymentId, const Crypto::Hash& txHash);

  // ── timestamp_idx ─────────────────────────────────────────────────────────
  // Key = {uint64_t timestamp BE, Crypto::Hash} (40 bytes) – unique
  bool putTimestamp(uint64_t timestamp, const Crypto::Hash& blockHash);
  bool getBlockHashesByTimestampRange(uint64_t tsBegin, uint64_t tsEnd,
                                      uint32_t limit,
                                      std::vector<Crypto::Hash>& hashes,
                                      uint32_t& totalInRange) const;
  bool removeTimestamp(uint64_t timestamp, const Crypto::Hash& blockHash);

  // ── gen_tx_idx ────────────────────────────────────────────────────────────
  bool putGeneratedTxCount(uint32_t height, uint64_t count);
  bool getGeneratedTxCount(uint32_t height, uint64_t& count) const;
  bool removeGeneratedTxCount(uint32_t height);

  // ── account_registrations ───────────────────────────────────────────────
  // Key: uint64_t packed as blockHeight << 32 | txIndex (native byte order)
  // Value: 64 bytes — spend pubkey (32) || view pubkey (32)
  bool putAccountRegistration(uint32_t blockHeight, uint32_t txIndex,
                              const uint8_t* spendKey, const uint8_t* viewKey);
  bool getAccountRegistration(uint32_t blockHeight, uint32_t txIndex,
                              uint8_t* spendKey, uint8_t* viewKey) const;
  bool removeAccountRegistration(uint32_t blockHeight, uint32_t txIndex);

  // Reverse lookup: find the first (canonical) registration matching given spend+view keys.
  bool findAccountRegistrationByKeys(const uint8_t* spendKey, const uint8_t* viewKey,
                                     uint32_t& blockHeight, uint32_t& txIndex) const;
  bool getCanonicalAccountRegistrationsCount(uint64_t& count) const;

  // ── Resize when map is full ───────────────────────────────────────────────
  // Call this when no write txn is active, then re-begin the txn.
  void resizeMap();
  // Proactively doubles the map if fill ratio >= threshold (default 80%).
  // Call before beginWriteTxn() to prevent mid-batch MDB_MAP_FULL.
  void growMapIfNeeded(double threshold = 0.8);

  // ── IBD helpers ───────────────────────────────────────────────────────────
  // True while a write txn is active.  Used by Blockchain to detect an open
  // IBD batch so it can reuse rather than re-open the transaction.
  bool hasActiveTxn() const noexcept { return m_writeTxn != nullptr; }
  bool hasNestedTxn() const noexcept { return m_parentWriteTxn != nullptr; }

  // Enable/disable MDB_NOSYNC (skips ALL per-commit fdatasyncs).
  // Only safe during IBD; call with enable=false when returning to live
  // operation — that call also forces a full mdb_env_sync to flush all
  // pending writes.  Must NOT be called with enable=true for live blocks.
  void setFastSyncMode(bool enable);

  // Force-flush all dirty pages to disk (mdb_env_sync force=1) without
  // changing the MDB_NOSYNC flag.  Use as a checkpoint during IBD batches
  // so a crash reverts to the previous checkpoint rather than corrupting the DB.
  void syncToDisk();

  MDB_env* getEnv() const { return m_env; }

  // Returns true if another process holds the exclusive lock on path.
  static bool isLocked(const std::string& path);

private:
  MDB_env* m_env            = nullptr;
  MDB_txn* m_writeTxn       = nullptr;
  MDB_txn* m_parentWriteTxn = nullptr;

#ifdef _WIN32
  HANDLE m_lockFile = INVALID_HANDLE_VALUE;
#else
  int    m_lockFd   = -1;
#endif

  MDB_dbi m_dbiBlockMeta;
  MDB_dbi m_dbiBlockData;
  MDB_dbi m_dbiTxEntries;
  MDB_dbi m_dbiHashToHeight;
  MDB_dbi m_dbiHashingBlobs;
  MDB_dbi m_dbiSpentKeys;
  MDB_dbi m_dbiTxIndices;
  MDB_dbi m_dbiKeyOutputs;
  MDB_dbi m_dbiKeyOutputCounts;
  MDB_dbi m_dbiPaymentIdIdx;
  MDB_dbi m_dbiTimestampIdx;
  MDB_dbi m_dbiGenTxIdx;
  MDB_dbi m_dbiAccountRegistrations;

  // RAII guard for read transactions.  If a write txn is active the guard
  // borrows it (no cleanup); otherwise it opens a temporary read-only txn
  // that is automatically aborted when the guard goes out of scope.
  struct TxnGuard {
    MDB_txn* txn   = nullptr;
    bool     owning = false;
    TxnGuard() = default;
    TxnGuard(MDB_txn* t, bool own) : txn(t), owning(own) {}
    ~TxnGuard() { if (owning && txn) mdb_txn_abort(txn); }
    TxnGuard(const TxnGuard&)            = delete;
    TxnGuard& operator=(const TxnGuard&) = delete;
    TxnGuard(TxnGuard&& o) noexcept : txn(o.txn), owning(o.owning) { o.owning = false; }
    TxnGuard& operator=(TxnGuard&& o) noexcept {
      if (owning && txn) mdb_txn_abort(txn);
      txn = o.txn; owning = o.owning; o.owning = false;
      return *this;
    }
  };

  TxnGuard readTxn() const;

  void checkRc(int rc, const char* op) const;
  void openDb(MDB_txn* setupTxn, const char* name, unsigned int flags, MDB_dbi& dbi);

  // Helpers for big-endian encoding
  static void encBE32(uint8_t* out, uint32_t v);
  static void encBE64(uint8_t* out, uint64_t v);
  static uint32_t decBE32(const uint8_t* b);
  static uint64_t decBE64(const uint8_t* b);
};

// ─── UpgradeDetector adapter ───────────────────────────────────────────────
// Provides the Blocks-container interface that BasicUpgradeDetector<BC> needs.
class LMDBBlockView {
public:
  // Proxy for the 'bl' member that BasicUpgradeDetector accesses.
  // Carries majorVersion, minorVersion, and the pre-computed block hash
  // (read from DbBlockMeta.hash) so the free-function overload of
  // get_block_hash() below can satisfy calls like:
  //   get_block_hash(m_blockchain.back().bl)
  // without deserialising the full block from LMDB.
  struct BLProxy {
    uint8_t      majorVersion = 0;
    uint8_t      minorVersion = 0;
    Crypto::Hash blockHash{};  // copied from DbBlockMeta::hash
  };
  struct BlockProxy {
    BLProxy bl;
  };
  using value_type = BlockProxy;

  // ── Random-access iterator ──────────────────────────────────────────────
  // Because operator*() returns BlockProxy by value (a proxy object, not a
  // stored reference), operator->() cannot return BlockProxy* directly.
  // The standard solution is an "arrow proxy" that owns the value and returns
  // a pointer to it, so that it->field works correctly.
  struct ArrowProxy {
    BlockProxy val;
    const BlockProxy* operator->() const { return &val; }
  };

  struct Iterator {
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = BlockProxy;
    using difference_type   = ptrdiff_t;
    using pointer           = ArrowProxy;
    using reference         = BlockProxy;

    const LMDBBlockchainDB* db = nullptr;
    uint32_t pos = 0;

    BlockProxy operator*() const {
      DbBlockMeta m{};
      db->getBlockMeta(pos, m);
      BLProxy bl{m.majorVersion, m.minorVersion, {}};
      std::memcpy(bl.blockHash.data, m.hash, sizeof(m.hash));
      return BlockProxy{bl};
    }
    ArrowProxy operator->() const { return ArrowProxy{**this}; }
    Iterator& operator++()               { ++pos; return *this; }
    Iterator  operator++(int)            { auto t = *this; ++pos; return t; }
    Iterator& operator--()               { --pos; return *this; }
    Iterator  operator--(int)            { auto t = *this; --pos; return t; }
    Iterator& operator+=(ptrdiff_t n)    { pos = static_cast<uint32_t>(pos + n); return *this; }
    Iterator& operator-=(ptrdiff_t n)    { pos = static_cast<uint32_t>(pos - n); return *this; }
    Iterator  operator+(ptrdiff_t n) const { return {db, static_cast<uint32_t>(pos + n)}; }
    Iterator  operator-(ptrdiff_t n) const { return {db, static_cast<uint32_t>(pos - n)}; }
    ptrdiff_t operator-(const Iterator& o) const { return static_cast<ptrdiff_t>(pos) - o.pos; }
    BlockProxy operator[](ptrdiff_t n) const { return *(*this + n); }
    bool operator==(const Iterator& o) const { return pos == o.pos; }
    bool operator!=(const Iterator& o) const { return pos != o.pos; }
    bool operator< (const Iterator& o) const { return pos <  o.pos; }
    bool operator> (const Iterator& o) const { return pos >  o.pos; }
    bool operator<=(const Iterator& o) const { return pos <= o.pos; }
    bool operator>=(const Iterator& o) const { return pos >= o.pos; }
  };

  // Non-member form: n + it  (required for full random-access conformance)
  friend Iterator operator+(ptrdiff_t n, const Iterator& it) { return it + n; }

  explicit LMDBBlockView(LMDBBlockchainDB& db) : m_db(db) {}

  uint32_t size()  const { return m_db.getChainHeight(); }
  bool     empty() const { return m_db.getChainHeight() == 0; }

  BlockProxy operator[](uint32_t height) const {
    DbBlockMeta m{};
    m_db.getBlockMeta(height, m);
    BLProxy bl{m.majorVersion, m.minorVersion, {}};
    std::memcpy(bl.blockHash.data, m.hash, sizeof(m.hash));
    return BlockProxy{bl};
  }

  BlockProxy back() const {
    uint32_t h = m_db.getChainHeight();
    return (*this)[h - 1];
  }

  Iterator begin() const { return {&m_db, 0}; }
  Iterator end()   const { return {&m_db, m_db.getChainHeight()}; }

private:
  LMDBBlockchainDB& m_db;
};

// ─── ADL overload for BasicUpgradeDetector ─────────────────────────────────
// BasicUpgradeDetector calls get_block_hash(m_blockchain.back().bl) where
// bl is LMDBBlockView::BLProxy.  Providing this overload in the same namespace
// lets ADL resolve the call without touching UpgradeDetector.h or
// deserialising the full Block.
inline Crypto::Hash get_block_hash(const LMDBBlockView::BLProxy& b) {
  return b.blockHash;
}

} // namespace CryptoNote
