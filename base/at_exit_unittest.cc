// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Don't test the global AtExitManager, because asking it to process its
// AtExit callbacks can ruin the global state that other tests may depend on.
class ShadowingAtExitManager : public base::AtExitManager {
 public:
  ShadowingAtExitManager() : AtExitManager(true) {}
};

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
  ShadowingAtExitManager shadowing_at_exit_manager;

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
  ShadowingAtExitManager shadowing_at_exit_manager;

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

