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

#include "CryptoNoteCore/Currency.h"
#include "INode.h"
#include "IWalletLegacy.h"
#include "System/Dispatcher.h"
#include "System/Event.h"
#include "WalletLegacy/WalletLegacy.h"

namespace Tests {
namespace Common {

class TestWalletLegacy : private CryptoNote::IWalletLegacyObserver {
public:
  TestWalletLegacy(System::Dispatcher& dispatcher, const CryptoNote::Currency& currency, CryptoNote::INode& node);
  ~TestWalletLegacy();

  std::error_code init();
  std::error_code sendTransaction(const std::string& address, uint64_t amount, Crypto::Hash& txHash);
  void waitForSynchronizationToHeight(uint32_t height);
  CryptoNote::IWalletLegacy* wallet();
  CryptoNote::AccountPublicAddress address() const;

protected:
  virtual void synchronizationCompleted(std::error_code result) override;
  virtual void synchronizationProgressUpdated(uint32_t current, uint32_t total) override;

private:
  System::Dispatcher& m_dispatcher;
  System::Event m_synchronizationCompleted;
  System::Event m_someTransactionUpdated;

  CryptoNote::INode& m_node;
  const CryptoNote::Currency& m_currency;
  std::unique_ptr<CryptoNote::IWalletLegacy> m_wallet;
  std::unique_ptr<CryptoNote::IWalletLegacyObserver> m_walletObserver;
  uint32_t m_currentHeight;
  uint32_t m_synchronizedHeight;
  std::error_code m_lastSynchronizationResult;
};

}
}
