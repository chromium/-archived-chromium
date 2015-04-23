// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HUNG_WINDOW_DETECTOR_H__
#define CHROME_BROWSER_HUNG_WINDOW_DETECTOR_H__

#include "base/lock.h"
#include "chrome/common/worker_thread_ticker.h"

// This class provides the following functionality:
// Given a top-level window handle, it enumerates all descendant windows
// of that window and, on finding a window that belongs to a different
// thread from that of the top-level window, it tests to see if that window
// is responding to messages. It does this test by first calling the
// IsHungAppWindow API and, additionally (since the IsHungAppWindow API
// does not deal correctly with suspended threads), send a dummy message
// (WM_NULL) to the window and verifies that the call does not timeout.
// This class is typically used in conjunction with the WorkerThreadTicker
// class so that the checking can happen on a periodic basis.
// If a hung window is detected it calls back the specified implementation of
// the HungWindowNotification interface. Currently this class only supports
// a single callback but it can be extended later to support multiple
// callbacks.
class HungWindowDetector : public WorkerThreadTicker::Callback {
 public:
  // This property specifies the message timeout for a child window.
  static const wchar_t kHungChildWindowTimeout[];
  // This is the notification callback interface that is used to notify
  // callers about a non-responsive window
  class HungWindowNotification  {
   public:
    enum ActionOnHungWindow {
      HUNG_WINDOW_IGNORE,
      HUNG_WINDOW_TERMINATE_THREAD,
      HUNG_WINDOW_TERMINATE_PROCESS,
    };

    // This callback method is invoked when a hung window is detected.
    // A return value of false indicates that we should stop enumerating the
    // child windows of the browser window to check if they are hung.
    virtual bool OnHungWindowDetected(HWND hung_window, HWND top_level_window,
                                      ActionOnHungWindow* action) = 0;
  };

  // The notification object is not owned by this class. It is assumed that
  // this pointer will be valid throughout the lifetime of this class.
  // Ownership of this pointer is not transferred to this class.
  // Note that the Initialize method needs to be called to initiate monitoring
  // of hung windows.
  HungWindowDetector(HungWindowNotification* notification);
  ~HungWindowDetector();

  // This method initialized the monitoring of hung windows. All descendant
  // windows of the passed-in top-level window which belong to a thread
  // different from that of the top-level window are monitored. The
  // message_response_timeout parameter indicates how long this class must
  // wait for a window to respond to a sent message before it is considered
  // to be non-responsive.
  // Initialize can be called multiple times to change the actual window to
  // be monitored as well as the message response timeout
  bool Initialize(HWND top_level_window,
                  int message_response_timeout);

  // Implementation of the WorkerThreadTicker::Callback interface
  virtual void OnTick();

 private:
  // Helper function that checks whether the specified child window is hung.
  // If so, it invokes the HungWindowNotification interface implementation
  bool CheckChildWindow(HWND child_window);

  static BOOL CALLBACK ChildWndEnumProc(HWND child_window, LPARAM param);

  // Pointer to the HungWindowNotification callback interface. This class does
  // not RefCount this pointer and it is assumed that the pointer will be valid
  // throughout the lifetime of this class.
  HungWindowNotification* notification_;
  HWND top_level_window_;

  // How long do we wait before we consider a window hung (in ms)
  int message_response_timeout_;
  Lock hang_detection_lock_;
  // Indicates if this object is currently enumerating hung windows
  bool enumerating_;

  DISALLOW_EVIL_CONSTRUCTORS(HungWindowDetector);
};


#endif  // CHROME_BROWSER_HUNG_WINDOW_DETECTOR_H__
