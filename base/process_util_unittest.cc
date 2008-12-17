// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _CRT_SECURE_NO_WARNINGS

#include "base/multiprocess_test.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_LINUX)
#include <dlfcn.h>
#endif

namespace base {

class ProcessUtilTest : public MultiProcessTest {
};

MULTIPROCESS_TEST_MAIN(SimpleChildProcess) {
  return 0;
}

TEST_F(ProcessUtilTest, SpawnChild) {
  ProcessHandle handle = this->SpawnChild(L"SimpleChildProcess");

  ASSERT_NE(static_cast<ProcessHandle>(NULL), handle);
  EXPECT_TRUE(WaitForSingleProcess(handle, 1000));
}

MULTIPROCESS_TEST_MAIN(SlowChildProcess) {
  // Sleep until file "SlowChildProcess.die" is created.
  FILE *fp;
  do {
    PlatformThread::Sleep(100);
    fp = fopen("SlowChildProcess.die", "r");
  } while (!fp);
  fclose(fp);
  remove("SlowChildProcess.die");
  exit(0);
  return 0;
}

#if defined(OS_WIN)
#define EXE_SUFFIX L".exe"
#else
#define EXE_SUFFIX L""
#endif

// TODO(port): finish port on Mac
#if !defined(OS_MACOSX)
TEST_F(ProcessUtilTest, KillSlowChild) {
  remove("SlowChildProcess.die");
  int oldcount = GetProcessCount(L"base_unittests" EXE_SUFFIX, 0);

  ProcessHandle handle = this->SpawnChild(L"SlowChildProcess");

  ASSERT_NE(static_cast<ProcessHandle>(NULL), handle);
  EXPECT_EQ(oldcount+1, GetProcessCount(L"base_unittests" EXE_SUFFIX, 0));
  FILE *fp = fopen("SlowChildProcess.die", "w");
  fclose(fp);
  // TODO(port): do something less racy here
  PlatformThread::Sleep(1000);
  EXPECT_EQ(oldcount, GetProcessCount(L"base_unittests" EXE_SUFFIX, 0));
}
#endif

// TODO(estade): if possible, port these 2 tests.
#if defined(OS_WIN)
TEST_F(ProcessUtilTest, EnableLFH) {
  ASSERT_TRUE(EnableLowFragmentationHeap());
  if (IsDebuggerPresent()) {
    // Under these conditions, LFH can't be enabled. There's no point to test
    // anything.
    const char* no_debug_env = getenv("_NO_DEBUG_HEAP");
    if (!no_debug_env || strcmp(no_debug_env, "1"))
      return;
  }
  HANDLE heaps[1024] = { 0 };
  unsigned number_heaps = GetProcessHeaps(1024, heaps);
  EXPECT_GT(number_heaps, 0u);
  for (unsigned i = 0; i < number_heaps; ++i) {
    ULONG flag = 0;
    SIZE_T length;
    ASSERT_NE(0, HeapQueryInformation(heaps[i],
                                      HeapCompatibilityInformation,
                                      &flag,
                                      sizeof(flag),
                                      &length));
    // If flag is 0, the heap is a standard heap that does not support
    // look-asides. If flag is 1, the heap supports look-asides. If flag is 2,
    // the heap is a low-fragmentation heap (LFH). Note that look-asides are not
    // supported on the LFH.

    // We don't have any documented way of querying the HEAP_NO_SERIALIZE flag.
    EXPECT_LE(flag, 2u);
    EXPECT_NE(flag, 1u);
  }
}

TEST_F(ProcessUtilTest, CalcFreeMemory) {
  ProcessMetrics* metrics =
      ProcessMetrics::CreateProcessMetrics(::GetCurrentProcess());
  ASSERT_TRUE(NULL != metrics);

  // Typical values here is ~1900 for total and ~1000 for largest. Obviously
  // it depends in what other tests have done to this process.
  FreeMBytes free_mem1 = {0};
  EXPECT_TRUE(metrics->CalculateFreeMemory(&free_mem1));
  EXPECT_LT(10u, free_mem1.total);
  EXPECT_LT(10u, free_mem1.largest);
  EXPECT_GT(2048u, free_mem1.total);
  EXPECT_GT(2048u, free_mem1.largest);
  EXPECT_GE(free_mem1.total, free_mem1.largest);
  EXPECT_TRUE(NULL != free_mem1.largest_ptr);

  // Allocate 20M and check again. It should have gone down.
  const int kAllocMB = 20;
  char* alloc = new char[kAllocMB * 1024 * 1024];
  EXPECT_TRUE(NULL != alloc);

  size_t expected_total = free_mem1.total - kAllocMB;
  size_t expected_largest = free_mem1.largest;

  FreeMBytes free_mem2 = {0};
  EXPECT_TRUE(metrics->CalculateFreeMemory(&free_mem2));
  EXPECT_GE(free_mem2.total, free_mem2.largest);
  EXPECT_GE(expected_total, free_mem2.total);
  EXPECT_GE(expected_largest, free_mem2.largest);
  EXPECT_TRUE(NULL != free_mem2.largest_ptr);

  delete[] alloc;
  delete metrics;
}
#endif  // defined(OS_WIN)

}  // namespace base
