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

#include "RandomOuts.h"
#include "TestGenerator.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"

GetRandomOutputs::GetRandomOutputs() {
  REGISTER_CALLBACK_METHOD(GetRandomOutputs, checkHalfUnlocked);
  REGISTER_CALLBACK_METHOD(GetRandomOutputs, checkFullyUnlocked);
}

bool GetRandomOutputs::generate(std::vector<test_event_entry>& events) const {
  TestGenerator generator(*m_currency, events);

  generator.generateBlocks();

  uint64_t sendAmount = MK_COINS(1);

  for (int i = 0; i < 10; ++i) {
    auto builder =
        generator.createTxBuilder(generator.minerAccount, generator.minerAccount, sendAmount, m_currency->minimumFee());

    auto tx = builder.build();
    generator.addEvent(tx);
    generator.makeNextBlock(tx);
  }

  // unlock half of the money
  generator.generateBlocks(m_currency->minedMoneyUnlockWindow() / 2 + 1);
  generator.addCallback("checkHalfUnlocked");

  // unlock the remaining part
  generator.generateBlocks(m_currency->minedMoneyUnlockWindow() / 2);
  generator.addCallback("checkFullyUnlocked");

  return true;
}

bool GetRandomOutputs::request(CryptoNote::Core& c, uint64_t ramount, size_t mixin,
                               CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response& resp) {
  resp.outs.clear();
  CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_request req;

  req.amounts.push_back(ramount);
  req.outs_count = static_cast<uint16_t>(mixin);

  for (auto amount : req.amounts) {
    std::vector<uint32_t> globalIndexes;
    std::vector<Crypto::PublicKey> publicKeys;
    if (!c.getRandomOutputs(amount, static_cast<uint16_t>(req.outs_count), globalIndexes, publicKeys)) {
      return false;
    }

    assert(globalIndexes.size() == publicKeys.size());
    resp.outs.emplace_back(CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_outs_for_amount{amount, {}});
    for (size_t i = 0; i < globalIndexes.size(); ++i) {
      resp.outs.back().outs.push_back({globalIndexes[i], publicKeys[i]});
    }
  }

  return true;
}

#define CHECK(cond)                                                                                                    \
  if ((cond) == false) {                                                                                               \
    LOG_ERROR("Condition " #cond " failed");                                                                           \
    return false;                                                                                                      \
  }

bool GetRandomOutputs::checkHalfUnlocked(CryptoNote::Core& c, size_t ev_index,
                                         const std::vector<test_event_entry>& events) {
  CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response resp;

  auto amount = MK_COINS(1);
  auto unlocked = m_currency->minedMoneyUnlockWindow() / 2 + 1;

  CHECK(request(c, amount, 0, resp));
  CHECK(resp.outs.size() == 1);
  CHECK(resp.outs[0].amount == amount);
  CHECK(resp.outs[0].outs.size() == 0);

  CHECK(request(c, amount, unlocked, resp));
  CHECK(resp.outs.size() == 1);
  CHECK(resp.outs[0].amount == amount);
  CHECK(resp.outs[0].outs.size() == unlocked);

  CHECK(request(c, amount, unlocked * 2, resp));
  CHECK(resp.outs.size() == 1);
  CHECK(resp.outs[0].amount == amount);
  CHECK(resp.outs[0].outs.size() == unlocked);

  return true;
}

bool GetRandomOutputs::checkFullyUnlocked(CryptoNote::Core& c, size_t ev_index,
                                          const std::vector<test_event_entry>& events) {
  CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS_response resp;

  auto amount = MK_COINS(1);
  auto unlocked = m_currency->minedMoneyUnlockWindow() + 1;

  CHECK(request(c, amount, unlocked, resp));
  CHECK(resp.outs.size() == 1);
  CHECK(resp.outs[0].amount == amount);
  CHECK(resp.outs[0].outs.size() == unlocked);

  CHECK(request(c, amount, unlocked * 2, resp));
  CHECK(resp.outs.size() == 1);
  CHECK(resp.outs[0].amount == amount);
  CHECK(resp.outs[0].outs.size() == unlocked);

  return true;
}
