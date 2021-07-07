// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2020, The Karbo developers
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
#include <functional>
#include <system_error>
#include <vector>
#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid.hpp>

#include "crypto/crypto.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolDefinitions.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"

#include "BlockchainExplorerData.h"
#include "ITransaction.h"

namespace CryptoNote {

class INodeObserver {
public:
  virtual ~INodeObserver() {}
  virtual void peerCountUpdated(size_t count) {}
  virtual void localBlockchainUpdated(uint32_t height) {}
  virtual void lastKnownBlockHeightUpdated(uint32_t height) {}
  virtual void poolChanged() {}
  virtual void blockchainSynchronized(uint32_t topHeight) {}
};

struct OutEntry {
  uint32_t outGlobalIndex;
  Crypto::PublicKey outKey;
};

struct OutsForAmount {
  uint64_t amount;
  std::vector<OutEntry> outs;
};

struct TransactionShortInfo {
  Crypto::Hash txId;
  TransactionPrefix txPrefix;
};

struct BlockShortEntry {
  Crypto::Hash blockHash;
  bool hasBlock;
  CryptoNote::Block block;
  std::vector<TransactionShortInfo> txsShortInfo;
};

struct BlockHeaderInfo {
  uint32_t index;
  uint8_t majorVersion;
  uint8_t minorVersion;
  uint64_t timestamp;
  Crypto::Hash hash;
  Crypto::Hash prevHash;
  uint32_t nonce;
  bool isAlternative;
  uint32_t depth; // last block index = current block index + depth
  difficulty_type difficulty;
  uint64_t reward;
};

struct p2pConnection {
  uint8_t version;
  boost::uuids::uuid connection_id;
  uint32_t remote_ip = 0;
  uint32_t remote_port = 0;
  bool is_incoming = false;
  uint64_t started = 0;

  enum state {
    state_befor_handshake = 0, //default state
    state_synchronizing,
    state_idle,
    state_normal,
    state_sync_required,
    state_pool_sync_required,
    state_shutdown
  };

  state connection_state = state_befor_handshake;
  uint32_t remote_blockchain_height = 0;
  uint32_t last_response_height = 0;
};

inline p2pConnection::state get_protocol_state_from_string(std::string s) {
  if (s == "state_befor_handshake")
    return p2pConnection::state_befor_handshake;
  if (s == "state_synchronizing")
    return p2pConnection::state_synchronizing;
  if (s == "state_idle")
    return p2pConnection::state_idle;
  if (s == "state_normal")
    return p2pConnection::state_normal;
  if (s == "state_sync_required")
    return p2pConnection::state_sync_required;
  if (s == "state_pool_sync_required")
    return p2pConnection::state_pool_sync_required;
  if (s == "state_shutdown")
    return p2pConnection::state_shutdown;
  else
    return p2pConnection::state_befor_handshake;
}

class INode {
public:
  typedef std::function<void(std::error_code)> Callback;

  virtual ~INode() {}
  virtual bool addObserver(INodeObserver* observer) = 0;
  virtual bool removeObserver(INodeObserver* observer) = 0;

  virtual void init(const Callback& callback) = 0;
  virtual bool shutdown() = 0;

  virtual size_t getPeerCount() const = 0;
  virtual uint32_t getLastLocalBlockHeight() const = 0;
  virtual uint32_t getLastKnownBlockHeight() const = 0;
  virtual uint32_t getLocalBlockCount() const = 0;
  virtual uint32_t getKnownBlockCount() const = 0;
  virtual uint64_t getMinimalFee() const = 0;
  virtual uint64_t getNextDifficulty() const = 0;
  virtual uint64_t getNextReward() const = 0;
  virtual uint64_t getAlreadyGeneratedCoins() const = 0;
  virtual uint64_t getLastLocalBlockTimestamp() const = 0;
  virtual uint32_t getNodeHeight() const = 0;
  virtual BlockHeaderInfo getLastLocalBlockHeaderInfo() const = 0;
  virtual uint64_t getTransactionsCount() const = 0;
  virtual uint64_t getTransactionsPoolSize() const = 0;
  virtual uint64_t getAltBlocksCount() const = 0;
  virtual uint64_t getOutConnectionsCount() const = 0;
  virtual uint64_t getIncConnectionsCount() const = 0;
  virtual uint64_t getRpcConnectionsCount() const = 0;
  virtual uint64_t getWhitePeerlistSize() const = 0;
  virtual uint64_t getGreyPeerlistSize() const = 0;
  virtual std::string getNodeVersion() const = 0;

  virtual std::string feeAddress() const = 0;
  virtual uint64_t feeAmount() const = 0;

  virtual void setRootCert(const std::string &path) = 0;
  virtual void disableVerify() = 0;

  virtual void relayTransaction(const Transaction& transaction, const Callback& callback) = 0;
  virtual void getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount, std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback) = 0;
  virtual void getNewBlocks(std::vector<Crypto::Hash>&& knownBlockIds, std::vector<CryptoNote::block_complete_entry>& newBlocks, uint32_t& startHeight, const Callback& callback) = 0;
  virtual void getTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash, std::vector<uint32_t>& outsGlobalIndices, const Callback& callback) = 0;
  virtual void queryBlocks(std::vector<Crypto::Hash>&& knownBlockIds, uint64_t timestamp, std::vector<BlockShortEntry>& newBlocks, uint32_t& startHeight, const Callback& callback) = 0;
  virtual void getPoolSymmetricDifference(std::vector<Crypto::Hash>&& knownPoolTxIds, Crypto::Hash knownBlockId, bool& isBcActual, std::vector<std::unique_ptr<ITransactionReader>>& newTxs, std::vector<Crypto::Hash>& deletedTxIds, const Callback& callback) = 0;
  virtual void getMultisignatureOutputByGlobalIndex(uint64_t amount, uint32_t gindex, MultisignatureOutput& out, const Callback& callback) = 0;
  virtual void getBlocks(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks, const Callback& callback) = 0;
  virtual void getBlocks(const std::vector<Crypto::Hash>& blockHashes, std::vector<BlockDetails>& blocks, const Callback& callback) = 0;
  virtual void getBlocks(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<BlockDetails>& blocks, uint32_t& blocksNumberWithinTimestamps, const Callback& callback) = 0;
  virtual void getBlock(const uint32_t blockHeight, BlockDetails &block, const Callback& callback) = 0;
  virtual void getTransaction(const Crypto::Hash& transactionHash, CryptoNote::Transaction& transaction, const Callback& callback) = 0;
  virtual void getTransactions(const std::vector<Crypto::Hash>& transactionHashes, std::vector<TransactionDetails>& transactions, const Callback& callback) = 0;
  virtual void getTransactionsByPaymentId(const Crypto::Hash& paymentId, std::vector<TransactionDetails>& transactions, const Callback& callback) = 0;
  virtual void getPoolTransactions(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<TransactionDetails>& transactions, uint64_t& transactionsNumberWithinTimestamps, const Callback& callback) = 0;
  virtual void getBlockTimestamp(uint32_t height, uint64_t& timestamp, const Callback& callback) = 0;
  virtual void isSynchronized(bool& syncStatus, const Callback& callback) = 0;
  virtual void getConnections(std::vector<p2pConnection>& connections, const Callback& callback) = 0;
};

}
