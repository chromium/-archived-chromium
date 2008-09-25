// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system_monitor.h"

namespace base {

SystemMonitor::SystemMonitor()
    : battery_in_use_(IsBatteryPower()),
      suspended_(false) {
}

void SystemMonitor::ProcessPowerMessage(PowerEvent event_id) {
  // Suppress duplicate notifications.  Some platforms may 
  // send multiple notifications of the same event.
  switch (event_id) {
    case PowerStateEvent:
      {
        bool on_battery = IsBatteryPower();
        if (on_battery != battery_in_use_) {
          battery_in_use_ = on_battery;
          NotifyPowerStateChange();
        }
      }
      break;
    case ResumeEvent:
      if (suspended_) {
        suspended_ = false;
        NotifyResume();
      }
      break;
    case SuspendEvent:
      if (!suspended_) {
        suspended_ = true;
        NotifySuspend();
      }
      break;
  }
}

} // namespace base