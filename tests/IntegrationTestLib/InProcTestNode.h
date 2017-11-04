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

#include "TestNode.h"
#include "NetworkConfiguration.h"

#include <future>
#include <memory>
#include <thread>

#include <System/Dispatcher.h>


namespace CryptoNote {
class Core;
class CryptoNoteProtocolHandler;
class NodeServer;
class Currency;
}

namespace Tests {

class InProcTestNode : public TestNode {
public:
  InProcTestNode(const TestNodeConfiguration& cfg, const CryptoNote::Currency& currency, System::Dispatcher& d);
  ~InProcTestNode();

  virtual bool startMining(size_t threadsCount, const std::string &address) override;
  virtual bool stopMining() override;
  virtual bool stopDaemon() override;
  virtual bool getBlockTemplate(const std::string &minerAddress, CryptoNote::BlockTemplate &blockTemplate, uint64_t &difficulty) override;
  virtual bool submitBlock(const std::string& block) override;
  virtual bool getTailBlockId(Crypto::Hash &tailBlockId) override;
  virtual bool makeINode(std::unique_ptr<CryptoNote::INode>& node) override;
  virtual uint64_t getLocalHeight() override;

private:

  void workerThread(std::promise<std::string>& initPromise);

  System::Dispatcher& dispatcher;
  std::unique_ptr<CryptoNote::Core> core;
  std::unique_ptr<CryptoNote::CryptoNoteProtocolHandler> protocol;
  std::unique_ptr<CryptoNote::NodeServer> p2pNode;

  std::thread m_thread;
  const CryptoNote::Currency& m_currency;
  TestNodeConfiguration m_cfg;
};

}
