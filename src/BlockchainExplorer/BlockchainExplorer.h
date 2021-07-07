// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2019, The Karbo developers
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

#include <mutex>
#include <atomic>
#include <unordered_set>

#include "IBlockchainExplorer.h"
#include "INode.h"

#include "Common/ObserverManager.h"
#include "BlockchainExplorerErrors.h"

#include "Wallet/WalletAsyncContextCounter.h"

#include "Logging/LoggerRef.h"

namespace CryptoNote {

class BlockchainExplorer : public IBlockchainExplorer, public INodeObserver {
public:
  BlockchainExplorer(INode& node, Logging::ILogger& logger);

  BlockchainExplorer(const BlockchainExplorer&) = delete;
  BlockchainExplorer(BlockchainExplorer&&) = delete;

  BlockchainExplorer& operator=(const BlockchainExplorer&) = delete;
  BlockchainExplorer& operator=(BlockchainExplorer&&) = delete;

  virtual ~BlockchainExplorer();
    
  virtual bool addObserver(IBlockchainObserver* observer) override;
  virtual bool removeObserver(IBlockchainObserver* observer) override;

  virtual bool getBlocks(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks) override;
  virtual bool getBlocks(const std::vector<Crypto::Hash>& blockHashes, std::vector<BlockDetails>& blocks) override;
  virtual bool getBlocks(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<BlockDetails>& blocks, uint32_t& blocksNumberWithinTimestamps) override;

  virtual bool getBlockchainTop(BlockDetails& topBlock) override;

  virtual bool getTransactions(const std::vector<Crypto::Hash>& transactionHashes, std::vector<TransactionDetails>& transactions) override;
  virtual bool getTransactionsByPaymentId(const Crypto::Hash& paymentId, std::vector<TransactionDetails>& transactions) override;
  virtual bool getPoolTransactions(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<TransactionDetails>& transactions, uint64_t& transactionsNumberWithinTimestamps) override;
  virtual bool getPoolState(const std::vector<Crypto::Hash>& knownPoolTransactionHashes, Crypto::Hash knownBlockchainTop, bool& isBlockchainActual, std::vector<TransactionDetails>& newTransactions, std::vector<Crypto::Hash>& removedTransactions) override;

  virtual uint64_t getRewardBlocksWindow() override;
  virtual uint64_t getFullRewardMaxBlockSize(uint8_t majorVersion) override;

  virtual bool isSynchronized() override;

  virtual void init() override;
  virtual void shutdown() override;

  virtual void poolChanged() override;
  virtual void blockchainSynchronized(uint32_t topHeight) override;
  virtual void localBlockchainUpdated(uint32_t height) override;

  typedef WalletAsyncContextCounter AsyncContextCounter;

private:
  void poolUpdateEndHandler();

  class PoolUpdateGuard {
  public:
    PoolUpdateGuard();

    bool beginUpdate();
    bool endUpdate();

  private:
    enum class State {
      NONE,
      UPDATING,
      UPDATE_REQUIRED
    };

    std::atomic<State> m_state;
  };

  enum State {
    NOT_INITIALIZED,
    INITIALIZED
  };

  BlockDetails knownBlockchainTop;
  uint32_t knownBlockchainTopHeight;
  std::unordered_set<Crypto::Hash> knownPoolState;

  std::atomic<State> state;
  std::atomic<bool> synchronized;
  std::atomic<uint32_t> observersCounter;
  Tools::ObserverManager<IBlockchainObserver> observerManager;

  std::mutex mutex;

  INode& node;
  Logging::LoggerRef logger;

  AsyncContextCounter asyncContextCounter;
  PoolUpdateGuard poolUpdateGuard;
};
}
