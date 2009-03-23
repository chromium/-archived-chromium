// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROCESS_SINGLETON_H_
#define CHROME_BROWSER_PROCESS_SINGLETON_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/basictypes.h"
#include "base/file_path.h"

// ProcessSingleton ----------------------------------------------------------
//
// This class allows different browser processes to communicate with
// each other.  It is named according to the user data directory, so
// we can be sure that no more than one copy of the application can be
// running at once with a given data directory.
//
// Implementation notes:
// - the Windows implementation uses an invisible global message window;
// - the Linux implementation uses a Unix domain socket in the user data dir.

class ProcessSingleton {
 public:
  explicit ProcessSingleton(const FilePath& user_data_dir);
  ~ProcessSingleton();

  // Returns true if another process was found and notified, false if we
  // should continue with this process.
  // Windows code roughly based on Mozilla.
  //
  // TODO(brettw): this will not handle all cases. If two process start up too
  // close to each other, the Create() might not yet have happened for the
  // first one, so this function won't find it.
  bool NotifyOtherProcess();

  // Set ourselves up as the singleton instance.
  void Create();

  // Blocks the dispatch of CopyData messages.
  void Lock() {
    locked_ = true;
  }

  // Allows the dispatch of CopyData messages.
  void Unlock() {
    locked_ = false;
  }

 private:
  bool locked_;

#if defined(OS_WIN)
  // This ugly behemoth handles startup commands sent from another process.
  LRESULT OnCopyData(HWND hwnd, const COPYDATASTRUCT* cds);

  LRESULT CALLBACK WndProc(HWND hwnd,
                           UINT message,
                           WPARAM wparam,
                           LPARAM lparam);

  static LRESULT CALLBACK WndProcStatic(HWND hwnd,
                                        UINT message,
                                        WPARAM wparam,
                                        LPARAM lparam) {
    ProcessSingleton* msg_wnd = reinterpret_cast<ProcessSingleton*>(
        GetWindowLongPtr(hwnd, GWLP_USERDATA));
    return msg_wnd->WndProc(hwnd, message, wparam, lparam);
  }

  HWND remote_window_;  // The HWND_MESSAGE of another browser.
  HWND window_;  // The HWND_MESSAGE window.
#elif defined(OS_LINUX)
  // Set up a socket and sockaddr appropriate for messaging.
  void SetupSocket(int* sock, struct sockaddr_un* addr);

  // Path in file system to the socket.
  FilePath socket_path_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ProcessSingleton);
};

#endif  // #ifndef CHROME_BROWSER_PROCESS_SINGLETON_H_
