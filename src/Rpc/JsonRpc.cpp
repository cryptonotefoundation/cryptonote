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

#include "JsonRpc.h"
#include "HttpClient.h"
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

void invokeJsonRpcCommand(HttpClient& httpClient, JsonRpcRequest& jsReq, JsonRpcResponse& jsRes, const std::string& user, const std::string& password) {
  HttpRequest httpReq;
  HttpResponse httpRes;

  if (!user.empty() || !password.empty()) {
    httpReq.addHeader("Authorization", "Basic " + base64::encode(Common::asBinaryArray(user + ":" + password)));
  }
  httpReq.addHeader("Content-Type", "application/json");
  httpReq.setUrl("/json_rpc");
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
