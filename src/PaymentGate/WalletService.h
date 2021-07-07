// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2018-2019 The Cash2 developers
// Copyright (c) 2016-2019 The Karbo developers
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

#include <System/ContextGroup.h>
#include <System/Dispatcher.h>
#include <System/Event.h>
#include "IWallet.h"
#include "INode.h"
#include "CryptoNoteCore/Currency.h"
#include "PaymentServiceJsonRpcMessages.h"
#ifdef _WIN32
#undef ERROR //TODO: workaround for windows build. fix it
#endif
#include "Logging/LoggerRef.h"

#include <fstream>
#include <memory>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/hashed_index.hpp>

namespace CryptoNote {
class IFusionManager;
}

namespace PaymentService {

struct WalletConfiguration {
  std::string walletFile;
  std::string walletPassword;
  std::string secretViewKey;
  std::string secretSpendKey;
  std::string mnemonicSeed;
  bool generateDeterministic;
  uint32_t scanHeight;
};

void generateNewWallet(const CryptoNote::Currency& currency, const WalletConfiguration& conf, Logging::ILogger& logger, System::Dispatcher& dispatcher, CryptoNote::INode& node);
void changePassword(const CryptoNote::Currency& currency, const WalletConfiguration& conf, Logging::ILogger& logger, System::Dispatcher& dispatcher, CryptoNote::INode& node, const std::string newPassword);

struct TransactionsInBlockInfoFilter;

class WalletService {
public:
  WalletService(const CryptoNote::Currency& currency, System::Dispatcher& sys, CryptoNote::INode& node, CryptoNote::IWallet& wallet,
    CryptoNote::IFusionManager& fusionManager, const WalletConfiguration& conf, Logging::ILogger& logger);
  virtual ~WalletService();

  void init();
  void saveWallet();

  std::error_code saveWalletNoThrow();
  std::error_code resetWallet();
  std::error_code resetWallet(const uint32_t scanHeight);
  std::error_code exportWallet(const std::string& fileName);
  std::error_code replaceWithNewWallet(const std::string& viewSecretKey);
  std::error_code replaceWithNewWallet(const std::string& viewSecretKey, const uint32_t scanHeight);
  std::error_code createAddress(const std::string& spendSecretKeyText, bool reset, std::string& address);
  std::error_code createAddress(const std::string& spendSecretKeyText, const uint32_t scanHeight, std::string& address);
  std::error_code createAddress(std::string& address);
  std::error_code createAddressList(const std::vector<std::string>& spendSecretKeysText, bool reset, std::vector<std::string>& addresses);
  std::error_code createAddressList(const std::vector<std::string>& spendSecretKeysText, const std::vector<uint32_t>& scanHeights, std::vector<std::string>& addresses);
  std::error_code createTrackingAddress(const std::string& spendPublicKeyText, std::string& address);
  std::error_code createTrackingAddress(const std::string& spendPublicKeyText, const uint32_t scanHeight, std::string& address);
  std::error_code deleteAddress(const std::string& address);
  std::error_code getSpendkeys(const std::string& address, std::string& publicSpendKeyText, std::string& secretSpendKeyText);
  std::error_code getBalance(const std::string& address, uint64_t& availableBalance, uint64_t& lockedAmount);
  std::error_code getBalance(uint64_t& availableBalance, uint64_t& lockedAmount);
  std::error_code getBlockHashes(uint32_t firstBlockIndex, uint32_t blockCount, std::vector<std::string>& blockHashes);
  std::error_code getViewKey(std::string& viewSecretKey);
  std::error_code getMnemonicSeed(const std::string& address, std::string& mnemonicSeed);
  std::error_code getTransactionHashes(const std::vector<std::string>& addresses, const std::string& blockHash,
    uint32_t blockCount, const std::string& paymentId, std::vector<TransactionHashesInBlockRpcInfo>& transactionHashes);
  std::error_code getTransactionHashes(const std::vector<std::string>& addresses, uint32_t firstBlockIndex,
    uint32_t blockCount, const std::string& paymentId, std::vector<TransactionHashesInBlockRpcInfo>& transactionHashes);
  std::error_code getTransactions(const std::vector<std::string>& addresses, const std::string& blockHash,
    uint32_t blockCount, const std::string& paymentId, std::vector<TransactionsInBlockRpcInfo>& transactionHashes);
  std::error_code getTransactions(const std::vector<std::string>& addresses, uint32_t firstBlockIndex,
    uint32_t blockCount, const std::string& paymentId, std::vector<TransactionsInBlockRpcInfo>& transactionHashes);
  std::error_code getTransaction(const std::string& transactionHash, TransactionRpcInfo& transaction);
  std::error_code getTransactionSecretKey(const std::string& transactionHash, std::string& transactionSecretKey);
  std::error_code getTransactionProof(const std::string& transactionHash, const std::string& destinationAddress, const std::string& transactionSecretKey, std::string& transactionProof);
  std::error_code getAddresses(std::vector<std::string>& addresses);
  std::error_code getAddressesCount(size_t& addressesCount);
  std::error_code sendTransaction(const SendTransaction::Request& request, std::string& transactionHash, std::string& transactionSecretKey);
  std::error_code createDelayedTransaction(const CreateDelayedTransaction::Request& request, std::string& transactionHash);
  std::error_code getDelayedTransactionHashes(std::vector<std::string>& transactionHashes);
  std::error_code deleteDelayedTransaction(const std::string& transactionHash);
  std::error_code sendDelayedTransaction(const std::string& transactionHash);
  std::error_code getUnconfirmedTransactionHashes(const std::vector<std::string>& addresses, std::vector<std::string>& transactionHashes);
  std::error_code getStatus(uint32_t& blockCount, uint32_t& knownBlockCount, uint32_t& localDaemonBlockCount, std::string& lastBlockHash, uint32_t& peerCount, uint64_t& minimalFee);
  std::error_code sendFusionTransaction(uint64_t threshold, uint32_t anonymity, const std::vector<std::string>& addresses,
    const std::string& destinationAddress, std::string& transactionHash);
  std::error_code estimateFusion(uint64_t threshold, const std::vector<std::string>& addresses, uint32_t& fusionReadyCount, uint32_t& totalOutputCount);
  std::error_code validateAddress(const std::string& address, bool& isValid, std::string& _address, std::string& spendPublicKey, std::string& viewPublicKey);
  std::error_code getReserveProof(std::string& reserveProof, const std::string& address, const std::string& message, const uint64_t& amount = 0);
  std::error_code signMessage(const std::string& message, const std::string& address, std::string& signature);
  std::error_code verifyMessage(const std::string& message, const std::string& signature, const std::string& address, bool& isValid);

private:
  void refresh();
  void reset();

