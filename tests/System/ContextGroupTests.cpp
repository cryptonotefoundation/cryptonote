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

#include <System/Context.h>
#include <System/ContextGroup.h>
#include <System/Dispatcher.h>
#include <System/Event.h>
#include <System/InterruptedException.h>
#include <System/Ipv4Address.h>
#include <System/Timer.h>
#include <System/TcpConnection.h>
#include <System/TcpConnector.h>
#include <System/TcpListener.h>
#include <gtest/gtest.h>

#include <thread>

using namespace System;

TEST(ContextGroupTests, testHangingUp) {
  Dispatcher dispatcher;
  Event e(dispatcher);
  Context<> context(dispatcher, [&] {
    Timer(dispatcher).sleep(std::chrono::milliseconds(100));
  });

  Context<> contextSecond(dispatcher, [&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    e.set();
    dispatcher.yield();
  });

  e.wait();
}

TEST(ContextGroupTests, ContextGroupWaitIsWaiting) {
  Dispatcher dispatcher;

  bool contextFinished = false;
  ContextGroup cg1(dispatcher);
  cg1.spawn([&] {
    dispatcher.yield();
    contextFinished = true;
  });

  cg1.wait();
  ASSERT_TRUE(contextFinished);
}

TEST(ContextGroupTests, ContextGroupInterruptIsInterrupting) {
  Dispatcher dispatcher;

  bool interrupted = false;
  ContextGroup cg1(dispatcher);
  cg1.spawn([&] {
    interrupted = dispatcher.interrupted();
  });

  cg1.interrupt();
  cg1.wait();

  ASSERT_TRUE(interrupted);
} 

TEST(ContextGroupTests, ContextGroupDestructorIsInterrupt_Waitable) {
  Dispatcher dispatcher;

  bool interrupted = false;
  bool contextFinished = false;
  {
    ContextGroup cg1(dispatcher);
    cg1.spawn([&] {
      interrupted = dispatcher.interrupted();
      Timer(dispatcher).sleep(std::chrono::milliseconds(100));
      contextFinished = true;
    });
  }

  ASSERT_TRUE(interrupted);
  ASSERT_TRUE(contextFinished);
}

TEST(ContextGroupTests, TimerIsContextIntrerruptible) {
  Dispatcher dispatcher;
  
  bool interrupted = false;
  {
    ContextGroup cg1(dispatcher);
    cg1.spawn([&] {
      try {
        Timer(dispatcher).sleep(std::chrono::milliseconds(1000));
      } catch (InterruptedException&) {
        interrupted = true;
      }
    });

    dispatcher.yield();
  }

  ASSERT_TRUE(interrupted);
}

TEST(ContextGroupTests, ListenerAcceptIsContextIntrerruptible) {
  Dispatcher dispatcher;

  bool interrupted = false;
  {
    ContextGroup cg1(dispatcher);
    cg1.spawn([&] {
      try {
        TcpListener(dispatcher, Ipv4Address("0.0.0.0"), 12345).accept();
      } catch (InterruptedException&) {
        interrupted = true;
      }
    });

    dispatcher.yield();
  }

  ASSERT_TRUE(interrupted);
}

TEST(ContextGroupTests, ConnectorConnectIsContextIntrerruptible) {
  Dispatcher dispatcher;

  bool interrupted = false;
  {
    ContextGroup cg1(dispatcher);
    cg1.spawn([&] {
      try {
        TcpConnector(dispatcher).connect(Ipv4Address("127.0.0.1"), 12345);
      } catch (InterruptedException&) {
        interrupted = true;
      }
    });

    dispatcher.yield();
  }
  ASSERT_TRUE(interrupted);
}

TEST(ContextGroupTests, ConnectionReadIsContextIntrerruptible) {
  Dispatcher dispatcher;

  bool interrupted = false;
  {
    Event connected(dispatcher);
    ContextGroup cg1(dispatcher);
    cg1.spawn([&] {
      try {
        auto conn = TcpListener(dispatcher, Ipv4Address("0.0.0.0"), 12345).accept();
        Timer(dispatcher).sleep(std::chrono::milliseconds(1000));
        conn.write(nullptr, 0);
      } catch (InterruptedException&) {
      }
    });

    cg1.spawn([&] {
      try {
        auto conn = TcpConnector(dispatcher).connect(Ipv4Address("127.0.0.1"), 12345);
        connected.set();
        uint8_t a[10];
        conn.read(a, 10);
        conn.write(nullptr, 0);
      } catch (InterruptedException&) {
        interrupted = true;
      }
    });

    connected.wait();
    dispatcher.yield();
  }

  ASSERT_TRUE(interrupted);
}

TEST(ContextGroupTests, TimerSleepIsThrowingWhenCurrentContextIsInterrupted) {
  Dispatcher dispatcher;

  bool interrupted = false;
  {
    ContextGroup cg1(dispatcher);
    cg1.spawn([&] {
      try {
        Timer(dispatcher).sleep(std::chrono::milliseconds(1000));
      } catch (InterruptedException&) {
        interrupted = true;
      }
    });
  }

  ASSERT_TRUE(interrupted);
}

