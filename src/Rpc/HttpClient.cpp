// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2020, The Karbo developers
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

#include "HttpClient.h"

#include <HTTP/HttpParser.h>
#include <System/Ipv4Resolver.h>
#include <System/Ipv4Address.h>
#include <System/TcpConnector.h>
#include <System/SocketStream.h>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/stream.hpp>
#ifdef _WIN32
#include <wincrypt.h>
#endif
#if defined(__ANDROID__)
#include "HttpRootCerts.h"
#endif

using boost::asio::ip::tcp;


// TODO: must be native
void getHostName(const std::string url, std::string &hostname) {
  size_t sep_pos = url.find("/");
  if (sep_pos == std::string::npos) hostname = url;
  else hostname = url.substr(sep_pos);
}

#ifdef _WIN32
void add_windows_root_certs(boost::asio::ssl::context &ctx) {
  HCERTSTORE hStore = CertOpenSystemStore(0, "ROOT");
  if (hStore != NULL) {
    X509_STORE *store = X509_STORE_new();
    PCCERT_CONTEXT pContext = NULL;
    while ((pContext = CertEnumCertificatesInStore(hStore, pContext)) != NULL) {
      X509 *x509 = d2i_X509(NULL,
                            (const unsigned char **) &pContext->pbCertEncoded,
                            pContext->cbCertEncoded);
      if (x509 != NULL) {
        X509_STORE_add_cert(store, x509);
        X509_free(x509);
      }
    }
    CertFreeCertificateContext(pContext);
    CertCloseStore(hStore, 0);
    SSL_CTX_set_cert_store(ctx.native_handle(), store);
  }
}

// Fix LNK2001: unresolved external symbol ___iob_func in VS2017
#define stdin  (__acrt_iob_func(0))
#define stdout (__acrt_iob_func(1))
#define stderr (__acrt_iob_func(2))

FILE _iob[] = { *stdin, *stdout, *stderr };
extern "C" FILE * __cdecl __iob_func(void) { return _iob; }

#endif

#if defined(__ANDROID__)
void add_emb_root_certs(boost::asio::ssl::context &ctx) {
  std::string emb_certs = (char *) root_crts;
  size_t cert_start = 0;
  size_t cert_end = 0;
  while (true) {
    cert_end = emb_certs.find("\n\n", cert_start + 2);
    if (cert_end != std::string::npos) {
      ctx.add_certificate_authority(boost::asio::buffer(emb_certs.data() + cert_start, cert_end - cert_start));
      cert_start = cert_end;
    } else {
      break;
    }
  }
}
#endif

#if defined(_WIN32)
void sockSetup(SOCKET &sock) {
  const int32_t rw_timeout = 60000;
  const unsigned long enable_keep_alive = 1;
  setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (const char *) &enable_keep_alive, sizeof(enable_keep_alive));
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *) &rw_timeout, sizeof(rw_timeout));
  setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *) &rw_timeout, sizeof(rw_timeout));
}
#endif

