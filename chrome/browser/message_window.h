// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MESSAGE_WINDOW_H_
#define CHROME_BROWSER_MESSAGE_WINDOW_H_

#include <string>
#include <windows.h>

#include "base/basictypes.h"
#include "base/file_path.h"

// MessageWindow -------------------------------------------------------------
//
// Class for dealing with the invisible global message window for IPC. This
// window allows different browser processes to communicate with each other.
// It is named according to the user data directory, so we can be sure that
// no more than one copy of the application can be running at once with a
// given data directory.

class MessageWindow {
 public:
  explicit MessageWindow(const FilePath& user_data_dir);
  ~MessageWindow();

  // Returns true if another process was found and notified, false if we
  // should continue with this process. Roughly based on Mozilla
  //
  // TODO(brettw): this will not handle all cases. If two process start up too
  // close to each other, the window might not have been created yet for the
  // first one, so this function won't find it.
  bool NotifyOtherProcess();

  // Create the toplevel message window for IPC.
  void Create();

  // Blocks the dispatch of CopyData messages.
  void Lock() {
    locked_ = true;
  }

  // Allows the dispatch of CopyData messages.
  void Unlock() {
    locked_ = false;
  }

  // This ugly behemoth handles startup commands sent from another process.
  LRESULT OnCopyData(HWND hwnd, const COPYDATASTRUCT* cds);

  // Looks for zombie renderer and plugin processes that could have survived.
  void HuntForZombieChromeProcesses();

 private:
  LRESULT CALLBACK WndProc(HWND hwnd,
                           UINT message,
                           WPARAM wparam,
                           LPARAM lparam);

  static LRESULT CALLBACK WndProcStatic(HWND hwnd,
                                        UINT message,
                                        WPARAM wparam,
                                        LPARAM lparam) {
    MessageWindow* msg_wnd = reinterpret_cast<MessageWindow*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));
    return msg_wnd->WndProc(hwnd, message, wparam, lparam);
  }

  HWND remote_window_;  // The HWND_MESSAGE of another browser.
  HWND window_;  // The HWND_MESSAGE window.
  bool locked_;

  DISALLOW_COPY_AND_ASSIGN(MessageWindow);
};

#endif  // #ifndef CHROME_BROWSER_MESSAGE_WINDOW_H_
