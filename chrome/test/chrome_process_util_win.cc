// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chrome_process_util.h"

#include <windows.h>

#include <vector>

#include "base/logging.h"
#include "base/process_util.h"
#include "chrome/common/chrome_constants.h"

base::ProcessId ChromeBrowserProcessId(const FilePath& data_dir) {
  HWND message_window = FindWindowEx(HWND_MESSAGE, NULL,
                                     chrome::kMessageWindowClass,
                                     data_dir.value().c_str());
  if (!message_window)
    return -1;

  DWORD browser_pid = -1;
  GetWindowThreadProcessId(message_window, &browser_pid);

  return browser_pid;
}
