// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/multiprocess_test.h"
#include "base/platform_thread.h"
#include "base/stats_table.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <process.h>
#include <windows.h>
#endif

using base::TimeTicks;

namespace {
class StatsTableTest : public MultiProcessTest {
};

// Open a StatsTable and verify that we can write to each of the
// locations in the table.
TEST_F(StatsTableTest, VerifySlots) {
  const std::wstring kTableName = L"VerifySlotsStatTable";
  const int kMaxThreads = 1;
  const int kMaxCounter = 5;
  StatsTable table(kTableName, kMaxThreads, kMaxCounter);

  // Register a single thread.
  std::wstring thread_name = L"mainThread";
  int slot_id = table.RegisterThread(thread_name);
  EXPECT_TRUE(slot_id);

  // Fill up the table with counters.
  std::wstring counter_base_name = L"counter";
  for (int index=0; index < kMaxCounter; index++) {
    std::wstring counter_name = counter_base_name;
    StringAppendF(&counter_name, L"counter.ctr%d", index);
    int counter_id = table.FindCounter(counter_name);
    EXPECT_GT(counter_id, 0);
  }

  // Try to allocate an additional thread.  Verify it fails.
  slot_id = table.RegisterThread(L"too many threads");
  EXPECT_EQ(slot_id, 0);

  // Try to allocate an additional counter.  Verify it fails.
  int counter_id = table.FindCounter(counter_base_name);
  EXPECT_EQ(counter_id, 0);
}

// CounterZero will continually be set to 0.
const std::wstring kCounterZero = L"CounterZero";
// Counter1313 will continually be set to 1313.
const std::wstring kCounter1313 = L"Counter1313";
// CounterIncrement will be incremented each time.
const std::wstring kCounterIncrement = L"CounterIncrement";
// CounterDecrement will be decremented each time.
const std::wstring kCounterDecrement = L"CounterDecrement";
// CounterMixed will be incremented by odd numbered threads and
// decremented by even threads.
const std::wstring kCounterMixed = L"CounterMixed";
// The number of thread loops that we will do.
const int kThreadLoops = 1000;

// TODO(estade): port this test
#if defined(OS_WIN)
unsigned __stdcall StatsTableMultipleThreadMain(void* param) {
  // Each thread will open the shared memory and set counters
  // concurrently in a loop.  We'll use some pauses to
  // mixup the thread scheduling.
  int16 id = reinterpret_cast<int16>(param);

  StatsCounter zero_counter(kCounterZero);
  StatsCounter lucky13_counter(kCounter1313);
  StatsCounter increment_counter(kCounterIncrement);
  StatsCounter decrement_counter(kCounterDecrement);
  for (int index = 0; index < kThreadLoops; index++) {
    StatsCounter mixed_counter(kCounterMixed);  // create this one in the loop
    zero_counter.Set(0);
    lucky13_counter.Set(1313);
    increment_counter.Increment();
    decrement_counter.Decrement();
    if (id % 2)
      mixed_counter.Decrement();
    else
      mixed_counter.Increment();
    PlatformThread::Sleep(index % 10);   // short wait
  }
  return 0;
}

// Create a few threads and have them poke on their counters.
TEST_F(StatsTableTest, MultipleThreads) {
  // Create a stats table.
  const std::wstring kTableName = L"MultipleThreadStatTable";
  const int kMaxThreads = 20;
  const int kMaxCounter = 5;
  StatsTable table(kTableName, kMaxThreads, kMaxCounter);
  StatsTable::set_current(&table);

  EXPECT_EQ(0, table.CountThreadsRegistered());

  // Spin up a set of threads to go bang on the various counters.
  // After we join the threads, we'll make sure the counters
  // contain the values we expected.
  HANDLE threads[kMaxThreads];

  // Spawn the threads.
  for (int16 index = 0; index < kMaxThreads; index++) {
    void* argument = reinterpret_cast<void*>(index);
    unsigned thread_id;
    threads[index] = reinterpret_cast<HANDLE>(
      _beginthreadex(NULL, 0, StatsTableMultipleThreadMain, argument, 0,
        &thread_id));
    EXPECT_NE((HANDLE)NULL, threads[index]);
  }

  // Wait for the threads to finish.
  for (int index = 0; index < kMaxThreads; index++) {
    DWORD rv = WaitForSingleObject(threads[index], 60 * 1000);
    EXPECT_EQ(rv, WAIT_OBJECT_0);  // verify all threads finished
  }
  StatsCounter zero_counter(kCounterZero);
  StatsCounter lucky13_counter(kCounter1313);
  StatsCounter increment_counter(kCounterIncrement);
  StatsCounter decrement_counter(kCounterDecrement);
  StatsCounter mixed_counter(kCounterMixed);

  // Verify the various counters are correct.
  std::wstring name;
  name = L"c:" + kCounterZero;
  EXPECT_EQ(0, table.GetCounterValue(name));
  name = L"c:" + kCounter1313;
  EXPECT_EQ(1313 * kMaxThreads,
      table.GetCounterValue(name));
  name = L"c:" + kCounterIncrement;
  EXPECT_EQ(kMaxThreads * kThreadLoops,
      table.GetCounterValue(name));
  name = L"c:" + kCounterDecrement;
  EXPECT_EQ(-kMaxThreads * kThreadLoops,
      table.GetCounterValue(name));
  name = L"c:" + kCounterMixed;
  EXPECT_EQ((kMaxThreads % 2) * kThreadLoops,
      table.GetCounterValue(name));
  EXPECT_EQ(0, table.CountThreadsRegistered());
}
#endif  // defined(OS_WIN)

const std::wstring kTableName = L"MultipleProcessStatTable";

extern "C" int DYNAMIC_EXPORT StatsTableMultipleProcessMain() {
  // Each process will open the shared memory and set counters
  // concurrently in a loop.  We'll use some pauses to
  // mixup the scheduling.

  StatsTable table(kTableName, 0, 0);
  StatsTable::set_current(&table);
  StatsCounter zero_counter(kCounterZero);
  StatsCounter lucky13_counter(kCounter1313);
  StatsCounter increment_counter(kCounterIncrement);
  StatsCounter decrement_counter(kCounterDecrement);
  for (int index = 0; index < kThreadLoops; index++) {
    zero_counter.Set(0);
    lucky13_counter.Set(1313);
    increment_counter.Increment();
    decrement_counter.Decrement();
    PlatformThread::Sleep(index % 10);   // short wait
  }
  return 0;
}

// Create a few processes and have them poke on their counters.
TEST_F(StatsTableTest, MultipleProcesses) {
  // Create a stats table.
  const std::wstring kTableName = L"MultipleProcessStatTable";
  const int kMaxProcs = 20;
  const int kMaxCounter = 5;
  StatsTable table(kTableName, kMaxProcs, kMaxCounter);
  StatsTable::set_current(&table);

  EXPECT_EQ(0, table.CountThreadsRegistered());

  // Spin up a set of processes to go bang on the various counters.
  // After we join the processes, we'll make sure the counters
  // contain the values we expected.
  ProcessHandle procs[kMaxProcs];

  // Spawn the processes.
  for (int16 index = 0; index < kMaxProcs; index++) {
    procs[index] = this->SpawnChild(L"StatsTableMultipleProcessMain");
    EXPECT_NE(static_cast<ProcessHandle>(NULL), procs[index]);
  }

  // Wait for the processes to finish.
  for (int index = 0; index < kMaxProcs; index++) {
    EXPECT_TRUE(process_util::WaitForSingleProcess(procs[index], 60 * 1000));
  }

  StatsCounter zero_counter(kCounterZero);
  StatsCounter lucky13_counter(kCounter1313);
  StatsCounter increment_counter(kCounterIncrement);
  StatsCounter decrement_counter(kCounterDecrement);

  // Verify the various counters are correct.
  std::wstring name;
  name = L"c:" + kCounterZero;
  EXPECT_EQ(0, table.GetCounterValue(name));
  name = L"c:" + kCounter1313;
  EXPECT_EQ(1313 * kMaxProcs,
      table.GetCounterValue(name));
  name = L"c:" + kCounterIncrement;
  EXPECT_EQ(kMaxProcs * kThreadLoops,
      table.GetCounterValue(name));
  name = L"c:" + kCounterDecrement;
  EXPECT_EQ(-kMaxProcs * kThreadLoops,
      table.GetCounterValue(name));
  EXPECT_EQ(0, table.CountThreadsRegistered());
}

class MockStatsCounter : public StatsCounter {
 public:
  MockStatsCounter(const std::wstring& name)
      : StatsCounter(name) {}
  int* Pointer() { return GetPtr(); }
};

// Test some basic StatsCounter operations
TEST_F(StatsTableTest, StatsCounter) {
  // Create a stats table.
  const std::wstring kTableName = L"StatTable";
  const int kMaxThreads = 20;
  const int kMaxCounter = 5;
  StatsTable table(kTableName, kMaxThreads, kMaxCounter);
  StatsTable::set_current(&table);

  MockStatsCounter foo(L"foo");

  // Test initial state.
  EXPECT_TRUE(foo.Enabled());
  EXPECT_NE(foo.Pointer(), static_cast<int*>(0));
  EXPECT_EQ(0, table.GetCounterValue(L"c:foo"));
  EXPECT_EQ(0, *(foo.Pointer()));

  // Test Increment.
  while(*(foo.Pointer()) < 123) foo.Increment();
  EXPECT_EQ(123, table.GetCounterValue(L"c:foo"));
  foo.Add(0);
  EXPECT_EQ(123, table.GetCounterValue(L"c:foo"));
  foo.Add(-1);
  EXPECT_EQ(122, table.GetCounterValue(L"c:foo"));

  // Test Set.
  foo.Set(0);
  EXPECT_EQ(0, table.GetCounterValue(L"c:foo"));
  foo.Set(100);
  EXPECT_EQ(100, table.GetCounterValue(L"c:foo"));
  foo.Set(-1);
  EXPECT_EQ(-1, table.GetCounterValue(L"c:foo"));
  foo.Set(0);
  EXPECT_EQ(0, table.GetCounterValue(L"c:foo"));

  // Test Decrement.
  foo.Decrement(1);
  EXPECT_EQ(-1, table.GetCounterValue(L"c:foo"));
  foo.Decrement(0);
  EXPECT_EQ(-1, table.GetCounterValue(L"c:foo"));
  foo.Decrement(-1);
  EXPECT_EQ(0, table.GetCounterValue(L"c:foo"));
}

class MockStatsCounterTimer : public StatsCounterTimer {
 public:
  MockStatsCounterTimer(const std::wstring& name)
      : StatsCounterTimer(name) {}

