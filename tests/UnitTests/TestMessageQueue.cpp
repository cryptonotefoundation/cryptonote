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

#include "gtest/gtest.h"

#include <thread>
#include <iostream>

#include "System/ContextGroup.h"
#include "System/Dispatcher.h"
#include "System/Event.h"

#include "CryptoNoteCore/MessageQueue.h"
#include "CryptoNoteCore/BlockchainMessages.h"
#include "CryptoNoteCore/IntrusiveLinkedList.h"
#include "CryptoNoteCore/CryptoNoteTools.h"

using namespace CryptoNote;

class MessageQueueTest : public testing::Test {
public:
  MessageQueueTest() : contextGroup(dispatcher) {}

  bool addMessageQueue(MessageQueue<BlockchainMessage>&  messageQueue);
  bool removeMessageQueue(MessageQueue<BlockchainMessage>& messageQueue);

  void sendBlockchainMessage(const BlockchainMessage& message);
  void interruptBlockchainMessageWaiting();

  void SetUp() override;
  void TearDown() override;

protected:
  System::Dispatcher dispatcher;
  System::ContextGroup contextGroup;
  IntrusiveLinkedList<MessageQueue<BlockchainMessage>> blockchainMessageQueueList;
};

bool MessageQueueTest::addMessageQueue(MessageQueue<BlockchainMessage>& messageQueue) {
  return blockchainMessageQueueList.insert(messageQueue);
}

bool MessageQueueTest::removeMessageQueue(MessageQueue<BlockchainMessage>& messageQueue) {
  return blockchainMessageQueueList.remove(messageQueue);
}

void MessageQueueTest::sendBlockchainMessage(const BlockchainMessage& message) {
  for (IntrusiveLinkedList<MessageQueue<BlockchainMessage>>::iterator iter = blockchainMessageQueueList.begin(); iter != blockchainMessageQueueList.end(); ++iter) {
    iter->push(message);
  }
}

void MessageQueueTest::interruptBlockchainMessageWaiting() {
  for (IntrusiveLinkedList<MessageQueue<BlockchainMessage>>::iterator iter = blockchainMessageQueueList.begin(); iter != blockchainMessageQueueList.end(); ++iter) {
    iter->stop();
  }
}

void MessageQueueTest::SetUp() {
  ASSERT_TRUE(blockchainMessageQueueList.empty());
}

void MessageQueueTest::TearDown() {
  ASSERT_TRUE(blockchainMessageQueueList.empty());
}

TEST_F(MessageQueueTest, singleNewBlockMessage) {
  MessageQueue<BlockchainMessage> queue(dispatcher);
  MesageQueueGuard<MessageQueueTest, BlockchainMessage> guard(*this, queue);

  Crypto::Hash randomHash;
  for (uint8_t& i : randomHash.data) {
    i = rand();
  }


  contextGroup.spawn([&]() {
    const BlockchainMessage& m = queue.front();
    ASSERT_EQ(m.getType(), BlockchainMessage::MessageType::NEW_BLOCK_MESSAGE);
    Crypto::Hash h;
    ASSERT_TRUE(m.getNewBlockHash(h));
    ASSERT_EQ(h, randomHash);
    ASSERT_NO_THROW(queue.pop());
  });

  ASSERT_NO_THROW(sendBlockchainMessage(BlockchainMessage(NewBlockMessage(randomHash))));

  contextGroup.wait();
}

TEST_F(MessageQueueTest, singleNewAlternativeBlockMessage) {
  MessageQueue<BlockchainMessage> queue(dispatcher);
  MesageQueueGuard<MessageQueueTest, BlockchainMessage> guard(*this, queue);

  Crypto::Hash randomHash;
  for (uint8_t& i : randomHash.data) {
    i = rand();
  }

  contextGroup.spawn([&]() {
    const BlockchainMessage& m = queue.front();
    ASSERT_EQ(m.getType(), BlockchainMessage::MessageType::NEW_ALTERNATIVE_BLOCK_MESSAGE);
    Crypto::Hash h;
    ASSERT_TRUE(m.getNewAlternativeBlockHash(h));
    ASSERT_EQ(h, randomHash);
    ASSERT_NO_THROW(queue.pop());
  });

  ASSERT_NO_THROW(sendBlockchainMessage(BlockchainMessage(NewAlternativeBlockMessage(randomHash))));

  contextGroup.wait();
}

