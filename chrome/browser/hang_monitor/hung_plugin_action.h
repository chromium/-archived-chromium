// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HANG_MONITOR_HUNG_WINDOW_DETECTOR_H__
#define CHROME_BROWSER_HANG_MONITOR_HUNG_WINDOW_DETECTOR_H__

#include "chrome/browser/hang_monitor/hung_window_detector.h"
// This class provides an implementation the
// HungWindowDetector::HungWindowNotification callback interface.
// It checks to see if the hung window belongs to a process different
// from that of the browser process and, if so, it returns an action
// of HungWindowNotification::HUNG_WINDOW_TERMINATE_PROCESS.
// Note: We can write other action classes that implement the same
// interface and switch the action done on hung plugins based on user
// preferences.
class HungPluginAction : public HungWindowDetector::HungWindowNotification {
 public:
  HungPluginAction();
  ~HungPluginAction();
  // HungWindowNotification implementation
  virtual bool OnHungWindowDetected(HWND hung_window,
                                    HWND top_level_window,
                                    ActionOnHungWindow* action);

 protected:
  void OnWindowResponsive(HWND window);

  // The callback function for the SendMessageCallback API
  static void CALLBACK HungWindowResponseCallback(HWND target_window,
                                                  UINT message,
                                                  ULONG_PTR data,
                                                  LRESULT result);

  static BOOL CALLBACK DismissMessageBox(HWND window, LPARAM ignore);

 protected:
  bool GetPluginName(HWND plugin_window,
                     DWORD browser_process_id,
                     std::wstring *plugin_name);
  // The currently hung plugin window that we are prompting the user about
  HWND current_hung_plugin_window_;
};

#endif  // CHROME_BROWSER_HANG_MONITOR_HUNG_WINDOW_DETECTOR_H__
