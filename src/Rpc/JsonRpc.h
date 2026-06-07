// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2026, The Karbo developers
//
// This file is part of Karbo.

#pragma once

#include <boost/optional.hpp>
#include <boost/foreach.hpp>
#include <functional>

#include "CoreRpcServerCommandsDefinitions.h"
#include "Common/JsonValue.h"
#include "Common/StringTools.h"
#include "Common/base64.hpp"
#include "HTTP/HttpClient.h"
#include "HTTP/HttpRequest.h"
#include "HTTP/HttpResponse.h"
#include "Serialization/ISerializer.h"
#include "Serialization/SerializationTools.h"

namespace CryptoNote {

class HttpClient;
  
namespace JsonRpc {

const int errParseError = -32700;
const int errInvalidRequest = -32600;
const int errMethodNotFound = -32601;
const int errInvalidParams = -32602;
const int errInternalError = -32603;

class JsonRpcError: public std::exception {
public:
  JsonRpcError();
  JsonRpcError(int c);
  JsonRpcError(int c, const std::string& msg);

#ifdef _MSC_VER
  virtual const char* what() const override {
#else
  virtual const char* what() const noexcept override {
#endif
    return message.c_str();
  }

  void serialize(ISerializer& s) {
    s(code, "code");
    s(message, "message");
  }

  int code;
  std::string message;
};

typedef boost::optional<Common::JsonValue> OptionalId;

class JsonRpcRequest {
public:
  
  JsonRpcRequest() : psReq(Common::JsonValue::OBJECT) {}

  bool parseRequest(const std::string& requestBody) {
    try {
      psReq = Common::JsonValue::fromString(requestBody);
    } catch (std::exception&) {
      throw JsonRpcError(errParseError);
    }

    if (!psReq.contains("method")) {
      throw JsonRpcError(errInvalidRequest);
    }

    method = psReq("method").getString();

    if (psReq.contains("id")) {
      id = psReq("id");
    }

    return true;
  }

  template <typename T>
  bool loadParams(T& v) const {
    loadFromJsonValue(v, psReq.contains("params") ? 
      psReq("params") : Common::JsonValue(Common::JsonValue::NIL));
    return true;
  }

  template <typename T>
  bool setParams(const T& v) {
    psReq.set("params", storeToJsonValue(v));
    return true;
  }

  const std::string& getMethod() const {
    return method;
  }

  void setMethod(const std::string& m) {
    method = m;
  }

  const OptionalId& getId() const {
    return id;
  }

  std::string getBody() {
    psReq.set("jsonrpc", std::string("2.0"));
    psReq.set("method", method);
    return psReq.toString();
  }

private:

  Common::JsonValue psReq;
  OptionalId id;
  std::string method;
};


class JsonRpcResponse {
public:

  JsonRpcResponse() : psResp(Common::JsonValue::OBJECT) {}

  void parse(const std::string& responseBody) {
    try {
      psResp = Common::JsonValue::fromString(responseBody);
    } catch (std::exception&) {
      throw JsonRpcError(errParseError);
    }
  }

  void setId(const OptionalId& id) {
    if (id.is_initialized()) {
      psResp.insert("id", id.get());
    }
  }

  void setError(const JsonRpcError& err) {
    psResp.set("error", storeToJsonValue(err));
  }

  bool getError(JsonRpcError& err) const {
    if (!psResp.contains("error")) {
      return false;
    }

    loadFromJsonValue(err, psResp("error"));
    return true;
  }

  std::string getBody() {
    psResp.set("jsonrpc", std::string("2.0"));
    return psResp.toString();
  }

  template <typename T>
  bool setResult(const T& v) {
    psResp.set("result", storeToJsonValue(v));
    return true;
  }