TEST_F(MessageQueueTest, singleChainSwitchMessage) {
  MessageQueue<BlockchainMessage> queue(dispatcher);
  MesageQueueGuard<MessageQueueTest, BlockchainMessage> guard(*this, queue);

  const size_t NUMBER_OF_BLOCKS = 10;
  std::vector<Crypto::Hash> randomHashes;
  for (size_t i = 0; i < NUMBER_OF_BLOCKS; ++i) {
    Crypto::Hash randomHash;
    for (uint8_t& j : randomHash.data) {
      j = rand();
    }
    randomHashes.push_back(randomHash);
  }

  contextGroup.spawn([&]() {
    const BlockchainMessage& m = queue.front();
    ASSERT_EQ(m.getType(), BlockchainMessage::MessageType::CHAIN_SWITCH_MESSAGE);
    std::vector<Crypto::Hash> res;
    ASSERT_TRUE(m.getChainSwitch(res));
    ASSERT_EQ(res, randomHashes);
    ASSERT_NO_THROW(queue.pop());
  });


  std::vector<Crypto::Hash> copy = randomHashes;
  ASSERT_NO_THROW(sendBlockchainMessage(BlockchainMessage(ChainSwitchMessage(std::move(copy)))));

  contextGroup.wait();
}

TEST_F(MessageQueueTest, manyMessagesOneListener) {
  MessageQueue<BlockchainMessage> queue(dispatcher);
  MesageQueueGuard<MessageQueueTest, BlockchainMessage> guard(*this, queue);

  const size_t NUMBER_OF_BLOCKS = 10;
  std::vector<Crypto::Hash> randomHashes;
  for (size_t i = 0; i < NUMBER_OF_BLOCKS; ++i) {
    Crypto::Hash randomHash;
    for (uint8_t& j : randomHash.data) {
      j = rand();
    }
    randomHashes.push_back(randomHash);
  }

  contextGroup.spawn([&]() {
    for (size_t i = 0; i < NUMBER_OF_BLOCKS; ++i) {
      const BlockchainMessage& m = queue.front();
      ASSERT_EQ(m.getType(), BlockchainMessage::MessageType::NEW_BLOCK_MESSAGE);
      Crypto::Hash h;
      ASSERT_TRUE(m.getNewBlockHash(h));
      ASSERT_EQ(h, randomHashes[i]);
      ASSERT_NO_THROW(queue.pop());
    }
  });

  for (auto h : randomHashes) {
    ASSERT_NO_THROW(sendBlockchainMessage(BlockchainMessage(NewBlockMessage(h))));
  }

  contextGroup.wait();
}

TEST_F(MessageQueueTest, manyMessagesManyListeners) {
  const size_t NUMBER_OF_LISTENERS = 5;
  std::array<std::unique_ptr<MessageQueue<BlockchainMessage>>, NUMBER_OF_LISTENERS> queues;
  std::array<std::unique_ptr<MesageQueueGuard<MessageQueueTest, BlockchainMessage>>, NUMBER_OF_LISTENERS> quards;

  for (size_t i = 0; i < NUMBER_OF_LISTENERS; ++i) {
    queues[i] = std::unique_ptr<MessageQueue<BlockchainMessage>>(new MessageQueue<BlockchainMessage>(dispatcher));
    quards[i] = std::unique_ptr<MesageQueueGuard<MessageQueueTest, BlockchainMessage>>(new MesageQueueGuard<MessageQueueTest, BlockchainMessage>(*this, *queues[i]));
  }

  const size_t NUMBER_OF_BLOCKS = 10;
  std::vector<Crypto::Hash> randomHashes;
  for (size_t i = 0; i < NUMBER_OF_BLOCKS; ++i) {
    Crypto::Hash randomHash;
    for (uint8_t& j : randomHash.data) {
      j = rand();
    }
    randomHashes.push_back(randomHash);
  }

  contextGroup.spawn([&]() {
    for (size_t i = 0; i < NUMBER_OF_LISTENERS; ++i) {
      for (size_t j = 0; j < NUMBER_OF_BLOCKS; ++j) {
        const BlockchainMessage& m = queues[i]->front();
        ASSERT_EQ(m.getType(), BlockchainMessage::MessageType::NEW_BLOCK_MESSAGE);
        Crypto::Hash h;
        ASSERT_TRUE(m.getNewBlockHash(h));
        ASSERT_EQ(h, randomHashes[j]);
        ASSERT_NO_THROW(queues[i]->pop());
      }
    }
  });


  for (auto h : randomHashes) {
    ASSERT_NO_THROW(sendBlockchainMessage(BlockchainMessage(NewBlockMessage(h))));
  }

  contextGroup.wait();
}

