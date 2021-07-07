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

#include <unordered_map>

#include "Common/JsonValue.h"
#include "JsonRpcServer/JsonRpcServer.h"
#include "PaymentServiceJsonRpcMessages.h"
#include "Serialization/JsonInputValueSerializer.h"
#include "Serialization/JsonOutputStreamSerializer.h"

namespace PaymentService {

class WalletService;

class PaymentServiceJsonRpcServer : public CryptoNote::JsonRpcServer {
public:
  PaymentServiceJsonRpcServer(System::Dispatcher& sys, System::Event& stopEvent, WalletService& service, Logging::ILogger& loggerGroup);
  PaymentServiceJsonRpcServer(const PaymentServiceJsonRpcServer&) = delete;

protected:
  virtual void processJsonRpcRequest(const Common::JsonValue& req, Common::JsonValue& resp) override;

private:
  WalletService& service;
  Logging::LoggerRef logger;

  typedef std::function<void (const Common::JsonValue& jsonRpcParams, Common::JsonValue& jsonResponse)> HandlerFunction;

  template <typename RequestType, typename ResponseType, typename RequestHandler>
  HandlerFunction jsonHandler(RequestHandler handler) {
    return [handler] (const Common::JsonValue& jsonRpcParams, Common::JsonValue& jsonResponse) mutable {
      RequestType request;
      ResponseType response;

      try {
        CryptoNote::JsonInputValueSerializer inputSerializer(const_cast<Common::JsonValue&>(jsonRpcParams));
        serialize(request, inputSerializer);
      } catch (std::exception&) {
        makeGenericErrorReponse(jsonResponse, "Invalid Request", -32600);
        return;
      }

      std::error_code ec = handler(request, response);
      if (ec) {
        makeErrorResponse(ec, jsonResponse);
        return;
      }

      CryptoNote::JsonOutputStreamSerializer outputSerializer;
      serialize(response, outputSerializer);
      fillJsonResponse(outputSerializer.getValue(), jsonResponse);
    };
  }

  std::unordered_map<std::string, HandlerFunction> handlers;

  std::error_code handleSave(const Save::Request& request, Save::Response& response);
  std::error_code handleReset(const Reset::Request& request, Reset::Response& response);
  std::error_code handleExport(const Export::Request& request, Export::Response& response);
  std::error_code handleCreateAddress(const CreateAddress::Request& request, CreateAddress::Response& response);
  std::error_code handleCreateAddressList(const CreateAddressList::Request& request, CreateAddressList::Response& response);
  std::error_code handleDeleteAddress(const DeleteAddress::Request& request, DeleteAddress::Response& response);
  std::error_code handleGetSpendKeys(const GetSpendKeys::Request& request, GetSpendKeys::Response& response);
  std::error_code handleGetBalance(const GetBalance::Request& request, GetBalance::Response& response);
  std::error_code handleGetBlockHashes(const GetBlockHashes::Request& request, GetBlockHashes::Response& response);
  std::error_code handleGetTransactionHashes(const GetTransactionHashes::Request& request, GetTransactionHashes::Response& response);
  std::error_code handleGetTransactions(const GetTransactions::Request& request, GetTransactions::Response& response);
  std::error_code handleGetUnconfirmedTransactionHashes(const GetUnconfirmedTransactionHashes::Request& request, GetUnconfirmedTransactionHashes::Response& response);
  std::error_code handleGetTransaction(const GetTransaction::Request& request, GetTransaction::Response& response);
  std::error_code handleGetTransactionSecretKey(const GetTransactionSecretKey::Request& request, GetTransactionSecretKey::Response& response);
  std::error_code handleGetTransactionProof(const GetTransactionProof::Request& request, GetTransactionProof::Response& response);
  std::error_code handleSendTransaction(const SendTransaction::Request& request, SendTransaction::Response& response);
  std::error_code handleCreateDelayedTransaction(const CreateDelayedTransaction::Request& request, CreateDelayedTransaction::Response& response);
  std::error_code handleGetDelayedTransactionHashes(const GetDelayedTransactionHashes::Request& request, GetDelayedTransactionHashes::Response& response);
  std::error_code handleDeleteDelayedTransaction(const DeleteDelayedTransaction::Request& request, DeleteDelayedTransaction::Response& response);
  std::error_code handleSendDelayedTransaction(const SendDelayedTransaction::Request& request, SendDelayedTransaction::Response& response);
  std::error_code handleGetViewKey(const GetViewKey::Request& request, GetViewKey::Response& response);
  std::error_code handleGetMnemonicSeed(const GetMnemonicSeed::Request& request, GetMnemonicSeed::Response& response);
  std::error_code handleGetStatus(const GetStatus::Request& request, GetStatus::Response& response);
  std::error_code handleGetAddresses(const GetAddresses::Request& request, GetAddresses::Response& response);
  std::error_code handleGetAddressesCount(const GetAddressesCount::Request& request, GetAddressesCount::Response& response);
  std::error_code handleValidateAddress(const ValidateAddress::Request& request, ValidateAddress::Response& response);
  std::error_code handleGetReserveProof(const GetReserveProof::Request& request, GetReserveProof::Response& response);
  std::error_code handleSignMessage(const SignMessage::Request& request, SignMessage::Response& response);
  std::error_code handleVerifyMessage(const VerifyMessage::Request& request, VerifyMessage::Response& response);
  std::error_code handleSendFusionTransaction(const SendFusionTransaction::Request& request, SendFusionTransaction::Response& response);
  std::error_code handleEstimateFusion(const EstimateFusion::Request& request, EstimateFusion::Response& response);
};

}//namespace PaymentService