  template <typename T>
  bool getResult(T& v) const {
    if (!psResp.contains("result")) {
      return false;
    }

    loadFromJsonValue(v, psResp("result"));
    return true;
  }

private:
  Common::JsonValue psResp;
};


void invokeJsonRpcCommand(HttpClient& httpClient, JsonRpcRequest& req, JsonRpcResponse& res, const std::string& user = "", const std::string& password = "");

template <typename Request, typename Response>
void invokeJsonRpcCommand(HttpClient& httpClient, const std::string& method, const Request& req, Response& res, const std::string& user = "", const std::string& password = "") {
  JsonRpcRequest jsReq;
  JsonRpcResponse jsRes;

  jsReq.setMethod(method);
  jsReq.setParams(req);

  invokeJsonRpcCommand(httpClient, jsReq, jsRes, user, password);

  if (!jsRes.getResult(res)) {
    throw std::runtime_error("Failed to get result from JSON-RPC response");
  }
}

template <typename Request, typename Response>
void invokeJsonCommand(HttpClient& client, const std::string& url,
  const Request& req, Response& res,
  const std::string& method = "POST",
  const std::string& user = "", const std::string& password = "") {
  HttpRequest hreq;
  HttpResponse hres;

  hreq.setMethod(method);
  hreq.setUrl(url);
  hreq.addHeader("Content-Type", "application/json");
  hreq.addHeader("User-Agent", "NodeRpcProxy");

  if (!user.empty() || !password.empty()) {
    hreq.addHeader("Authorization", "Basic " + base64::encode(Common::asBinaryArray(user + ":" + password)));
  }

  hreq.setBody(storeToJson(req));

  client.request(hreq, hres);

  if (hres.getStatus() != HttpResponse::STATUS_200) {
    throw std::runtime_error("HTTP status: " + std::to_string(hres.getStatus()));
  }

  if (!loadFromJson(res, hres.getBody())) {
    throw std::runtime_error("Failed to parse JSON response");
  }
}

template <typename Request, typename Response>
void invokeBinaryCommand(HttpClient& client, const std::string& url,
  const Request& req, Response& res,
  const std::string& user = "", const std::string& password = "") {
  HttpRequest hreq;
  HttpResponse hres;

  hreq.setMethod("POST");
  hreq.setUrl(url);
  hreq.addHeader("Content-Type", "application/octet-stream");
  hreq.addHeader("User-Agent", "NodeRpcProxy");

  if (!user.empty() || !password.empty()) {
    hreq.addHeader("Authorization", "Basic " + base64::encode(Common::asBinaryArray(user + ":" + password)));
  }

  hreq.setBody(storeToBinaryKeyValue(req));

  client.request(hreq, hres);

  if (hres.getStatus() != HttpResponse::STATUS_200) {
    throw std::runtime_error("HTTP status: " + std::to_string(hres.getStatus()));
  }

  if (!loadFromBinaryKeyValue(res, hres.getBody())) {
    throw std::runtime_error("Failed to parse binary response");
  }
}

template <typename Request, typename Response, typename Handler>
bool invokeMethod(const JsonRpcRequest& jsReq, JsonRpcResponse& jsRes, Handler handler) {
  Request req;
  Response res;

  if (!std::is_same<Request, CryptoNote::EMPTY_STRUCT>::value && !jsReq.loadParams(req)) {
    throw JsonRpcError(JsonRpc::errInvalidParams);
  }

  bool result = handler(req, res);

  if (result) {
    if (!jsRes.setResult(res)) {
      throw JsonRpcError(JsonRpc::errInternalError);
    }
  }
  return result;
}

typedef std::function<bool(void*, const JsonRpcRequest& req, JsonRpcResponse& res)> JsonMemberMethod;

template <typename Class, typename Params, typename Result>
JsonMemberMethod makeMemberMethod(bool (Class::*handler)(const Params&, Result&)) {
  return [handler](void* obj, const JsonRpcRequest& req, JsonRpcResponse& res) {
    return JsonRpc::invokeMethod<Params, Result>(
      req, res, std::bind(handler, static_cast<Class*>(obj), std::placeholders::_1, std::placeholders::_2));
  };
}


}


}
