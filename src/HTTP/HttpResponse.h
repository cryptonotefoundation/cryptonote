// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2026, The Karbo developers
//
// This file is part of Karbo.

#pragma once

#include <map>
#include <string>
#include <ostream>

namespace CryptoNote {

class HttpResponse {
public:
  enum HTTP_STATUS {
    STATUS_200 = 200,
    STATUS_401 = 401,
    STATUS_404 = 404,
    STATUS_500 = 500
  };

  typedef std::map<std::string, std::string> Headers;

  HttpResponse();

  const Headers& getHeaders() const;
  const std::string& getBody() const;
  HTTP_STATUS getStatus() const;

  void addHeader(const std::string& name, const std::string& value);
  void setBody(const std::string& b);
  void setStatus(HTTP_STATUS s);

private:
  friend std::ostream& operator<<(std::ostream& os, const HttpResponse& resp);
  friend std::istream& operator>>(std::istream& is, HttpResponse& resp);

  HTTP_STATUS status;
  Headers headers;
  std::string body;

  static std::string statusToString(HTTP_STATUS status);
};

inline std::ostream& operator<<(std::ostream& os, const HttpResponse& resp) {
  os << "HTTP/1.1 " << static_cast<int>(resp.status) << " " 
     << HttpResponse::statusToString(resp.status) << "\r\n";
  
  for (const auto& header : resp.headers) {
    os << header.first << ": " << header.second << "\r\n";
  }
  
  os << "\r\n";
  if (!resp.body.empty()) {
    os << resp.body;
  }
  
  return os;
}

} // namespace CryptoNote
