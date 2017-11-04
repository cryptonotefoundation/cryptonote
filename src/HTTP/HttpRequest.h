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

#pragma once

#include <iostream>
#include <map>
#include <string>

namespace CryptoNote {
  class HttpRequest {
  public:
    typedef std::map<std::string, std::string> Headers;

    const std::string& getMethod() const;
    const std::string& getUrl() const;
    const Headers& getHeaders() const;
    const std::string& getBody() const;

    void addHeader(const std::string& name, const std::string& value);
    void setBody(const std::string& b);
    void setUrl(const std::string& uri);

  private:
    friend class HttpParser;

    std::string method;
    std::string url;
    Headers headers;
    std::string body;

    friend std::ostream& operator<<(std::ostream& os, const HttpRequest& resp);
    std::ostream& printHttpRequest(std::ostream& os) const;
  };

  inline std::ostream& operator<<(std::ostream& os, const HttpRequest& resp) {
    return resp.printHttpRequest(os);
  }
}
