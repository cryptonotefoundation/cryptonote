// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2026, The Karbo developers
//
// This file is part of Karbo.

#include "HttpParser.h"
#include <algorithm>
#include <stdexcept>

namespace CryptoNote {

HttpParser::HttpParser() {
}

void HttpParser::receiveRequest(std::istream& stream, HttpRequest& request) {
  std::string line;
  
  // Read request line
  if (!std::getline(stream, line) || line.empty()) {
    throw std::runtime_error("Failed to read HTTP request line");
  }
  
  // Remove \r if present
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
  
  // Parse method, URL, and version
  size_t methodEnd = line.find(' ');
  if (methodEnd == std::string::npos) {
    throw std::runtime_error("Invalid HTTP request line");
  }
  
  std::string method = line.substr(0, methodEnd);
  size_t urlEnd = line.find(' ', methodEnd + 1);
  if (urlEnd == std::string::npos) {
    throw std::runtime_error("Invalid HTTP request line");
  }
  
  std::string url = line.substr(methodEnd + 1, urlEnd - methodEnd - 1);
  
  request.setMethod(method);
  request.setUrl(url);
  
  // Read headers
  HttpRequest::Headers headers;
  receiveHeaders(stream, headers);
  for (const auto& header : headers) {
    request.addHeader(header.first, header.second);
  }
  
  // Read body if present
  size_t contentLength = getContentLength(headers);
  if (contentLength > 0) {
    std::string body;
    receiveBody(stream, body, contentLength);
    request.setBody(body);
  }
}

void HttpParser::receiveResponse(std::istream& stream, HttpResponse& response) {
  std::string line;
  
  // Read status line
  if (!std::getline(stream, line) || line.empty()) {
    throw std::runtime_error("Failed to read HTTP response line");
  }
  
  // Remove \r if present
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
  
  // Parse HTTP/1.1 200 OK
  size_t versionEnd = line.find(' ');
  if (versionEnd == std::string::npos) {
    throw std::runtime_error("Invalid HTTP response line");
  }
  
  size_t codeEnd = line.find(' ', versionEnd + 1);
  if (codeEnd == std::string::npos) {
    codeEnd = line.length();
  }
  
  std::string statusCodeStr = line.substr(versionEnd + 1, codeEnd - versionEnd - 1);
  int statusCode = std::stoi(statusCodeStr);
  
  response.setStatus(static_cast<HttpResponse::HTTP_STATUS>(statusCode));
  
  // Read headers
  HttpRequest::Headers headers;
  receiveHeaders(stream, headers);
  for (const auto& header : headers) {
    response.addHeader(header.first, header.second);
  }
  
  // Read body if present
  size_t contentLength = getContentLength(headers);
  if (contentLength > 0) {
    std::string body;
    receiveBody(stream, body, contentLength);
    response.setBody(body);
  }
}

void HttpParser::receiveHeaders(std::istream& stream, HttpRequest::Headers& headers) {
  std::string line;
  
  while (std::getline(stream, line)) {
    // Remove \r if present
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    
    // Empty line marks end of headers
    if (line.empty()) {
      break;
    }
    
    // Parse header: "Name: Value"
    size_t colonPos = line.find(':');
    if (colonPos == std::string::npos) {
      continue; // Skip malformed headers
    }
    
    std::string name = line.substr(0, colonPos);
    std::string value = line.substr(colonPos + 1);
    
    // Trim leading whitespace from value
    size_t valueStart = value.find_first_not_of(" \t");
    if (valueStart != std::string::npos) {
      value = value.substr(valueStart);
    }
    
    // Convert header name to lowercase
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    
    headers[name] = value;
  }
}

void HttpParser::receiveBody(std::istream& stream, std::string& body, size_t bodyLength) {
  body.resize(bodyLength);
  stream.read(&body[0], bodyLength);

  if (!stream || stream.gcount() != static_cast<std::streamsize>(bodyLength)) {
    throw std::runtime_error("Failed to read complete HTTP body");
  }
}

size_t HttpParser::getContentLength(const HttpRequest::Headers& headers) {
  auto it = headers.find("content-length");
  if (it == headers.end()) {
    return 0;
  }
  
  try {
    return std::stoull(it->second);
  } catch (...) {
    return 0;
  }
}

} // namespace CryptoNote
