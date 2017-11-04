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

#include <ostream>
#include <string>
#include <map>

namespace CryptoNote {

  class HttpResponse {
  public:
    enum HTTP_STATUS {
      STATUS_200,
      STATUS_404,
      STATUS_500
    };

    HttpResponse();

    void setStatus(HTTP_STATUS s);
    void addHeader(const std::string& name, const std::string& value);
    void setBody(const std::string& b);

    const std::map<std::string, std::string>& getHeaders() const { return headers; }
    HTTP_STATUS getStatus() const { return status; }
    const std::string& getBody() const { return body; }

  private:
    friend std::ostream& operator<<(std::ostream& os, const HttpResponse& resp);
    std::ostream& printHttpResponse(std::ostream& os) const;

    HTTP_STATUS status;
    std::map<std::string, std::string> headers;
    std::string body;
  };

  inline std::ostream& operator<<(std::ostream& os, const HttpResponse& resp) {
    return resp.printHttpResponse(os);
  }

} //namespace CryptoNote
