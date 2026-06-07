// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2026, The Karbo developers
//
// This file is part of Karbo.

#include "HttpResponse.h"

namespace CryptoNote {

HttpResponse::HttpResponse() : status(STATUS_200) {
}

const HttpResponse::Headers& HttpResponse::getHeaders() const {
  return headers;
}

const std::string& HttpResponse::getBody() const {
  return body;
}

HttpResponse::HTTP_STATUS HttpResponse::getStatus() const {
  return status;
}

void HttpResponse::addHeader(const std::string& name, const std::string& value) {
  headers[name] = value;
}

void HttpResponse::setBody(const std::string& b) {
  body = b;
  addHeader("Content-Length", std::to_string(body.size()));
}

void HttpResponse::setStatus(HTTP_STATUS s) {
  status = s;
}

std::string HttpResponse::statusToString(HTTP_STATUS status) {
  switch (status) {
    case STATUS_200:
      return "OK";
    case STATUS_401:
      return "Unauthorized";
    case STATUS_404:
      return "Not Found";
    case STATUS_500:
      return "Internal Server Error";
    default:
      return "Unknown";
  }
}

} // namespace CryptoNote
