// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system_monitor.h"
#include "testing/gtest/include/gtest/gtest.h"

class PowerTest : public base::SystemMonitor::PowerObserver {
 public:
  PowerTest()
    : battery_(false),
      power_state_changes_(0),
      suspends_(0),
      resumes_(0) {};

  // PowerObserver callbacks.
  void OnPowerStateChange(base::SystemMonitor*) { power_state_changes_++; };
  void OnSuspend(base::SystemMonitor*) { suspends_++; };
  void OnResume(base::SystemMonitor*) { resumes_++; };

  // Test status counts.
  bool battery() { return battery_; }
  int power_state_changes() { return power_state_changes_; }
  int suspends() { return suspends_; }
  int resumes() { return resumes_; }

 private:
  bool battery_;   // Do we currently think we're on battery power.
  int power_state_changes_;  // Count of OnPowerStateChange notifications.
  int suspends_;  // Count of OnSuspend notifications.
  int resumes_;  // Count of OnResume notifications.
};

TEST(SystemMonitor, PowerNotifications) {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  PowerTest test;
  monitor->AddObserver(&test);

  // Send a bunch of power changes.  Since the battery power hasn't
  // actually changed, we shouldn't get notifications.
  for (int index = 0; index < 5; index++) {
    monitor->ProcessPowerMessage(base::SystemMonitor::PowerStateEvent);
    EXPECT_EQ(test.power_state_changes(), 0);
  }

  // Sending resume when not suspended should have no effect.
  monitor->ProcessPowerMessage(base::SystemMonitor::ResumeEvent);
  EXPECT_EQ(test.resumes(), 0);

  // Pretend we suspended.
  monitor->ProcessPowerMessage(base::SystemMonitor::SuspendEvent);
  EXPECT_EQ(test.suspends(), 1);

  // Send a second suspend notification.  This should be suppressed.
  monitor->ProcessPowerMessage(base::SystemMonitor::SuspendEvent);
  EXPECT_EQ(test.suspends(), 1);

  // Pretend we were awakened.
  monitor->ProcessPowerMessage(base::SystemMonitor::ResumeEvent);
  EXPECT_EQ(test.resumes(), 1);

  // Send a duplicate resume notification.  This should be suppressed.
  monitor->ProcessPowerMessage(base::SystemMonitor::ResumeEvent);
  EXPECT_EQ(test.resumes(), 1);
}