TEST(ContextGroupTests, ListenerAcceptIsThrowingWhenCurrentContextIsInterrupted) {
  Dispatcher dispatcher;

  bool interrupted = false;
  {
    ContextGroup cg1(dispatcher);
    cg1.spawn([&] {
      try {
        TcpListener(dispatcher, Ipv4Address("0.0.0.0"), 12345).accept();
      } catch (InterruptedException&) {
        interrupted = true;
      }
    });
  }

  ASSERT_TRUE(interrupted);
}

TEST(ContextGroupTests, ConnectorConnectIsThrowingWhenCurrentContextIsInterrupted) {
  Dispatcher dispatcher;

  bool interrupted = false;
  {
    ContextGroup cg1(dispatcher);
    cg1.spawn([&] {
      try {
        TcpConnector(dispatcher).connect(Ipv4Address("127.0.0.1"), 12345);
      } catch (InterruptedException&) {
        interrupted = true;
      }
    });
  }

  ASSERT_TRUE(interrupted);
}

TEST(ContextGroupTests, ConnectionReadIsThrowingWhenCurrentContextIsInterrupted) {
  Dispatcher dispatcher;

  bool interrupted = false;
  {
    Event connected(dispatcher);
    ContextGroup cg1(dispatcher);
    cg1.spawn([&] {
      try {
        auto conn = TcpListener(dispatcher, Ipv4Address("0.0.0.0"), 12345).accept();
        conn.write(nullptr, 0);
      } catch (InterruptedException&) {
      }
    });

    cg1.spawn([&] {
      try {
        auto conn = TcpConnector(dispatcher).connect(Ipv4Address("127.0.0.1"), 12345);
        connected.set();
        dispatcher.yield();
        uint8_t a[10];
        conn.read(a, 10);
        conn.write(nullptr, 0);
      } catch (InterruptedException&) {
        interrupted = true;
      }
    });

    connected.wait();
  }

  ASSERT_TRUE(interrupted);
}

TEST(ContextGroupTests, ConnectionWriteIsThrowingWhenCurrentContextIsInterrupted) {
  Dispatcher dispatcher;

  bool interrupted = false;
  {
    Event connected(dispatcher);
    ContextGroup cg1(dispatcher);
    cg1.spawn([&] {
      try {
        auto conn = TcpListener(dispatcher, Ipv4Address("0.0.0.0"), 12345).accept();
        conn.write(nullptr, 0);
      } catch (InterruptedException&) {
      }
    });

    cg1.spawn([&] {
      try {
        auto conn = TcpConnector(dispatcher).connect(Ipv4Address("127.0.0.1"), 12345);
        connected.set();
        dispatcher.yield();
        conn.write(nullptr, 0);
      } catch (InterruptedException&) {
        interrupted = true;
      }
    });

    connected.wait();
  }

  ASSERT_TRUE(interrupted);
}

TEST(ContextGroupTests, DispatcherInterruptIsInterrupting) {
  bool interrupted = false;
  {
    Dispatcher dispatcher;
    Context<> context(dispatcher, [&] {
      try {
        Timer(dispatcher).sleep(std::chrono::milliseconds(1000));
      } catch (InterruptedException&) {
        interrupted = true;
      }
    });

    dispatcher.yield();
  }

  ASSERT_TRUE(interrupted);
}

TEST(ContextGroupTests, DispatcherInterruptSetsFlag) {
  Dispatcher dispatcher;
  Context<> context(dispatcher, [&] {
    try {
      Timer(dispatcher).sleep(std::chrono::milliseconds(10));
    } catch (InterruptedException&) {
    }
  });

  dispatcher.interrupt();
  dispatcher.yield();
  ASSERT_TRUE(dispatcher.interrupted());
  ASSERT_FALSE(dispatcher.interrupted());
}

TEST(ContextGroupTests, ContextGroupIsWaitingIncludigNestedSpawns) {
  Dispatcher dispatcher;

  bool contextFinished = false;
  bool nestedContextFinished = false;

  ContextGroup cg1(dispatcher);
  cg1.spawn([&] {
    try {
      cg1.spawn([&] {
        try {
          Timer(dispatcher).sleep(std::chrono::milliseconds(100));
          nestedContextFinished = true;
        } catch (InterruptedException&) {
        }
      });

      Timer(dispatcher).sleep(std::chrono::milliseconds(100));
      contextFinished = true;
    } catch (InterruptedException&) {
    }
  });

  cg1.wait();

  ASSERT_TRUE(contextFinished);
  ASSERT_TRUE(nestedContextFinished);
}

TEST(ContextGroupTests, ContextGroupIsWaitingNestedSpawnsEvenThoughItWasInterrupted) {
  Dispatcher dispatcher;

  bool contextFinished = false;
  bool nestedContextFinished = false;

  {
    ContextGroup cg1(dispatcher);
    cg1.spawn([&] {
      try {
        Timer(dispatcher).sleep(std::chrono::milliseconds(100));
        contextFinished = true;
      } catch (InterruptedException&) {
        cg1.spawn([&] {
          try {
            Timer(dispatcher).sleep(std::chrono::milliseconds(100));
            nestedContextFinished = true;
          } catch (InterruptedException&) {
          }
        });
      }
    });

    dispatcher.yield();
  }

  ASSERT_FALSE(contextFinished);
  ASSERT_TRUE(nestedContextFinished);
}
