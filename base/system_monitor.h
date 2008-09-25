// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYSTEM_MONITOR_H_
#define BASE_SYSTEM_MONITOR_H_

#include "base/logging.h"
#include "base/observer_list.h"
#include "base/singleton.h"

namespace base {

// Singleton class for monitoring various system-related subsystems
// such as power management, network status, etc.
// TODO(mbelshe):  Add support beyond just power management.
class SystemMonitor {
 public:
  // Access to the Singleton
  static SystemMonitor* Get() {
    return Singleton<SystemMonitor>::get();
  }

  //
  // Power-related APIs
  //

  // Is the computer currently on battery power.
  bool BatteryPower() { return battery_in_use_; }

  // Normalized list of power events.
  enum PowerEvent {
    // The Power status of the system has changed.
    PowerStateEvent,

    // The system is being suspended.
    SuspendEvent,

    // The system is being resumed.
    ResumeEvent
  };

  class PowerObserver {
  public:
    // Notification of a change in power status of the computer, such
    // as from switching between battery and A/C power.
    virtual void OnPowerStateChange(SystemMonitor*) = 0;

    // Notification that the system is suspending.
    virtual void OnSuspend(SystemMonitor*) = 0;

    // Notification that the system is resuming.
    virtual void OnResume(SystemMonitor*) = 0;
  };

  void AddObserver(PowerObserver* obs) {
    observer_list_.AddObserver(obs);
  }

  void RemoveObserver(PowerObserver* obs) {
    observer_list_.RemoveObserver(obs);
  }

  void NotifyPowerStateChange() {
    LOG(INFO) << L"PowerStateChange: " 
             << (BatteryPower() ? L"On" : L"Off") << L" battery";
    FOR_EACH_OBSERVER(PowerObserver, observer_list_,
      OnPowerStateChange(this));
  }

  void NotifySuspend() {
    FOR_EACH_OBSERVER(PowerObserver, observer_list_, OnSuspend(this));
  }

  void NotifyResume() {
    FOR_EACH_OBSERVER(PowerObserver, observer_list_, OnResume(this));
  }

  // Constructor.  
  // Don't use this; access SystemMonitor via the Singleton.
  SystemMonitor();

#if defined(OS_WIN)
  // Windows-specific handling of a WM_POWERBROADCAST message.
  // Embedders of this API should hook their top-level window
  // message loop and forward WM_POWERBROADCAST through this call.
  void ProcessWmPowerBroadcastMessage(int event_id);
#endif

  // Cross-platform handling of a power event.
  // This is only exposed for testing.
  void ProcessPowerMessage(PowerEvent event_id);

 private:
  // Platform-specific method to check whether the system is currently
  // running on battery power.  Returns true if running on batteries,
  // false otherwise.
  bool IsBatteryPower();

  ObserverList<PowerObserver> observer_list_;
  bool battery_in_use_;
  bool suspended_;

  DISALLOW_COPY_AND_ASSIGN(SystemMonitor);
};

}

#endif  // BASE_SYSTEM_MONITOR_H_

