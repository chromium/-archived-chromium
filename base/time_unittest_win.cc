// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <process.h>

#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockTimeTicks : public TimeTicks {
 public:
  static int Ticker() {
    return static_cast<int>(InterlockedIncrement(&ticker_));
  }

  static void InstallTicker() {
    old_tick_function_ = tick_function_;
    tick_function_ = reinterpret_cast<TickFunction>(&Ticker);
    ticker_ = -5;
  }

  static void UninstallTicker() {
    tick_function_ = old_tick_function_;
  }

 private:
  static volatile LONG ticker_;
  static TickFunction old_tick_function_;
};

volatile LONG MockTimeTicks::ticker_;
MockTimeTicks::TickFunction MockTimeTicks::old_tick_function_;

HANDLE g_rollover_test_start;

unsigned __stdcall RolloverTestThreadMain(void* param) {
  int64 counter = reinterpret_cast<int64>(param);
  DWORD rv = WaitForSingleObject(g_rollover_test_start, INFINITE);
  EXPECT_EQ(rv, WAIT_OBJECT_0);

  TimeTicks last = TimeTicks::Now();
  for (int index = 0; index < counter; index++) {
    TimeTicks now = TimeTicks::Now();
    int64 milliseconds = (now - last).InMilliseconds();
    EXPECT_GT(milliseconds, 0);
    EXPECT_LT(milliseconds, 250);
    last = now;
  }
  return 0;
}

}  // namespace

TEST(TimeTicks, WinRollover) {
  // The internal counter rolls over at ~49days.  We'll use a mock
  // timer to test this case.
  // Basic test algorithm:
  //   1) Set clock to rollover - N
  //   2) Create N threads
  //   3) Start the threads
  //   4) Each thread loops through TimeTicks() N times
  //   5) Each thread verifies integrity of result.

  const int kThreads = 8;
  // Use int64 so we can cast into a void* without a compiler warning.
  const int64 kChecks = 10;

  // It takes a lot of iterations to reproduce the bug!
  // (See bug 1081395)
  for (int loop = 0; loop < 4096; loop++) {
    // Setup
    MockTimeTicks::InstallTicker();
    g_rollover_test_start = CreateEvent(0, TRUE, FALSE, 0);
    HANDLE threads[kThreads];

    for (int index = 0; index < kThreads; index++) {
      void* argument = reinterpret_cast<void*>(kChecks);
      unsigned thread_id;
      threads[index] = reinterpret_cast<HANDLE>(
        _beginthreadex(NULL, 0, RolloverTestThreadMain, argument, 0,
          &thread_id));
      EXPECT_NE((HANDLE)NULL, threads[index]);
    }

    // Start!
    SetEvent(g_rollover_test_start);

    // Wait for threads to finish
    for (int index = 0; index < kThreads; index++) {
      DWORD rv = WaitForSingleObject(threads[index], INFINITE);
      EXPECT_EQ(rv, WAIT_OBJECT_0);
    }

    CloseHandle(g_rollover_test_start);

    // Teardown
    MockTimeTicks::UninstallTicker();
  }
}
