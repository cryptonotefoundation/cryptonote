// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
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

#include "BaseTests.h"

#include <System/Timer.h>
#include "WalletLegacy/WalletLegacy.h"
#include "WalletLegacyObserver.h"
#include <Logging/LoggerRef.h>

using namespace Tests;
using namespace CryptoNote;
using namespace Logging;

class WalletLegacyTests : public BaseTest {

};


TEST_F(WalletLegacyTests, checkNetworkShutdown) {
  auto networkCfg = TestNetworkBuilder(3, Topology::Star).
    setBlockchain("testnet_300").build();

  networkCfg[0].nodeType = NodeType::InProcess;
  network.addNodes(networkCfg);
  network.waitNodesReady();

  auto& daemon = network.getNode(0);

  {
    auto node = daemon.makeINode();
	
    WalletLegacy wallet(currency, *node, logger);
    wallet.initAndGenerate("pass");

    WalletLegacyObserver observer;
    wallet.addObserver(&observer);

    std::error_code syncResult;
    ASSERT_TRUE(observer.m_syncResult.waitFor(std::chrono::seconds(10), syncResult));
    ASSERT_TRUE(!syncResult);

    // sync completed
    auto syncProgress = observer.getSyncProgress();

    network.getNode(1).stopDaemon();
    network.getNode(2).stopDaemon();

    System::Timer(dispatcher).sleep(std::chrono::seconds(10));

    // check that sync progress was not updated
    ASSERT_EQ(syncProgress, observer.getSyncProgress()); 
  }
}
