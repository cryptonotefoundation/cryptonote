// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

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

  void HttpRequest::addHeader(const std::string& name, const std::string& value) {
    headers[name] = value;
  }
  void HttpRequest::setBody(const std::string& b) {
    body = b;
    if (!body.empty()) {
      headers["Content-Length"] = std::to_string(body.size());
    }
    else {
      headers.erase("Content-Length");
    }
  }

  void HttpRequest::setUrl(const std::string& u) {
    url = u;
  }

  std::ostream& HttpRequest::printHttpRequest(std::ostream& os) const {
    os << "POST " << url << " HTTP/1.1\r\n";
    auto host = headers.find("Host");
    if (host == headers.end()) {
      os << "Host: " << "127.0.0.1" << "\r\n";
    }

    for (auto pair : headers) {
      os << pair.first << ": " << pair.second << "\r\n";
    }
    
    os << "\r\n";
    if (!body.empty()) {
      os << body;
    }

    return os;
  }
}
