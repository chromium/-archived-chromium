// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "chrome/common/chrome_process_filter.h"

#include "base/path_service.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"

BrowserProcessFilter::BrowserProcessFilter(const std::wstring user_data_dir)
    : browser_process_id_(0),
      user_data_dir_(user_data_dir) {
  // Find the message window (if any) for the current user data directory,
  // and get its process ID.  We'll only count browser processes that either
  // have the same process ID or have that process ID as their parent.

  if (user_data_dir_.length() == 0)
    PathService::Get(chrome::DIR_USER_DATA, &user_data_dir_);


  HWND message_window = FindWindowEx(HWND_MESSAGE, NULL,
                                     chrome::kMessageWindowClass,
                                     user_data_dir_.c_str());
  if (message_window)
    GetWindowThreadProcessId(message_window, &browser_process_id_);
}

bool BrowserProcessFilter::Includes(uint32 pid,
                                    uint32 parent_pid) const {
  return browser_process_id_ && (browser_process_id_ == pid ||
                                 browser_process_id_ == parent_pid);
}
