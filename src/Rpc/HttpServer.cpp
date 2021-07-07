// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2016 XDN developers
// Copyright (c) 2016-2018 Karbowanec developers
//
// This file is part of Karbo.
//
// Karbo is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Karbo is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Karbo.  If not, see <http://www.gnu.org/licenses/>.

#include "HttpServer.h"
#include <thread>
#include <string.h>
#include <streambuf>
#include <array>
#include <vector>
#include <boost/scope_exit.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl/stream.hpp>

#include <Common/base64.hpp>
#include <Common/StringTools.h>
#include <HTTP/HttpParser.h>
#include <System/InterruptedException.h>
#include <System/TcpStream.h>
#include <System/SocketStream.h>
#include <System/Ipv4Address.h>

using boost::asio::ip::tcp;
using namespace Logging;


namespace {
	void fillUnauthorizedResponse(CryptoNote::HttpResponse& response) {
		response.setStatus(CryptoNote::HttpResponse::STATUS_401);
		response.addHeader("WWW-Authenticate", "Basic realm=\"RPC\"");
		response.addHeader("Content-Type", "text/plain");
		response.setBody("Authorization required");
	}
}

namespace CryptoNote {

HttpServer::HttpServer(System::Dispatcher& dispatcher, Logging::ILogger& log)
  : m_dispatcher(dispatcher), workingContextGroup(dispatcher), logger(log, "HttpServer") {
  this->m_server_ssl_do = false;
  this->m_server_ssl_is_run = false;
  this->m_server_ssl_clients = 0;
  this->m_server_ssl_port = 0;
  this->m_address = "";
  this->m_chain_file = "";
  this->m_dh_file = "";
  this->m_key_file = "";
}

void HttpServer::setCerts(const std::string& chain_file, const std::string& key_file, const std::string& dh_file){
  this->m_chain_file = chain_file;
  this->m_dh_file = dh_file;
  this->m_key_file = key_file;
}

void HttpServer::start(const std::string& address, uint16_t port, uint16_t port_ssl,
                       bool server_ssl_enable, const std::string& user, const std::string& password) {
  m_listener = System::TcpListener(m_dispatcher, System::Ipv4Address(address), port);
  workingContextGroup.spawn(std::bind(&HttpServer::acceptLoop, this));

  this->m_server_ssl_do = server_ssl_enable;
  this->m_server_ssl_port = port_ssl;
  this->m_address = address;

  if (!user.empty() || !password.empty()) {
    m_credentials = base64::encode(Common::asBinaryArray(user + ":" + password));
  }

  if (!this->m_chain_file.empty() && !this->m_key_file.empty() && !this->m_dh_file.empty() &&
      this->m_server_ssl_port != 0 && this->m_server_ssl_do) {
    this->m_ssl_server_thread = boost::thread(&HttpServer::sslServer, this);
    this->m_ssl_server_thread.detach();
  }
}

void HttpServer::stop() {
  workingContextGroup.interrupt();
  workingContextGroup.wait();
  this->m_server_ssl_do = false;
  while (this->m_server_ssl_is_run) {
    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
  }
}

void HttpServer::sslServerUnitControl(boost::asio::ssl::stream<tcp::socket&> &stream,
                                      boost::system::error_code &ec,
                                      bool &unit_do,
                                      bool &unit_control_do,
                                      size_t &stream_timeout_n) {
  const size_t unit_timeout = 200;
  while (unit_control_do) {
    if (stream_timeout_n >= unit_timeout || !this->m_server_ssl_do) {
      unit_do = false;
      break;
    } else {
      stream_timeout_n++;
    }
    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
  }
  if (unit_control_do && !unit_do) {
    try {
      stream.shutdown(ec);
    } catch (std::exception& e) {
      logger(ERROR, BRIGHT_RED) << "SSL server unit control error: " << e.what() << std::endl;
    }
  }
}

void HttpServer::sslServerUnit(boost::asio::ip::tcp::socket &socket, boost::asio::ssl::context &ctx){
  const size_t request_max_len = 1024 * 32;
  bool keep_alive_conn = true;
  bool unit_do = true;
  bool unit_control_do = true;
  size_t stream_timeout_n = 0;
  boost::system::error_code ec;
  boost::asio::ssl::stream<tcp::socket&> stream(socket, ctx);

  boost::thread control_t(std::bind(&HttpServer::sslServerUnitControl, this, std::ref(stream),
                                                                             std::ref(ec),
                                                                             std::ref(unit_do),
                                                                             std::ref(unit_control_do),
                                                                             std::ref(stream_timeout_n)));

  this->m_server_ssl_clients++;

  try {
    stream.handshake(boost::asio::ssl::stream_base::server, ec);
    if (!ec) {
      char req_buff[request_max_len];
      while (keep_alive_conn) {
        const char *header_end_sep = "\r\n\r\n";
        const char *content_lenght_name = "Content-Length";
        const char *content_lenght_end_sep = "\r\n";
        size_t req_size_full = 0;
        size_t header_end = 0;
        size_t stream_len = 0;
        bool header_found = false;
        memset(req_buff, 0x00, sizeof(char) * request_max_len);
        while (unit_do) {
          size_t req_size = 0;
          if (unit_do) req_size = stream.read_some(boost::asio::buffer((char *) req_buff + req_size_full,
                                                   request_max_len - req_size_full - 1),
                                                   ec);
          req_size_full += req_size;
          if (req_size == 0) {
            keep_alive_conn = false;
            break;
          } else {
            if (!header_found) {
              std::string data = std::string(req_buff);
              header_end = data.find(header_end_sep);
              if (header_end != std::string::npos) {
                header_found = true;
                data.resize(header_end + 2);
                data.push_back(0x00);
                size_t content_lenght_start = data.find(content_lenght_name);
                size_t content_lenght_end = data.find(content_lenght_end_sep, content_lenght_start);
                if (content_lenght_start != std::string::npos && content_lenght_end != std::string::npos) {
                  sscanf(data.substr(content_lenght_start + strlen(content_lenght_name) + 2,
                                     content_lenght_end - content_lenght_start - strlen(content_lenght_name) - 2).c_str(),
                         "%zu",
                         &stream_len);
                  stream_len += header_end + 4;
                }
              }
            }
          }
          if (header_found) {
            if (stream_len > 0) {
              if (req_size_full >= stream_len) break;
            } else {
              if (req_size_full == header_end + 4) break;
            }
          }
        }
        if (req_size_full > 0 && req_size_full < request_max_len && unit_do) {
          System::SocketStreambuf streambuf((char *) req_buff, req_size_full);

          HttpParser parser;
          HttpRequest req;
          HttpResponse resp;
          resp.addHeader("Access-Control-Allow-Origin", "*");

          std::iostream io_stream(&streambuf);
          parser.receiveRequest(io_stream, req);

          if (authenticate(req)) {
            processRequest(req, resp);
          } else {
            logger(WARNING) << "Authorization required" << std::endl;
          }
          io_stream << resp;
          io_stream.flush();

          std::vector<uint8_t> resp_data;
          streambuf.getRespdata(resp_data);
          size_t resp_size_data = resp_data.size();
          size_t resp_size_full = 0;
          while (resp_size_full < resp_size_data && unit_do) {
            size_t resp_size = 0;
            if (unit_do) resp_size = stream.write_some(boost::asio::buffer(resp_data.data() + resp_size_full,
                                                       resp_size_data - resp_size_full),
                                                       ec);
            if (resp_size > 0) {
              resp_size_full += resp_size;
            } else {
              keep_alive_conn = false;
              break;
            }
          }
          stream_timeout_n = 0;
        } else {
          logger(DEBUGGING) << "Unable to process request (SSL server)" << std::endl;
        }
      }
    }
  } catch (std::exception& e) {
    logger(ERROR, BRIGHT_RED) << "SSL server unit error: " << e.what() << std::endl;
  }
  unit_control_do = false;
  control_t.join();
  this->m_server_ssl_clients--;
}

void HttpServer::sslServerControl(tcp::acceptor &accept) {
  while (this->m_server_ssl_do) boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
  while (this->m_server_ssl_clients > 0) {
    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
  }
  if (accept.is_open()) {
    boost::system::error_code ec;
    accept.close(ec);
    if (ec) {
      logger(DEBUGGING) << "SSL server control error" << std::endl;
    }
  }
  this->m_ssl_server_thread.interrupt();
  this->m_server_ssl_is_run = false;
}

void HttpServer::sslServer() {
  while (this->m_server_ssl_do) {
    this->m_server_ssl_is_run = false;
    try {
      boost::asio::io_service io_service;
      tcp::acceptor accept(io_service, tcp::endpoint(boost::asio::ip::address::from_string(this->m_address),
                                                     this->m_server_ssl_port));

      boost::thread control_t(std::bind(&HttpServer::sslServerControl, this, std::ref(accept)));
      control_t.detach();

      boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
      ctx.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2);
      ctx.use_certificate_chain_file(this->m_chain_file);
      ctx.use_private_key_file(this->m_key_file, boost::asio::ssl::context::pem);
      ctx.use_tmp_dh_file(this->m_dh_file);

      while (this->m_server_ssl_do) {
        tcp::socket sock(io_service);
        this->m_server_ssl_is_run = true;
        accept.accept(sock);
        if (this->m_server_ssl_do) {
          boost::thread t(std::bind(&HttpServer::sslServerUnit, this, std::move(sock), std::ref(ctx)));
          t.detach();
        }
      }
    } catch (std::exception& e) {
      if (this->m_server_ssl_do) {
        logger(ERROR, BRIGHT_RED) << "SSL server error: " << e.what() << std::endl;
      }
    }
  }
  this->m_server_ssl_is_run = false;
}

