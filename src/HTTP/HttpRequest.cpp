// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2026, The Karbo developers
//
// This file is part of Karbo.

#include "HttpRequest.h"

namespace CryptoNote {

const std::string& HttpRequest::getMethod() const {
  return method;
}

const std::string& HttpRequest::getUrl() const {
  return url;
}

const HttpRequest::Headers& HttpRequest::getHeaders() const {
  return headers;
}

const std::string& HttpRequest::getBody() const {
  return body;
}

void HttpRequest::setMethod(const std::string& m) {
  method = m;
}

void HttpRequest::setUrl(const std::string& u) {
  url = u;
}

void HttpRequest::addHeader(const std::string& name, const std::string& value) {
  headers[name] = value;
}

void HttpRequest::setBody(const std::string& b) {
  body = b;
  addHeader("Content-Length", std::to_string(body.size()));
}

} // namespace CryptoNote