namespace CryptoNote {

HttpClient::HttpClient(System::Dispatcher& dispatcher, const std::string& address, uint16_t port, bool ssl_enable) :
  m_dispatcher(dispatcher), m_address(address), m_port(port), m_ssl_enable(ssl_enable), m_ssl_cert(""), m_ssl_no_verify(false) {
}

HttpClient::~HttpClient() {
  if (m_connected) {
    disconnect();
  }
}

void HttpClient::setRootCert(const std::string &path) {
  if (this->m_ssl_cert.empty()) this->m_ssl_cert = path;
}

void HttpClient::disableVerify() {
  if (!this->m_ssl_no_verify) this->m_ssl_no_verify = true;
}

void HttpClient::request(HttpRequest &req, HttpResponse &res) {
  if (!m_connected) {
    connect();
  }
  req.setHost(m_address);
  if (this->m_ssl_enable) {
    try {
      System::SocketStreambuf streambuf((char *) "", 1);
      std::iostream stream(&streambuf);
      HttpParser parser;
      stream << req;
      stream.flush();
      std::vector<uint8_t> req_data;
      std::vector<uint8_t> resp_data;
      streambuf.getRespdata(req_data);
      size_t req_data_size = (size_t) req_data.size();
      size_t write_size = 0;
      size_t write_full_size = 0;
      while (write_full_size < req_data_size) {
        write_size = this->m_ssl_sock->write_some(boost::asio::buffer(req_data.data() + write_full_size,
                                                  req_data_size - write_full_size));
        if (write_size > 0) {
          write_full_size += write_size;
        } else {
          break;
        }
      }
      size_t resp_size = 0;
      size_t resp_size_full = 0;
      const size_t resp_buff_size = 1024;
      char resp_buff[resp_buff_size];
      const char *header_end_sep = "\r\n\r\n";
      const char *content_lenght_name = "Content-Length";
      const char *content_lenght_end_sep = "\r\n";
      size_t header_end = 0;
      size_t stream_len = 0;
      bool header_found = false;
      while (true) {
        memset(resp_buff, 0x00, sizeof(char) * resp_buff_size);
        resp_size = this->m_ssl_sock->read_some(boost::asio::buffer((char *) resp_buff,
                                                resp_buff_size));
        resp_size_full += resp_size;
        if (resp_size > 0) {
          resp_data.resize(resp_size_full);
          memcpy(resp_data.data() + resp_size_full - resp_size, resp_buff, resp_size);
          if (!header_found) {
            std::string data = std::string((char *) resp_data.data());
            data.push_back(0x00);
            size_t header_end = data.find(header_end_sep);
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
          if (header_found) {
            if (stream_len > 0) {
              if (resp_size_full >= stream_len) break;
            } else {
              if (resp_size_full == header_end + 4) break;
            }
          }
        } else {
          break;
        }
      }
      streambuf.setRespdata(resp_data);
      parser.receiveResponse(stream, res);
    } catch (const std::exception &) {
      disconnect();
      throw;
    }
  } else {
    try {
      std::iostream stream(m_streamBuf.get());
      HttpParser parser;
      stream << req;
      stream.flush();
      parser.receiveResponse(stream, res);
    } catch (const std::exception &) {
      disconnect();
      throw;
    }
  }
}

void HttpClient::connect() {
  std::string hostname;
  getHostName(this->m_address, hostname);
  if (this->m_ssl_enable) {
    try {
      boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
      if (this->m_ssl_cert.empty()) {
#if defined(_WIN32)
        add_windows_root_certs(ctx);
#elif defined(__ANDROID__)
        add_emb_root_certs(ctx);
#else
        ctx.set_default_verify_paths();
#endif
      } else {
        ctx.load_verify_file(m_ssl_cert);
      }
      this->m_ssl_sock.reset(new boost::asio::ssl::stream<tcp::socket> (this->m_io_service, std::ref(ctx)));
      tcp::resolver resolver(this->m_io_service);
      tcp::resolver::query query(hostname, std::to_string(this->m_port));
      boost::asio::connect(this->m_ssl_sock->lowest_layer(), resolver.resolve(query));
#if defined(_WIN32)
      sockSetup((SOCKET &) this->m_ssl_sock->lowest_layer().native_handle());
#endif
      this->m_ssl_sock->lowest_layer().set_option(tcp::no_delay(true));
      this->m_ssl_sock->lowest_layer().set_option(boost::asio::socket_base::keep_alive(true));
      if (!this->m_ssl_no_verify) {
        this->m_ssl_sock->set_verify_mode(boost::asio::ssl::verify_peer);
      } else {
        this->m_ssl_sock->set_verify_mode(boost::asio::ssl::verify_none);
      }
      this->m_ssl_sock->set_verify_callback(boost::asio::ssl::rfc2818_verification(hostname));
      this->m_ssl_sock->handshake(boost::asio::ssl::stream_base::client);
      m_connected = true;
    } catch (const std::exception& e) {
      throw ConnectException(e.what());
    }
  } else {
    try {
      auto ipAddr = System::Ipv4Resolver(m_dispatcher).resolve(hostname);
      m_connection = System::TcpConnector(m_dispatcher).connect(ipAddr, m_port);
      m_streamBuf.reset(new System::TcpStreambuf(m_connection));
      m_connected = true;
    } catch (const std::exception& e) {
      throw ConnectException(e.what());
    }
  }
}

bool HttpClient::isConnected() const {
  return m_connected;
}

void HttpClient::disconnect() {
  if (this->m_ssl_enable) {
    try {
      this->m_ssl_sock->lowest_layer().close();
    } catch (std::exception&) {
      //Ignoring possible exception.
    }
    this->m_ssl_sock.reset();
  } else {
    m_streamBuf.reset();
    try {
      m_connection.write(nullptr, 0); //Socket shutdown.
    } catch (std::exception&) {
      //Ignoring possible exception.
    }
    try {
      m_connection = System::TcpConnection();
    } catch (std::exception&) {
      //Ignoring possible exception.
    }
  }
  m_connected = false;
}

ConnectException::ConnectException(const std::string& whatArg) : std::runtime_error(whatArg.c_str()) {
}

}
