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

#include "BlockchainMonitor.h"

#include "Common/StringTools.h"

#include <System/EventLock.h>
#include <System/Timer.h>
#include <System/InterruptedException.h>

#include "Rpc/CoreRpcServerCommandsDefinitions.h"
#include "Rpc/JsonRpc.h"
#include "Rpc/HttpClient.h"

BlockchainMonitor::BlockchainMonitor(System::Dispatcher& dispatcher, const std::string& daemonHost, uint16_t daemonPort, size_t pollingInterval, Logging::ILogger& logger):
  m_dispatcher(dispatcher),
  m_daemonHost(daemonHost),
  m_daemonPort(daemonPort),
  m_pollingInterval(pollingInterval),
  m_stopped(false),
  m_httpEvent(dispatcher),
  m_sleepingContext(dispatcher),
  m_logger(logger, "BlockchainMonitor") {

  m_httpEvent.set();
}

void BlockchainMonitor::waitBlockchainUpdate() {
  m_logger(Logging::DEBUGGING) << "Waiting for blockchain updates";
  m_stopped = false;

  Crypto::Hash lastBlockHash = requestLastBlockHash();

  while(!m_stopped) {
    m_sleepingContext.spawn([this] () {
      System::Timer timer(m_dispatcher);
      timer.sleep(std::chrono::seconds(m_pollingInterval));
    });

    m_sleepingContext.wait();

    if (lastBlockHash != requestLastBlockHash()) {
      m_logger(Logging::DEBUGGING) << "Blockchain has been updated";
      break;
    }
  }

  if (m_stopped) {
    m_logger(Logging::DEBUGGING) << "Blockchain monitor has been stopped";
    throw System::InterruptedException();
  }
}

void BlockchainMonitor::stop() {
  m_logger(Logging::DEBUGGING) << "Sending stop signal to blockchain monitor";
  m_stopped = true;

  m_sleepingContext.interrupt();
  m_sleepingContext.wait();
}

Crypto::Hash BlockchainMonitor::requestLastBlockHash() {
  m_logger(Logging::DEBUGGING) << "Requesting last block hash";

  try {
    CryptoNote::HttpClient client(m_dispatcher, m_daemonHost, m_daemonPort);

    CryptoNote::COMMAND_RPC_GET_LAST_BLOCK_HEADER::request request;
    CryptoNote::COMMAND_RPC_GET_LAST_BLOCK_HEADER::response response;

    System::EventLock lk(m_httpEvent);
    CryptoNote::JsonRpc::invokeJsonRpcCommand(client, "getlastblockheader", request, response);

    if (response.status != CORE_RPC_STATUS_OK) {
      throw std::runtime_error("Core responded with wrong status: " + response.status);
    }

    Crypto::Hash blockHash;
    if (!Common::podFromHex(response.block_header.hash, blockHash)) {
      throw std::runtime_error("Couldn't parse block hash: " + response.block_header.hash);
    }

    m_logger(Logging::DEBUGGING) << "Last block hash: " << Common::podToHex(blockHash);

    return blockHash;
  } catch (std::exception& e) {
    m_logger(Logging::ERROR) << "Failed to request last block hash: " << e.what();
    throw;
  }
}
