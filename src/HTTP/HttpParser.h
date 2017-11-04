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

#ifndef HTTPPARSER_H_
#define HTTPPARSER_H_

#include <iostream>
#include <map>
#include <string>
#include "HttpRequest.h"
#include "HttpResponse.h"

namespace CryptoNote {

//Blocking HttpParser
class HttpParser {
public:
  HttpParser() {};

  void receiveRequest(std::istream& stream, HttpRequest& request);
  void receiveResponse(std::istream& stream, HttpResponse& response);
  static HttpResponse::HTTP_STATUS parseResponseStatusFromString(const std::string& status);
private:
  void readWord(std::istream& stream, std::string& word);
  void readHeaders(std::istream& stream, HttpRequest::Headers &headers);
  bool readHeader(std::istream& stream, std::string& name, std::string& value);
  size_t getBodyLen(const HttpRequest::Headers& headers);
  void readBody(std::istream& stream, std::string& body, const size_t bodyLen);
};

} //namespace CryptoNote

#endif /* HTTPPARSER_H_ */
