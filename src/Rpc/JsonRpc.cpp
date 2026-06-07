// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2026, The Karbo developers
//
// This file is part of Karbo.

#include "JsonRpc.h"
#include <HTTP/HttpClient.h>
#include <HTTP/HttpRequest.h>
#include <HTTP/HttpResponse.h>
#include <Common/base64.hpp>
#include "CryptoNoteCore/TransactionPool.h"

namespace CryptoNote {

namespace JsonRpc {

JsonRpcError::JsonRpcError() : code(0) {}

JsonRpcError::JsonRpcError(int c) : code(c) {
  switch (c) {
  case errParseError: message = "Parse error"; break;
  case errInvalidRequest: message = "Invalid request"; break;
  case errMethodNotFound: message = "Method not found"; break;
  case errInvalidParams: message = "Invalid params"; break;
  case errInternalError: message = "Internal error"; break;
  default: message = "Unknown error"; break;
  }
}

JsonRpcError::JsonRpcError(int c, const std::string& msg) : code(c), message(msg) {
}

void invokeJsonRpcCommand(HttpClient& httpClient, JsonRpcRequest& jsReq, JsonRpcResponse& jsRes,
  const std::string& user, const std::string& password) {
  HttpRequest httpReq;
  HttpResponse httpRes;

  httpReq.setMethod("POST");
  httpReq.setUrl("/json_rpc");
  httpReq.addHeader("Content-Type", "application/json");
  httpReq.addHeader("User-Agent", "NodeRpcProxy");

  if (!user.empty() || !password.empty()) {
    httpReq.addHeader("Authorization", "Basic " + base64::encode(Common::asBinaryArray(user + ":" + password)));
  }

  httpReq.setBody(jsReq.getBody());

  httpClient.request(httpReq, httpRes);

  if (httpRes.getStatus() != HttpResponse::STATUS_200) {
    throw std::runtime_error("JSON-RPC call failed, HTTP status = " + std::to_string(httpRes.getStatus()));
  }

  jsRes.parse(httpRes.getBody());

  JsonRpcError err;
  if (jsRes.getError(err)) {
    throw err;
  }
}

}
}
