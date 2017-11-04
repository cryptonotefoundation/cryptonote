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

#include <gtest/gtest.h>
#include "Common/BlockingQueue.h"

#include <future>
#include <vector>
#include <numeric>
#include <memory>

class ParallelProcessor {
public:

  ParallelProcessor(size_t threads)
    : m_threads(threads) {}

  template <typename F>
  void spawn(F f) {
    for (auto& t : m_threads) {
      t = std::thread(f);
    }
  }

  void join() {
    for (auto& t : m_threads) {
      t.join();
    }
  }

private:

  std::vector<std::thread> m_threads;
  
};

// single producer, many consumers
void TestQueue_SPMC(unsigned iterations, unsigned threadCount, unsigned queueSize) {

  BlockingQueue<int> bq(queueSize);

  ParallelProcessor processor(threadCount);
  std::atomic<int64_t> result(0);

  processor.spawn([&bq, &result]{
      int v = 0;
      int64_t sum = 0;

      while (bq.pop(v)) {
        sum += v;
      }
      
      result += sum;
      // std::cout << "Sum: " << sum << std::endl;
    });

  int64_t expectedSum = 0;

  for (unsigned i = 0; i < iterations; ++i) {
    expectedSum += i;
    ASSERT_TRUE(bq.push(i));
  }

  bq.close();
  processor.join();

  ASSERT_EQ(expectedSum, result.load());
}

void TestQueue_MPSC(unsigned iterations, unsigned threadCount, unsigned queueSize) {

  BlockingQueue<int> bq(queueSize);

  ParallelProcessor processor(threadCount);
  std::atomic<unsigned> counter(0);
  std::atomic<int64_t> pushed(0);

  processor.spawn([&]{
    int64_t sum = 0;

    for(;;) {
      unsigned value = counter.fetch_add(1);
      if (value >= iterations)
        break;

      bq.push(value);
      sum += value;
    }

    pushed += sum;
    // std::cout << "Sum: " << sum << std::endl;
  });

  int64_t expectedSum = 0;

  for (unsigned i = 0; i < iterations; ++i) {
    int value;
    ASSERT_TRUE(bq.pop(value));
    expectedSum += i;
  }

  ASSERT_EQ(0, bq.size());

  processor.join();

  ASSERT_EQ(expectedSum, pushed);
}


TEST(BlockingQueue, SPMC)
{
  TestQueue_SPMC(10000, 1, 1);
  TestQueue_SPMC(10000, 4, 1);
  TestQueue_SPMC(10000, 16, 16);
  TestQueue_SPMC(10000, 16, 100);
}

TEST(BlockingQueue, MPSC)
{
  TestQueue_MPSC(10000, 1, 1);
  TestQueue_MPSC(10000, 4, 1);
  TestQueue_MPSC(10000, 16, 16);
  TestQueue_MPSC(10000, 16, 100);
}


TEST(BlockingQueue, PerfTest)
{
  // TestQueue_SPMC(1000000, 32, 1);
}

TEST(BlockingQueue, Close)
{
  BlockingQueue<int> bq(4);
  ParallelProcessor p(4);

  p.spawn([&bq] {
    int v;
    while (bq.pop(v))
      ;
  });

  bq.push(10); // enqueue 1 item

  bq.close(); // all threads should unblock and finish
  p.join(); 
}

TEST(BlockingQueue, CloseAndWait)
{
  size_t queueSize = 100;
  BlockingQueue<int> bq(queueSize);
  ParallelProcessor p(4);

  std::atomic<size_t> itemsPopped(0);

  // fill the queue
  for (int i = 0; i < queueSize; ++i)
    bq.push(i); 

  p.spawn([&bq, &itemsPopped] {
    int v;
    while (bq.pop(v)) {
      itemsPopped += 1;
      // some delay to make close() really wait
      std::this_thread::sleep_for(std::chrono::milliseconds(10)); 
    }
  });


  // check with multiple closing
  auto f1 = std::async(std::launch::async, [&] { bq.close(true); });
  auto f2 = std::async(std::launch::async, [&] { bq.close(true); });

  bq.close(true);

  f1.get();
  f2.get();

  p.join();

  ASSERT_EQ(queueSize, itemsPopped.load());
}

TEST(BlockingQueue, AllowsMoveOnly)
{
  BlockingQueue<std::unique_ptr<int>> bq(1);

  std::unique_ptr<int> v(new int(100));
  ASSERT_TRUE(bq.push(std::move(v)));

  std::unique_ptr<int> popval;
  bq.pop(popval);

  ASSERT_EQ(*popval, 100);
}
