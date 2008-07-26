// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#define _CRT_SECURE_NO_WARNINGS

#include "testing/gtest/include/gtest/gtest.h"
#include "base/process_util.h"

TEST(ProcessUtilTest, EnableLFH) {
  ASSERT_TRUE(process_util::EnableLowFragmentationHeap());
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

TEST(ProcessUtilTest, CalcFreeMemory) {
  process_util::ProcessMetrics* metrics =
    process_util::ProcessMetrics::CreateProcessMetrics(::GetCurrentProcess());
  ASSERT_TRUE(NULL != metrics);

  // Typical values here is ~1900 for total and ~1000 for largest. Obviously
  // it depends in what other tests have done to this process.
  process_util::FreeMBytes free_mem1 = {0};
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

  process_util::FreeMBytes free_mem2 = {0};
  EXPECT_TRUE(metrics->CalculateFreeMemory(&free_mem2));
  EXPECT_GE(free_mem2.total, free_mem2.largest);
  EXPECT_GE(expected_total, free_mem2.total);
  EXPECT_GE(expected_largest, free_mem2.largest);
  EXPECT_TRUE(NULL != free_mem2.largest_ptr);

  delete[] alloc;
  delete metrics;
}