TEST_F(MessageQueueTest, interruptWaiting) {
  const size_t NUMBER_OF_LISTENERS = 5;
  std::array<std::unique_ptr<MessageQueue<BlockchainMessage>>, NUMBER_OF_LISTENERS> queues;
  std::array<std::unique_ptr<MesageQueueGuard<MessageQueueTest, BlockchainMessage>>, NUMBER_OF_LISTENERS> quards;

  for (size_t i = 0; i < NUMBER_OF_LISTENERS; ++i) {
    queues[i] = std::unique_ptr<MessageQueue<BlockchainMessage>>(new MessageQueue<BlockchainMessage>(dispatcher));
    quards[i] = std::unique_ptr<MesageQueueGuard<MessageQueueTest, BlockchainMessage>>(new MesageQueueGuard<MessageQueueTest, BlockchainMessage>(*this, *queues[i]));
  }

  const size_t NUMBER_OF_BLOCKS = 10;
  std::vector<Crypto::Hash> randomHashes;
  for (size_t i = 0; i < NUMBER_OF_BLOCKS; ++i) {
    Crypto::Hash randomHash;
    for (uint8_t& j : randomHash.data) {
      j = rand();
    }
    randomHashes.push_back(randomHash);
  }

  System::Event shutdownEvent(dispatcher);
  contextGroup.spawn([&]() {
    shutdownEvent.wait();
    for (size_t i = 0; i < NUMBER_OF_LISTENERS; ++i) {
      for (size_t j = 0; j < NUMBER_OF_BLOCKS; ++j) {
        const BlockchainMessage& m = queues[i]->front();
        ASSERT_EQ(m.getType(), BlockchainMessage::MessageType::NEW_BLOCK_MESSAGE);
        Crypto::Hash h;
        ASSERT_TRUE(m.getNewBlockHash(h));
        ASSERT_EQ(h, randomHashes[j]);
        ASSERT_NO_THROW(queues[i]->pop());
      }
    }

    for (size_t i = 0; i < NUMBER_OF_LISTENERS; ++i) {
      for (size_t j = 0; j < NUMBER_OF_BLOCKS; ++j) {
        ASSERT_ANY_THROW(queues[i]->front());
        ASSERT_ANY_THROW(queues[i]->pop());
      }
    }

  });

  for (auto h : randomHashes) {
    ASSERT_NO_THROW(sendBlockchainMessage(BlockchainMessage(NewBlockMessage(h))));
  }

  interruptBlockchainMessageWaiting();

  shutdownEvent.set();

  contextGroup.wait();
}

TEST_F(MessageQueueTest, doubleAddQueueToList) {
  MessageQueue<BlockchainMessage> queue(dispatcher);
  ASSERT_TRUE(blockchainMessageQueueList.insert(queue));
  ASSERT_FALSE(blockchainMessageQueueList.insert(queue));

  ASSERT_TRUE(blockchainMessageQueueList.remove(queue));
  ASSERT_FALSE(blockchainMessageQueueList.remove(queue));
}