void HttpServer::acceptLoop() {
  try {
    System::TcpConnection connection;
    bool accepted = false;

    while (!accepted) {
      try {
        connection = m_listener.accept();
        accepted = true;
      }
      catch (System::InterruptedException&) {
        throw;
      }
      catch (std::exception&) {
        // try again
      }
    }

    m_connections.insert(&connection);
    BOOST_SCOPE_EXIT_ALL(this, &connection) {
      m_connections.erase(&connection);
    };

    workingContextGroup.spawn(std::bind(&HttpServer::acceptLoop, this));

    //auto addr = connection.getPeerAddressAndPort();
    auto addr = std::pair<System::Ipv4Address, uint16_t>(static_cast<System::Ipv4Address>(0), 0);
    try {
      addr = connection.getPeerAddressAndPort();
    }
    catch (std::runtime_error&) {
      logger(WARNING) << "Could not get IP of connection";
    }

    logger(DEBUGGING) << "Incoming connection from " << addr.first.toDottedDecimal() << ":" << addr.second;

    System::TcpStreambuf streambuf(connection);
    std::iostream stream(&streambuf);
    HttpParser parser;

    for (;;) {
      HttpRequest req;
      HttpResponse resp;
      resp.addHeader("Access-Control-Allow-Origin", "*");

      parser.receiveRequest(stream, req);
      if (authenticate(req)) {
        processRequest(req, resp);
      }
      else {
        logger(WARNING) << "Authorization required " << addr.first.toDottedDecimal() << ":" << addr.second;
        fillUnauthorizedResponse(resp);
      }

      stream << resp;
      stream.flush();

      if (stream.peek() == std::iostream::traits_type::eof()) {
        break;
      }
    }

    logger(DEBUGGING) << "Closing connection from " << addr.first.toDottedDecimal() << ":" << addr.second << " total=" << m_connections.size();

  }
  catch (System::InterruptedException&) {
  }
  catch (std::exception& e) {
    logger(DEBUGGING) << "Connection error: " << e.what();
  }
}

bool HttpServer::authenticate(const HttpRequest& request) const {
	if (!m_credentials.empty()) {
		auto headerIt = request.getHeaders().find("authorization");
		if (headerIt == request.getHeaders().end()) {
			return false;
		}

		if (headerIt->second.substr(0, 6) != "Basic ") {
			return false;
		}

		if (headerIt->second.substr(6) != m_credentials) {
			return false;
		}
	}

	return true;
}

size_t HttpServer::get_connections_count() const {
	return m_connections.size() + m_server_ssl_clients;
}

}
