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

