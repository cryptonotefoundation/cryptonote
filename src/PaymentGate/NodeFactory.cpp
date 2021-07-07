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

#include "NodeFactory.h"

#include "NodeRpcProxy/NodeRpcProxy.h"
#include <memory>
#include <future>

namespace PaymentService {

class NodeRpcStub: public CryptoNote::INode {
public:
  virtual ~NodeRpcStub() {}
  virtual bool addObserver(CryptoNote::INodeObserver* observer) override { return true; }
  virtual bool removeObserver(CryptoNote::INodeObserver* observer) override { return true; }

  virtual void init(const Callback& callback) override { }
  virtual bool shutdown() override { return true; }

  virtual size_t getPeerCount() const override { return 0; }
  virtual uint32_t getLastLocalBlockHeight() const override { return 0; }
  virtual uint32_t getLastKnownBlockHeight() const override { return 0; }
  virtual uint32_t getLocalBlockCount() const override { return 0; }
  virtual uint32_t getKnownBlockCount() const override { return 0; }
  virtual uint64_t getLastLocalBlockTimestamp() const override { return 0; }
  virtual uint32_t getNodeHeight() const override { return 0; }
  virtual uint64_t getMinimalFee() const override { return 0; }
  virtual uint64_t getNextDifficulty() const override { return 0; }
  virtual uint64_t getNextReward() const override { return 0; }
  virtual uint64_t getAlreadyGeneratedCoins() const override { return 0; }
  virtual uint64_t getTransactionsCount() const { return 0; }
  virtual uint64_t getTransactionsPoolSize() const { return 0; }
  virtual uint64_t getAltBlocksCount() const { return 0; }
  virtual uint64_t getOutConnectionsCount() const { return 0; }
  virtual uint64_t getIncConnectionsCount() const { return 0; }
  virtual uint64_t getRpcConnectionsCount() const { return 0;return 0; }
  virtual uint64_t getWhitePeerlistSize() const { return 0; }
  virtual uint64_t getGreyPeerlistSize() const { return 0; }
  virtual std::string getNodeVersion() const { return ""; }

  virtual CryptoNote::BlockHeaderInfo getLastLocalBlockHeaderInfo() const override { return CryptoNote::BlockHeaderInfo(); }

  virtual void relayTransaction(const CryptoNote::Transaction& transaction, const Callback& callback) override { callback(std::error_code()); }
  virtual void getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint64_t outsCount,
    std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount>& result, const Callback& callback) override {
  }
  virtual void getNewBlocks(std::vector<Crypto::Hash>&& knownBlockIds, std::vector<CryptoNote::block_complete_entry>& newBlocks, uint32_t& startHeight, const Callback& callback) override {
    startHeight = 0;
    callback(std::error_code());
  }
  virtual void getTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash, std::vector<uint32_t>& outsGlobalIndices, const Callback& callback) override { }

  virtual void queryBlocks(std::vector<Crypto::Hash>&& knownBlockIds, uint64_t timestamp, std::vector<CryptoNote::BlockShortEntry>& newBlocks,
    uint32_t& startHeight, const Callback& callback) override {
    startHeight = 0;
    callback(std::error_code());
  };

  virtual void getPoolSymmetricDifference(std::vector<Crypto::Hash>&& knownPoolTxIds, Crypto::Hash knownBlockId, bool& isBcActual,
          std::vector<std::unique_ptr<CryptoNote::ITransactionReader>>& newTxs, std::vector<Crypto::Hash>& deletedTxIds, const Callback& callback) override {
    isBcActual = true;
    callback(std::error_code());
  }

  virtual void getBlocks(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<CryptoNote::BlockDetails>>& blocks,
    const Callback& callback) override { callback(std::error_code()); }

  virtual void getBlocks(const std::vector<Crypto::Hash>& blockHashes, std::vector<CryptoNote::BlockDetails>& blocks,
    const Callback& callback) override { callback(std::error_code()); }

  virtual void getBlocks(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t blocksNumberLimit, std::vector<CryptoNote::BlockDetails>& blocks, uint32_t& blocksNumberWithinTimestamps,
    const Callback& callback) override { callback(std::error_code()); }

  virtual void getBlock(const uint32_t blockHeight, CryptoNote::BlockDetails &block,
    const Callback& callback) override { callback(std::error_code()); }

  virtual void getTransactions(const std::vector<Crypto::Hash>& transactionHashes, std::vector<CryptoNote::TransactionDetails>& transactions,
    const Callback& callback) override { callback(std::error_code()); }

  virtual void getTransaction(const Crypto::Hash& transactionHash, CryptoNote::Transaction& transaction, const Callback& callback) override { callback(std::error_code()); }

  virtual void getPoolTransactions(uint64_t timestampBegin, uint64_t timestampEnd, uint32_t transactionsNumberLimit, std::vector<CryptoNote::TransactionDetails>& transactions, uint64_t& transactionsNumberWithinTimestamps,
    const Callback& callback) override { callback(std::error_code()); }

  virtual void getTransactionsByPaymentId(const Crypto::Hash& paymentId, std::vector<CryptoNote::TransactionDetails>& transactions, 
    const Callback& callback) override { callback(std::error_code()); }

  virtual void getMultisignatureOutputByGlobalIndex(uint64_t amount, uint32_t gindex, CryptoNote::MultisignatureOutput& out,
    const Callback& callback) override { callback(std::error_code()); }

  void getBlockTimestamp(uint32_t height, uint64_t& timestamp, const Callback& callback) { callback(std::error_code()); }

  virtual void isSynchronized(bool& syncStatus, const Callback& callback) override { callback(std::error_code()); }

  virtual void getConnections(std::vector<CryptoNote::p2pConnection>& connections, const Callback& callback) override { callback(std::error_code()); }

  virtual std::string feeAddress() const override { return std::string(); }
  virtual uint64_t feeAmount() const override { return 0; }

  virtual void setRootCert(const std::string &path) override { }
  virtual void disableVerify() override { }

};


class NodeInitObserver {
public:
  NodeInitObserver() {
    initFuture = initPromise.get_future();
  }

  void initCompleted(std::error_code result) {
    initPromise.set_value(result);
  }

  void waitForInitEnd() {
    std::error_code ec = initFuture.get();
    if (ec) {
      throw std::system_error(ec);
    }
    return;
  }

private:
  std::promise<std::error_code> initPromise;
  std::future<std::error_code> initFuture;
};

NodeFactory::NodeFactory() {
}

NodeFactory::~NodeFactory() {
}

CryptoNote::INode* NodeFactory::createNode(const std::string& daemonAddress,
                                           uint16_t daemonPort,
                                           const std::string &daemonPath,
                                           const bool &daemonSSL) {
  std::unique_ptr<CryptoNote::INode> node(new CryptoNote::NodeRpcProxy(daemonAddress, daemonPort, daemonPath, daemonSSL));

  NodeInitObserver initObserver;
  node->init(std::bind(&NodeInitObserver::initCompleted, &initObserver, std::placeholders::_1));
  initObserver.waitForInitEnd();

  return node.release();
}

CryptoNote::INode* NodeFactory::createNodeStub() {
  return new NodeRpcStub();
}

}