  void loadWallet();
  void loadTransactionIdIndex();

  void replaceWithNewWallet(const Crypto::SecretKey& viewSecretKey);
  void replaceWithNewWallet(const Crypto::SecretKey& viewSecretKey, const uint32_t scanHeight);

  std::vector<CryptoNote::TransactionsInBlockInfo> getTransactions(const Crypto::Hash& blockHash, size_t blockCount) const;
  std::vector<CryptoNote::TransactionsInBlockInfo> getTransactions(uint32_t firstBlockIndex, size_t blockCount) const;

  std::vector<TransactionHashesInBlockRpcInfo> getRpcTransactionHashes(const Crypto::Hash& blockHash, size_t blockCount, const TransactionsInBlockInfoFilter& filter) const;
  std::vector<TransactionHashesInBlockRpcInfo> getRpcTransactionHashes(uint32_t firstBlockIndex, size_t blockCount, const TransactionsInBlockInfoFilter& filter) const;

  std::vector<TransactionsInBlockRpcInfo> getRpcTransactions(const Crypto::Hash& blockHash, size_t blockCount, const TransactionsInBlockInfoFilter& filter) const;
  std::vector<TransactionsInBlockRpcInfo> getRpcTransactions(uint32_t firstBlockIndex, size_t blockCount, const TransactionsInBlockInfoFilter& filter) const;

  const CryptoNote::Currency& currency;
  CryptoNote::IWallet& wallet;
  CryptoNote::IFusionManager& fusionManager;
  CryptoNote::INode& node;
  const WalletConfiguration& config;
  bool inited;
  Logging::LoggerRef logger;
  System::Dispatcher& dispatcher;
  System::Event readyEvent;
  System::ContextGroup refreshContext;

  std::map<std::string, size_t> transactionIdIndex;
};

} //namespace PaymentService
