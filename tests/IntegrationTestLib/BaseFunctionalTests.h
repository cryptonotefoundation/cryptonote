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

#include <list>
#include <vector>
#include <memory>
#include <mutex>              
#include <condition_variable> 
#include <queue>

#include <boost/noncopyable.hpp>
#include <boost/program_options.hpp>

#include <System/Dispatcher.h>
#include <Logging/ConsoleLogger.h>

#include "CryptoNoteCore/Currency.h"
#include "IWalletLegacy.h"
#include "INode.h"
#include "TestNode.h"
#include "NetworkConfiguration.h"

namespace Tests {
  namespace Common {

  namespace po = boost::program_options;
    class Semaphore{
    private:
      std::mutex mtx;
      std::condition_variable cv;
      bool available;

    public:
      Semaphore() : available(false) { }

      void notify() {
        std::unique_lock<std::mutex> lck(mtx);
        available = true;
        cv.notify_one();
      }

      void wait() {
        std::unique_lock<std::mutex> lck(mtx);
        cv.wait(lck, [this](){ return available; });
        available = false;
      }

      bool wait_for(const std::chrono::milliseconds& rel_time) {
        std::unique_lock<std::mutex> lck(mtx);
        auto result = cv.wait_for(lck, rel_time, [this](){ return available; });
        available = false;
        return result;
      }
    };

    const uint16_t P2P_FIRST_PORT = 9000;
    const uint16_t RPC_FIRST_PORT = 9200;


    class BaseFunctionalTestsConfig {
    public:
      BaseFunctionalTestsConfig() {}

      void init(po::options_description& desc) {
        desc.add_options()
          ("daemon-dir,d", po::value<std::string>()->default_value("."), "path to bytecoind.exe")
          ("data-dir,n", po::value<std::string>()->default_value("."), "path to daemon's data directory")
          ("add-daemons,a", po::value<std::vector<std::string>>()->multitoken(), "add daemon to topology");
      }

      bool handleCommandLine(const po::variables_map& vm) {
        if (vm.count("daemon-dir")) {
          daemonDir = vm["daemon-dir"].as<std::string>();
        }

        if (vm.count("data-dir")) {
          dataDir = vm["data-dir"].as<std::string>();
        }

        if (vm.count("add-daemons")) {
          daemons = vm["add-daemons"].as<std::vector<std::string>>();
        }

        return true;
      }

      std::string daemonDir;
      std::string dataDir;
      std::vector<std::string> daemons;
    };



    class BaseFunctionalTests : boost::noncopyable {
    public:
      BaseFunctionalTests(const CryptoNote::Currency& currency, System::Dispatcher& d, const BaseFunctionalTestsConfig& config) :
          m_dispatcher(d),
          m_currency(currency),
          m_nextTimestamp(time(nullptr) - 365 * 24 * 60 * 60),
          m_config(config),
          m_dataDir(config.dataDir),
          m_daemonDir(config.daemonDir),
          m_testnetSize(1) {
        if (m_dataDir.empty()) {
          m_dataDir = ".";
        }
        if (m_daemonDir.empty()) {
          m_daemonDir = ".";
        }
      };

      ~BaseFunctionalTests();

      enum Topology {
        Ring,
        Line,
        Star
      };

    protected:

      TestNodeConfiguration createNodeConfiguration(size_t i);

      std::vector< std::unique_ptr<TestNode> > nodeDaemons;
      System::Dispatcher& m_dispatcher;
      const CryptoNote::Currency& m_currency;

      void launchTestnet(size_t count, Topology t = Line);
      void launchTestnetWithInprocNode(size_t count, Topology t = Line);
      void launchInprocTestnet(size_t count, Topology t = Line);
      void stopTestnet();

      void startNode(size_t index);
      void stopNode(size_t index);

      bool makeWallet(std::unique_ptr<CryptoNote::IWalletLegacy> & wallet, std::unique_ptr<CryptoNote::INode>& node, const std::string& password = "pass");
      bool mineBlocks(TestNode& node, const CryptoNote::AccountPublicAddress& address, size_t blockCount);
      bool mineBlock(std::unique_ptr<CryptoNote::IWalletLegacy>& wallet);
      bool mineBlock();
      bool startMining(size_t threads);
      bool stopMining();

      bool getNodeTransactionPool(size_t nodeIndex, CryptoNote::INode& node, std::vector<std::unique_ptr<CryptoNote::ITransactionReader>>& txPool);

      bool waitDaemonsReady();
      bool waitDaemonReady(size_t nodeIndex);
      bool waitForPeerCount(CryptoNote::INode& node, size_t expectedPeerCount);
      bool waitForPoolSize(size_t nodeIndex, CryptoNote::INode& node, size_t expectedPoolSize,
        std::vector<std::unique_ptr<CryptoNote::ITransactionReader>>& txPool);

      bool prepareAndSubmitBlock(TestNode& node, CryptoNote::BlockTemplate&& blockTemplate);

#ifdef __linux__
      std::vector<__pid_t> pids;
#endif

      Logging::ConsoleLogger logger;
      std::unique_ptr<CryptoNote::INode> mainNode;
      std::unique_ptr<CryptoNote::IWalletLegacy> workingWallet;
      uint64_t m_nextTimestamp;
      Topology m_topology;
      size_t m_testnetSize;

      BaseFunctionalTestsConfig m_config;
      std::string m_dataDir;
      std::string m_daemonDir;
      uint16_t m_mainDaemonRPCPort;
    };
  }
}
