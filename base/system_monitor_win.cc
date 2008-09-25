// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system_monitor.h"

namespace base {

void SystemMonitor::ProcessWmPowerBroadcastMessage(int event_id) {
  PowerEvent power_event;
  switch (event_id) {
    case PBT_APMPOWERSTATUSCHANGE:
      power_event = PowerStateEvent;
      break;
    case PBT_APMRESUMEAUTOMATIC:
      power_event = ResumeEvent;
      break;
    case PBT_APMSUSPEND:
      power_event = SuspendEvent;
      break;
    default:
      DCHECK(false);
  }
  ProcessPowerMessage(power_event);
}

// Function to query the system to see if it is currently running on
// battery power.  Returns true if running on battery.
bool SystemMonitor::IsBatteryPower() {
  SYSTEM_POWER_STATUS status;
  if (!GetSystemPowerStatus(&status)) {
    LOG(ERROR) << "GetSystemPowerStatus failed: " << GetLastError();
    return false;
  }
  return (status.ACLineStatus == 0);
}



} // namespace base