// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2026, The Karbo developers
//
// This file is part of Karbo.

#pragma once

#include <map>
#include <string>
#include <ostream>

namespace CryptoNote {

class HttpRequest {
public:
  typedef std::map<std::string, std::string> Headers;

  const std::string& getMethod() const;
  const std::string& getUrl() const;
  const Headers& getHeaders() const;
  const std::string& getBody() const;

  void setMethod(const std::string& method);
  void setUrl(const std::string& url);
  void addHeader(const std::string& name, const std::string& value);
  void setBody(const std::string& body);

private:
  friend std::ostream& operator<<(std::ostream& os, const HttpRequest& resp);
  
  std::string method;
  std::string url;
  Headers headers;
  std::string body;
};

inline std::ostream& operator<<(std::ostream& os, const HttpRequest& req) {
  os << req.method << " " << req.url << " HTTP/1.1\r\n";
  for (const auto& header : req.headers) {
    os << header.first << ": " << header.second << "\r\n";
  }
  
  os << "\r\n";
  if (!req.body.empty()) {
    os << req.body;
  }
  
  return os;
}

} // namespace CryptoNote