  TimeTicks start_time() { return start_time_; }
  TimeTicks stop_time() { return stop_time_; }
};

// Test some basic StatsCounterTimer operations
TEST_F(StatsTableTest, StatsCounterTimer) {
  // Create a stats table.
  const std::wstring kTableName = L"StatTable";
  const int kMaxThreads = 20;
  const int kMaxCounter = 5;
  StatsTable table(kTableName, kMaxThreads, kMaxCounter);
  StatsTable::set_current(&table);

  MockStatsCounterTimer bar(L"bar");

  // Test initial state.
  EXPECT_FALSE(bar.Running());
  EXPECT_TRUE(bar.start_time().is_null());
  EXPECT_TRUE(bar.stop_time().is_null());

  // Do some timing.
  bar.Start();
  PlatformThread::Sleep(500);
  bar.Stop();
  EXPECT_LE(500, table.GetCounterValue(L"t:bar"));

  // Verify that timing again is additive.
  bar.Start();
  PlatformThread::Sleep(500);
  bar.Stop();
  EXPECT_LE(1000, table.GetCounterValue(L"t:bar"));
}

// Test some basic StatsRate operations
TEST_F(StatsTableTest, StatsRate) {
  // Create a stats table.
  const std::wstring kTableName = L"StatTable";
  const int kMaxThreads = 20;
  const int kMaxCounter = 5;
  StatsTable table(kTableName, kMaxThreads, kMaxCounter);
  StatsTable::set_current(&table);

  StatsRate baz(L"baz");

  // Test initial state.
  EXPECT_FALSE(baz.Running());
  EXPECT_EQ(0, table.GetCounterValue(L"c:baz"));
  EXPECT_EQ(0, table.GetCounterValue(L"t:baz"));

  // Do some timing.
  baz.Start();
  PlatformThread::Sleep(500);
  baz.Stop();
  EXPECT_EQ(1, table.GetCounterValue(L"c:baz"));
  EXPECT_LE(500, table.GetCounterValue(L"t:baz"));

  // Verify that timing again is additive.
  baz.Start();
  PlatformThread::Sleep(500);
  baz.Stop();
  EXPECT_EQ(2, table.GetCounterValue(L"c:baz"));
  EXPECT_LE(1000, table.GetCounterValue(L"t:baz"));
}

// Test some basic StatsScope operations
TEST_F(StatsTableTest, StatsScope) {
  // Create a stats table.
  const std::wstring kTableName = L"StatTable";
  const int kMaxThreads = 20;
  const int kMaxCounter = 5;
  StatsTable table(kTableName, kMaxThreads, kMaxCounter);
  StatsTable::set_current(&table);

  StatsCounterTimer foo(L"foo");
  StatsRate bar(L"bar");

  // Test initial state.
  EXPECT_EQ(0, table.GetCounterValue(L"t:foo"));
  EXPECT_EQ(0, table.GetCounterValue(L"t:bar"));
  EXPECT_EQ(0, table.GetCounterValue(L"c:bar"));

  // Try a scope.
  {
    StatsScope<StatsCounterTimer> timer(foo);
    StatsScope<StatsRate> timer2(bar);
    PlatformThread::Sleep(500);
  }
  EXPECT_LE(500, table.GetCounterValue(L"t:foo"));
  EXPECT_LE(500, table.GetCounterValue(L"t:bar"));
  EXPECT_EQ(1, table.GetCounterValue(L"c:bar"));

  // Try a second scope.
  {
    StatsScope<StatsCounterTimer> timer(foo);
    StatsScope<StatsRate> timer2(bar);
    PlatformThread::Sleep(500);
  }
  EXPECT_LE(1000, table.GetCounterValue(L"t:foo"));
  EXPECT_LE(1000, table.GetCounterValue(L"t:bar"));
  EXPECT_EQ(2, table.GetCounterValue(L"c:bar"));
}

}  // namespace
