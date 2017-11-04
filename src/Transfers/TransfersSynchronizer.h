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

#pragma once

#include "Common/ObserverManager.h"
#include "ITransfersSynchronizer.h"
#include "IBlockchainSynchronizer.h"
#include "TypeHelpers.h"

#include <unordered_map>
#include <memory>
#include <cstring>

#include "Logging/LoggerRef.h"

namespace CryptoNote {
class Currency;
}

namespace CryptoNote {
 
class TransfersConsumer;
class INode;

class TransfersSyncronizer : public ITransfersSynchronizer, public IBlockchainConsumerObserver {
public:
  TransfersSyncronizer(const CryptoNote::Currency& currency, Logging::ILogger& logger, IBlockchainSynchronizer& sync, INode& node);
  virtual ~TransfersSyncronizer();

  void initTransactionPool(const std::unordered_set<Crypto::Hash>& uncommitedTransactions);

  // ITransfersSynchronizer
  virtual ITransfersSubscription& addSubscription(const AccountSubscription& acc) override;
  virtual bool removeSubscription(const AccountPublicAddress& acc) override;
  virtual void getSubscriptions(std::vector<AccountPublicAddress>& subscriptions) override;
  virtual ITransfersSubscription* getSubscription(const AccountPublicAddress& acc) override;
  virtual std::vector<Crypto::Hash> getViewKeyKnownBlocks(const Crypto::PublicKey& publicViewKey) override;

  void subscribeConsumerNotifications(const Crypto::PublicKey& viewPublicKey, ITransfersSynchronizerObserver* observer);
  void unsubscribeConsumerNotifications(const Crypto::PublicKey& viewPublicKey, ITransfersSynchronizerObserver* observer);

  // IStreamSerializable
  virtual void save(std::ostream& os) override;
  virtual void load(std::istream& in) override;

private:
  Logging::LoggerRef m_logger;

  // map { view public key -> consumer }
  typedef std::unordered_map<Crypto::PublicKey, std::unique_ptr<TransfersConsumer>> ConsumersContainer;
  ConsumersContainer m_consumers;

  typedef Tools::ObserverManager<ITransfersSynchronizerObserver> SubscribersNotifier;
  typedef std::unordered_map<Crypto::PublicKey, std::unique_ptr<SubscribersNotifier>> SubscribersContainer;
  SubscribersContainer m_subscribers;

  // std::unordered_map<AccountAddress, std::unique_ptr<TransfersConsumer>> m_subscriptions;
  IBlockchainSynchronizer& m_sync;
  INode& m_node;
  const CryptoNote::Currency& m_currency;

  virtual void onBlocksAdded(IBlockchainConsumer* consumer, const std::vector<Crypto::Hash>& blockHashes) override;
  virtual void onBlockchainDetach(IBlockchainConsumer* consumer, uint32_t blockIndex) override;
  virtual void onTransactionDeleteBegin(IBlockchainConsumer* consumer, Crypto::Hash transactionHash) override;
  virtual void onTransactionDeleteEnd(IBlockchainConsumer* consumer, Crypto::Hash transactionHash) override;
  virtual void onTransactionUpdated(IBlockchainConsumer* consumer, const Crypto::Hash& transactionHash,
    const std::vector<ITransfersContainer*>& containers) override;

  bool findViewKeyForConsumer(IBlockchainConsumer* consumer, Crypto::PublicKey& viewKey) const;
  SubscribersContainer::const_iterator findSubscriberForConsumer(IBlockchainConsumer* consumer) const;
};

}
