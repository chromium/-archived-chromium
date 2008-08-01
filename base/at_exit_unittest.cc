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

#include "base/at_exit.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

int g_test_counter_1 = 0;
int g_test_counter_2 = 0;

void IncrementTestCounter1() {
  ++g_test_counter_1;
}

void IncrementTestCounter2() {
  ++g_test_counter_2;
}

void ZeroTestCounters() {
  g_test_counter_1 = 0;
  g_test_counter_2 = 0;
}

void ExpectCounter1IsZero() {
  EXPECT_EQ(0, g_test_counter_1);
}

}  // namespace

TEST(AtExitTest, Basic) {
  ZeroTestCounters();
  base::AtExitManager::RegisterCallback(&IncrementTestCounter1);
  base::AtExitManager::RegisterCallback(&IncrementTestCounter2);
  base::AtExitManager::RegisterCallback(&IncrementTestCounter1);
  
  EXPECT_EQ(0, g_test_counter_1);
  EXPECT_EQ(0, g_test_counter_2);
  base::AtExitManager::ProcessCallbacksNow();
  EXPECT_EQ(2, g_test_counter_1);
  EXPECT_EQ(1, g_test_counter_2);
}

TEST(AtExitTest, LIFOOrder) {
  ZeroTestCounters();
  base::AtExitManager::RegisterCallback(&IncrementTestCounter1);
  base::AtExitManager::RegisterCallback(&ExpectCounter1IsZero);
  base::AtExitManager::RegisterCallback(&IncrementTestCounter2);
  
  EXPECT_EQ(0, g_test_counter_1);
  EXPECT_EQ(0, g_test_counter_2);
  base::AtExitManager::ProcessCallbacksNow();
  EXPECT_EQ(1, g_test_counter_1);
  EXPECT_EQ(1, g_test_counter_2);
}
