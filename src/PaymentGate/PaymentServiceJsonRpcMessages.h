// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2018-2019 The Cash2 developers
// Copyright (c) 2018-2019 The Karbo developers
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

#include <exception>
#include <limits>
#include <vector>

#include "Serialization/ISerializer.h"

namespace PaymentService {

const uint32_t DEFAULT_ANONYMITY_LEVEL = 6;

class RequestSerializationError: public std::exception {
public:
  virtual const char* what() const throw() override { return "Request error"; }
};

struct Save {
  struct Request {
    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct Reset {
  struct Request {
    std::string viewSecretKey;
    uint32_t scanHeight = std::numeric_limits<uint32_t>::max();

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct Export {
  struct Request {
    std::string fileName;

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct GetViewKey {
  struct Request {
    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    std::string viewSecretKey;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct GetMnemonicSeed {
  struct Request {
    std::string address;

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    std::string mnemonicSeed;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct GetStatus {
  struct Request {
    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    uint32_t blockCount;
    uint32_t knownBlockCount;
    uint32_t localDaemonBlockCount;
    std::string lastBlockHash;
    uint32_t peerCount;
    uint64_t minimalFee;
    std::string version;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct ValidateAddress {
	struct Request {
		std::string address;
		
		void serialize(CryptoNote::ISerializer& serializer);
	};

	struct Response {
		bool isValid;
		std::string address;
		std::string spendPublicKey;
		std::string viewPublicKey;

		void serialize(CryptoNote::ISerializer& serializer);
	};
};

struct GetAddresses {
  struct Request {
    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    std::vector<std::string> addresses;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct GetAddressesCount {
  struct Request {
    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    size_t addresses_count;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct CreateAddress {
  struct Request {
    std::string spendSecretKey;
    std::string spendPublicKey;
    uint32_t scanHeight = std::numeric_limits<uint32_t>::max();
    bool reset;

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    std::string address;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct CreateAddressList {
  struct Request {
    std::vector<std::string> spendSecretKeys;
    std::vector<uint32_t> scanHeights;
    bool reset;

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    std::vector<std::string> addresses;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct DeleteAddress {
  struct Request {
    std::string address;

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct GetSpendKeys {
  struct Request {
    std::string address;

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    std::string spendSecretKey;
    std::string spendPublicKey;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct GetBalance {
  struct Request {
    std::string address;

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    uint64_t availableBalance;
    uint64_t lockedAmount;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct GetBlockHashes {
  struct Request {
    uint32_t firstBlockIndex;
    uint32_t blockCount;

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    std::vector<std::string> blockHashes;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct TransactionHashesInBlockRpcInfo {
  std::string blockHash;
  std::vector<std::string> transactionHashes;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct GetTransactionHashes {
  struct Request {
    std::vector<std::string> addresses;
    std::string blockHash;
    uint32_t firstBlockIndex = std::numeric_limits<uint32_t>::max();
    uint32_t blockCount;
    std::string paymentId;

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    std::vector<TransactionHashesInBlockRpcInfo> items;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct TransferRpcInfo {
  uint8_t type;
  std::string address;
  int64_t amount;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct TransactionRpcInfo {
  uint8_t state;
  std::string transactionHash;
  uint32_t blockIndex;
  uint32_t confirmations;
  uint64_t timestamp;
  bool isBase;
  uint64_t unlockTime;
  int64_t amount;
  uint64_t fee;
  std::vector<TransferRpcInfo> transfers;
  std::string extra;
  std::string paymentId;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct GetTransaction {
  struct Request {
    std::string transactionHash;

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    TransactionRpcInfo transaction;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct TransactionsInBlockRpcInfo {
  std::string blockHash;
  std::vector<TransactionRpcInfo> transactions;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct GetTransactions {
  struct Request {
    std::vector<std::string> addresses;
    std::string blockHash;
    uint32_t firstBlockIndex = std::numeric_limits<uint32_t>::max();
    uint32_t blockCount;
    std::string paymentId;

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    std::vector<TransactionsInBlockRpcInfo> items;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct GetUnconfirmedTransactionHashes {
  struct Request {
    std::vector<std::string> addresses;

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    std::vector<std::string> transactionHashes;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct GetTransactionSecretKey {
  struct Request {
    std::string transactionHash;

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    std::string transactionSecretKey;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct GetTransactionProof {
  struct Request {
    std::string transactionHash;
    std::string destinationAddress;
    std::string transactionSecretKey;

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    std::string transactionProof;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct GetReserveProof {
  struct Request {
    std::string address;
    std::string message;
    uint64_t amount = 0;

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    std::string reserveProof;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct SignMessage {
  struct Request {
    std::string address;
    std::string message;
  
    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    std::string address;
    std::string signature;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct VerifyMessage {
  struct Request {
    std::string address;
    std::string message;
    std::string signature;

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    bool isValid;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct WalletRpcOrder {
  std::string address;
  uint64_t amount;

  void serialize(CryptoNote::ISerializer& serializer);
};

struct SendTransaction {
  struct Request {
    std::vector<std::string> sourceAddresses;
    std::vector<WalletRpcOrder> transfers;
    std::string changeAddress;
    uint64_t fee = 0;
    uint32_t anonymity = DEFAULT_ANONYMITY_LEVEL;
    std::string extra;
    std::string paymentId;
    uint64_t unlockTime = 0;

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    std::string transactionHash;
	std::string transactionSecretKey;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct CreateDelayedTransaction {
  struct Request {
    std::vector<std::string> addresses;
    std::vector<WalletRpcOrder> transfers;
    std::string changeAddress;
    uint64_t fee = 0;
    uint32_t anonymity = DEFAULT_ANONYMITY_LEVEL;
    std::string extra;
    std::string paymentId;
    uint64_t unlockTime = 0;

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    std::string transactionHash;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct GetDelayedTransactionHashes {
  struct Request {
    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    std::vector<std::string> transactionHashes;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct DeleteDelayedTransaction {
  struct Request {
    std::string transactionHash;

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct SendDelayedTransaction {
  struct Request {
    std::string transactionHash;

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct SendFusionTransaction {
  struct Request {
    uint64_t threshold;
    uint32_t anonymity = DEFAULT_ANONYMITY_LEVEL;
    std::vector<std::string> addresses;
    std::string destinationAddress;

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    std::string transactionHash;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

struct EstimateFusion {
  struct Request {
    uint64_t threshold;
    std::vector<std::string> addresses;

    void serialize(CryptoNote::ISerializer& serializer);
  };

  struct Response {
    uint32_t fusionReadyCount;
    uint32_t totalOutputCount;

    void serialize(CryptoNote::ISerializer& serializer);
  };
};

} //namespace PaymentService
