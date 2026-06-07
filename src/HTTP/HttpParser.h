// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2026, The Karbo developers
//
// This file is part of Karbo.

#pragma once

#include <iostream>
#include "HttpRequest.h"
#include "HttpResponse.h"

namespace CryptoNote {

class HttpParser {
public:
  HttpParser();

  void receiveRequest(std::istream& stream, HttpRequest& request);
  void receiveResponse(std::istream& stream, HttpResponse& response);

private:
  void receiveHeaders(std::istream& stream, HttpRequest::Headers& headers);
  void receiveBody(std::istream& stream, std::string& body, size_t bodyLength);
  size_t getContentLength(const HttpRequest::Headers& headers);
};

} // namespace CryptoNote
