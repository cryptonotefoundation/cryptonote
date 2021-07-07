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

#include "PaymentServiceJsonRpcServer.h"

#include <functional>

#include "PaymentServiceJsonRpcMessages.h"
#include "WalletService.h"

#include "Serialization/JsonInputValueSerializer.h"
#include "Serialization/JsonOutputStreamSerializer.h"

#include "version.h"

namespace PaymentService {

PaymentServiceJsonRpcServer::PaymentServiceJsonRpcServer(System::Dispatcher& sys, System::Event& stopEvent, WalletService& service, Logging::ILogger& loggerGroup) 
  : JsonRpcServer(sys, stopEvent, loggerGroup)
  , service(service)
  , logger(loggerGroup, "PaymentServiceJsonRpcServer")
{
  handlers.emplace("save", jsonHandler<Save::Request, Save::Response>(std::bind(&PaymentServiceJsonRpcServer::handleSave, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("reset", jsonHandler<Reset::Request, Reset::Response>(std::bind(&PaymentServiceJsonRpcServer::handleReset, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("export", jsonHandler<Export::Request, Export::Response>(std::bind(&PaymentServiceJsonRpcServer::handleExport, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("createAddress", jsonHandler<CreateAddress::Request, CreateAddress::Response>(std::bind(&PaymentServiceJsonRpcServer::handleCreateAddress, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("createAddressList", jsonHandler<CreateAddressList::Request, CreateAddressList::Response>(std::bind(&PaymentServiceJsonRpcServer::handleCreateAddressList, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("deleteAddress", jsonHandler<DeleteAddress::Request, DeleteAddress::Response>(std::bind(&PaymentServiceJsonRpcServer::handleDeleteAddress, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("getSpendKeys", jsonHandler<GetSpendKeys::Request, GetSpendKeys::Response>(std::bind(&PaymentServiceJsonRpcServer::handleGetSpendKeys, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("getBalance", jsonHandler<GetBalance::Request, GetBalance::Response>(std::bind(&PaymentServiceJsonRpcServer::handleGetBalance, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("getBlockHashes", jsonHandler<GetBlockHashes::Request, GetBlockHashes::Response>(std::bind(&PaymentServiceJsonRpcServer::handleGetBlockHashes, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("getTransactionHashes", jsonHandler<GetTransactionHashes::Request, GetTransactionHashes::Response>(std::bind(&PaymentServiceJsonRpcServer::handleGetTransactionHashes, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("getTransactions", jsonHandler<GetTransactions::Request, GetTransactions::Response>(std::bind(&PaymentServiceJsonRpcServer::handleGetTransactions, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("getUnconfirmedTransactionHashes", jsonHandler<GetUnconfirmedTransactionHashes::Request, GetUnconfirmedTransactionHashes::Response>(std::bind(&PaymentServiceJsonRpcServer::handleGetUnconfirmedTransactionHashes, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("getTransaction", jsonHandler<GetTransaction::Request, GetTransaction::Response>(std::bind(&PaymentServiceJsonRpcServer::handleGetTransaction, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("getTransactionSecretKey", jsonHandler<GetTransactionSecretKey::Request, GetTransactionSecretKey::Response>(std::bind(&PaymentServiceJsonRpcServer::handleGetTransactionSecretKey, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("getTransactionProof", jsonHandler<GetTransactionProof::Request, GetTransactionProof::Response>(std::bind(&PaymentServiceJsonRpcServer::handleGetTransactionProof, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("sendTransaction", jsonHandler<SendTransaction::Request, SendTransaction::Response>(std::bind(&PaymentServiceJsonRpcServer::handleSendTransaction, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("createDelayedTransaction", jsonHandler<CreateDelayedTransaction::Request, CreateDelayedTransaction::Response>(std::bind(&PaymentServiceJsonRpcServer::handleCreateDelayedTransaction, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("getDelayedTransactionHashes", jsonHandler<GetDelayedTransactionHashes::Request, GetDelayedTransactionHashes::Response>(std::bind(&PaymentServiceJsonRpcServer::handleGetDelayedTransactionHashes, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("deleteDelayedTransaction", jsonHandler<DeleteDelayedTransaction::Request, DeleteDelayedTransaction::Response>(std::bind(&PaymentServiceJsonRpcServer::handleDeleteDelayedTransaction, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("sendDelayedTransaction", jsonHandler<SendDelayedTransaction::Request, SendDelayedTransaction::Response>(std::bind(&PaymentServiceJsonRpcServer::handleSendDelayedTransaction, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("getViewKey", jsonHandler<GetViewKey::Request, GetViewKey::Response>(std::bind(&PaymentServiceJsonRpcServer::handleGetViewKey, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("getMnemonicSeed", jsonHandler<GetMnemonicSeed::Request, GetMnemonicSeed::Response>(std::bind(&PaymentServiceJsonRpcServer::handleGetMnemonicSeed, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("getStatus", jsonHandler<GetStatus::Request, GetStatus::Response>(std::bind(&PaymentServiceJsonRpcServer::handleGetStatus, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("getAddresses", jsonHandler<GetAddresses::Request, GetAddresses::Response>(std::bind(&PaymentServiceJsonRpcServer::handleGetAddresses, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("getAddressesCount", jsonHandler<GetAddressesCount::Request, GetAddressesCount::Response>(std::bind(&PaymentServiceJsonRpcServer::handleGetAddressesCount, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("sendFusionTransaction", jsonHandler<SendFusionTransaction::Request, SendFusionTransaction::Response>(std::bind(&PaymentServiceJsonRpcServer::handleSendFusionTransaction, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("estimateFusion", jsonHandler<EstimateFusion::Request, EstimateFusion::Response>(std::bind(&PaymentServiceJsonRpcServer::handleEstimateFusion, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("validateAddress", jsonHandler<ValidateAddress::Request, ValidateAddress::Response>(std::bind(&PaymentServiceJsonRpcServer::handleValidateAddress, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("getReserveProof", jsonHandler<GetReserveProof::Request, GetReserveProof::Response>(std::bind(&PaymentServiceJsonRpcServer::handleGetReserveProof, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("signMessage", jsonHandler<SignMessage::Request, SignMessage::Response>(std::bind(&PaymentServiceJsonRpcServer::handleSignMessage, this, std::placeholders::_1, std::placeholders::_2)));
  handlers.emplace("verifyMessage", jsonHandler<VerifyMessage::Request, VerifyMessage::Response>(std::bind(&PaymentServiceJsonRpcServer::handleVerifyMessage, this, std::placeholders::_1, std::placeholders::_2)));

}

void PaymentServiceJsonRpcServer::processJsonRpcRequest(const Common::JsonValue& req, Common::JsonValue& resp) {
  try {
    prepareJsonResponse(req, resp);

    if (!req.contains("method")) {
      logger(Logging::WARNING) << "Field \"method\" is not found in json request: " << req;
      makeGenericErrorReponse(resp, "Invalid Request", -3600);
      return;
    }

    if (!req("method").isString()) {
      logger(Logging::WARNING) << "Field \"method\" is not a string type: " << req;
      makeGenericErrorReponse(resp, "Invalid Request", -3600);
      return;
    }

    std::string method = req("method").getString();

    auto it = handlers.find(method);
    if (it == handlers.end()) {
      logger(Logging::WARNING) << "Requested method not found: " << method;
      makeMethodNotFoundResponse(resp);
      return;
    }

    logger(Logging::DEBUGGING) << method << " request came";

    Common::JsonValue params(Common::JsonValue::OBJECT);
    if (req.contains("params")) {
      params = req("params");
    }

    it->second(params, resp);
  } catch (std::exception& e) {
    logger(Logging::WARNING) << "Error occurred while processing JsonRpc request: " << e.what();
    makeGenericErrorReponse(resp, e.what());
  }
}

std::error_code PaymentServiceJsonRpcServer::handleSave(const Save::Request& /*request*/, Save::Response& /*response*/) {
  return service.saveWalletNoThrow();
}

std::error_code PaymentServiceJsonRpcServer::handleReset(const Reset::Request& request, Reset::Response& response) {
  if (request.viewSecretKey.empty()) {
    if (request.scanHeight != std::numeric_limits<uint32_t>::max()) {
      return service.resetWallet(request.scanHeight);
    } else {
      return service.resetWallet();
    }
  } else {
    if (request.scanHeight != std::numeric_limits<uint32_t>::max()) {
      return service.replaceWithNewWallet(request.viewSecretKey, request.scanHeight);
    } else {
      return service.replaceWithNewWallet(request.viewSecretKey);
    }
  }
}

std::error_code PaymentServiceJsonRpcServer::handleExport(const Export::Request& request, Export::Response& /*response*/) {
  return service.exportWallet(request.fileName);
}

std::error_code PaymentServiceJsonRpcServer::handleCreateAddress(const CreateAddress::Request& request, CreateAddress::Response& response) {
  if (request.spendSecretKey.empty() && request.spendPublicKey.empty()) {
    return service.createAddress(response.address);
  } else if (!request.spendSecretKey.empty()) {
    if (request.scanHeight != std::numeric_limits<uint32_t>::max()) {
      return service.createAddress(request.spendSecretKey, request.scanHeight, response.address);
    } else {
      return service.createAddress(request.spendSecretKey, request.reset, response.address);
    }
  } else {
    if (request.scanHeight != std::numeric_limits<uint32_t>::max()) {
      return service.createTrackingAddress(request.spendPublicKey, request.scanHeight, response.address);
    } else {
      return service.createTrackingAddress(request.spendPublicKey, response.address);
    }
  }
}

std::error_code PaymentServiceJsonRpcServer::handleCreateAddressList(const CreateAddressList::Request& request, CreateAddressList::Response& response) {
  if (request.scanHeights.size() != 0) {
    return service.createAddressList(request.spendSecretKeys, request.scanHeights, response.addresses);
  } else {
    return service.createAddressList(request.spendSecretKeys, request.reset, response.addresses);
  }
}

std::error_code PaymentServiceJsonRpcServer::handleDeleteAddress(const DeleteAddress::Request& request, DeleteAddress::Response& response) {
  return service.deleteAddress(request.address);
}

std::error_code PaymentServiceJsonRpcServer::handleGetSpendKeys(const GetSpendKeys::Request& request, GetSpendKeys::Response& response) {
  return service.getSpendkeys(request.address, response.spendPublicKey, response.spendSecretKey);
}

std::error_code PaymentServiceJsonRpcServer::handleGetBalance(const GetBalance::Request& request, GetBalance::Response& response) {
  if (!request.address.empty()) {
    return service.getBalance(request.address, response.availableBalance, response.lockedAmount);
  } else {
    return service.getBalance(response.availableBalance, response.lockedAmount);
  }
}

std::error_code PaymentServiceJsonRpcServer::handleGetBlockHashes(const GetBlockHashes::Request& request, GetBlockHashes::Response& response) {
  return service.getBlockHashes(request.firstBlockIndex, request.blockCount, response.blockHashes);
}

std::error_code PaymentServiceJsonRpcServer::handleGetTransactionHashes(const GetTransactionHashes::Request& request, GetTransactionHashes::Response& response) {
  if (!request.blockHash.empty()) {
    return service.getTransactionHashes(request.addresses, request.blockHash, request.blockCount, request.paymentId, response.items);
  } else {
    return service.getTransactionHashes(request.addresses, request.firstBlockIndex, request.blockCount, request.paymentId, response.items);
  }
}

std::error_code PaymentServiceJsonRpcServer::handleGetTransactions(const GetTransactions::Request& request, GetTransactions::Response& response) {
  if (!request.blockHash.empty()) {
    return service.getTransactions(request.addresses, request.blockHash, request.blockCount, request.paymentId, response.items);
  } else {
    return service.getTransactions(request.addresses, request.firstBlockIndex, request.blockCount, request.paymentId, response.items);
  }
}

std::error_code PaymentServiceJsonRpcServer::handleGetUnconfirmedTransactionHashes(const GetUnconfirmedTransactionHashes::Request& request, GetUnconfirmedTransactionHashes::Response& response) {
  return service.getUnconfirmedTransactionHashes(request.addresses, response.transactionHashes);
}

std::error_code PaymentServiceJsonRpcServer::handleGetTransaction(const GetTransaction::Request& request, GetTransaction::Response& response) {
  return service.getTransaction(request.transactionHash, response.transaction);
}

std::error_code PaymentServiceJsonRpcServer::handleGetTransactionSecretKey(const GetTransactionSecretKey::Request& request, GetTransactionSecretKey::Response& response) {
  return service.getTransactionSecretKey(request.transactionHash, response.transactionSecretKey);
}

std::error_code PaymentServiceJsonRpcServer::handleGetTransactionProof(const GetTransactionProof::Request& request, GetTransactionProof::Response& response) {
  return service.getTransactionProof(request.transactionHash, request.destinationAddress, request.transactionSecretKey, response.transactionProof);
}

std::error_code PaymentServiceJsonRpcServer::handleGetReserveProof(const GetReserveProof::Request& request, GetReserveProof::Response& response) {
  return service.getReserveProof(response.reserveProof, request.address, request.message, request.amount);
}

std::error_code PaymentServiceJsonRpcServer::handleSignMessage(const SignMessage::Request& request, SignMessage::Response& response) {
  if (request.address.empty()) {
    std::vector<std::string> addresses;
    service.getAddresses(addresses);
    response.address = addresses[0];
  } else {
    response.address = request.address;
  }
  
  return service.signMessage(request.message, request.address, response.signature);
}

std::error_code PaymentServiceJsonRpcServer::handleVerifyMessage(const VerifyMessage::Request& request, VerifyMessage::Response& response) {
  return service.verifyMessage(request.message, request.signature, request.address, response.isValid);
}

std::error_code PaymentServiceJsonRpcServer::handleSendTransaction(const SendTransaction::Request& request, SendTransaction::Response& response) {
  return service.sendTransaction(request, response.transactionHash, response.transactionSecretKey);
}

std::error_code PaymentServiceJsonRpcServer::handleCreateDelayedTransaction(const CreateDelayedTransaction::Request& request, CreateDelayedTransaction::Response& response) {
  return service.createDelayedTransaction(request, response.transactionHash);
}

std::error_code PaymentServiceJsonRpcServer::handleGetDelayedTransactionHashes(const GetDelayedTransactionHashes::Request& request, GetDelayedTransactionHashes::Response& response) {
  return service.getDelayedTransactionHashes(response.transactionHashes);
}

std::error_code PaymentServiceJsonRpcServer::handleDeleteDelayedTransaction(const DeleteDelayedTransaction::Request& request, DeleteDelayedTransaction::Response& response) {
  return service.deleteDelayedTransaction(request.transactionHash);
}

std::error_code PaymentServiceJsonRpcServer::handleSendDelayedTransaction(const SendDelayedTransaction::Request& request, SendDelayedTransaction::Response& response) {
  return service.sendDelayedTransaction(request.transactionHash);
}

std::error_code PaymentServiceJsonRpcServer::handleGetViewKey(const GetViewKey::Request& request, GetViewKey::Response& response) {
  return service.getViewKey(response.viewSecretKey);
}

std::error_code PaymentServiceJsonRpcServer::handleGetMnemonicSeed(const GetMnemonicSeed::Request& request, GetMnemonicSeed::Response& response) {
  return service.getMnemonicSeed(request.address, response.mnemonicSeed);
}

std::error_code PaymentServiceJsonRpcServer::handleGetStatus(const GetStatus::Request& request, GetStatus::Response& response) {
  response.version = PROJECT_VERSION_LONG;
  return service.getStatus(response.blockCount, response.knownBlockCount, response.localDaemonBlockCount, response.lastBlockHash, response.peerCount, response.minimalFee);
}

std::error_code PaymentServiceJsonRpcServer::handleValidateAddress(const ValidateAddress::Request& request, ValidateAddress::Response& response) {
  return service.validateAddress(request.address, response.isValid, response.address, response.spendPublicKey, response.viewPublicKey);
}

std::error_code PaymentServiceJsonRpcServer::handleGetAddresses(const GetAddresses::Request& request, GetAddresses::Response& response) {
  return service.getAddresses(response.addresses);
}

std::error_code PaymentServiceJsonRpcServer::handleGetAddressesCount(const GetAddressesCount::Request& request, GetAddressesCount::Response& response) {
  return service.getAddressesCount(response.addresses_count);
}

std::error_code PaymentServiceJsonRpcServer::handleSendFusionTransaction(const SendFusionTransaction::Request& request, SendFusionTransaction::Response& response) {
  return service.sendFusionTransaction(request.threshold, request.anonymity, request.addresses, request.destinationAddress, response.transactionHash);
}

std::error_code PaymentServiceJsonRpcServer::handleEstimateFusion(const EstimateFusion::Request& request, EstimateFusion::Response& response) {
  return service.estimateFusion(request.threshold, request.addresses, response.fusionReadyCount, response.totalOutputCount);
}

}
  