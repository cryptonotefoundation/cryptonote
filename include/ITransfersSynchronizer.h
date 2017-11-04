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

#include <cstdint>
#include <system_error>

#include "ITransaction.h"
#include "ITransfersContainer.h"
#include "IStreamSerializable.h"

namespace CryptoNote {

struct SynchronizationStart {
  uint64_t timestamp;
  uint64_t height;
};

struct AccountSubscription {
  AccountKeys keys;
  SynchronizationStart syncStart;
  size_t transactionSpendableAge;
};

class ITransfersSubscription;

class ITransfersObserver {
public:
  virtual void onError(ITransfersSubscription* object,
    uint32_t height, std::error_code ec) {
  }

  virtual void onTransactionUpdated(ITransfersSubscription* object, const Crypto::Hash& transactionHash) {}

  /**
   * \note The sender must guarantee that onTransactionDeleted() is called only after onTransactionUpdated() is called
   * for the same \a transactionHash.
   */
  virtual void onTransactionDeleted(ITransfersSubscription* object, const Crypto::Hash& transactionHash) {}
};

class ITransfersSubscription : public IObservable < ITransfersObserver > {
public:
  virtual ~ITransfersSubscription() {}

  virtual AccountPublicAddress getAddress() = 0;
  virtual ITransfersContainer& getContainer() = 0;
};

class ITransfersSynchronizerObserver {
public:
  virtual void onBlocksAdded(const Crypto::PublicKey& viewPublicKey, const std::vector<Crypto::Hash>& blockHashes) {}
  virtual void onBlockchainDetach(const Crypto::PublicKey& viewPublicKey, uint32_t blockIndex) {}
  virtual void onTransactionDeleteBegin(const Crypto::PublicKey& viewPublicKey, Crypto::Hash transactionHash) {}
  virtual void onTransactionDeleteEnd(const Crypto::PublicKey& viewPublicKey, Crypto::Hash transactionHash) {}
  virtual void onTransactionUpdated(const Crypto::PublicKey& viewPublicKey, const Crypto::Hash& transactionHash,
    const std::vector<ITransfersContainer*>& containers) {}
};

class ITransfersSynchronizer : public IStreamSerializable {
public:
  virtual ~ITransfersSynchronizer() {}

  virtual ITransfersSubscription& addSubscription(const AccountSubscription& acc) = 0;
  virtual bool removeSubscription(const AccountPublicAddress& acc) = 0;
  virtual void getSubscriptions(std::vector<AccountPublicAddress>& subscriptions) = 0;
  // returns nullptr if address is not found
  virtual ITransfersSubscription* getSubscription(const AccountPublicAddress& acc) = 0;
  virtual std::vector<Crypto::Hash> getViewKeyKnownBlocks(const Crypto::PublicKey& publicViewKey) = 0;
};

}
