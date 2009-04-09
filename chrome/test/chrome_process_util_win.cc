// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chrome_process_util.h"

#include <windows.h>

#include <vector>

#include "base/logging.h"
#include "base/process_util.h"
#include "chrome/common/chrome_constants.h"

namespace {

class ChromeProcessFilter : public base::ProcessFilter {
 public:
  explicit ChromeProcessFilter(base::ProcessId browser_pid)
      : browser_pid_(browser_pid) {}

  virtual bool Includes(base::ProcessId pid, base::ProcessId parent_pid) const {
    // Match browser process itself and its children.
    return browser_pid_ == pid || browser_pid_ == parent_pid;
  }

 private:
  base::ProcessId browser_pid_;

  DISALLOW_COPY_AND_ASSIGN(ChromeProcessFilter);
};

}  // namespace

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

ChromeProcessList GetRunningChromeProcesses(const FilePath& data_dir) {
  ChromeProcessList result;

  base::ProcessId browser_pid = ChromeBrowserProcessId(data_dir);
  if (browser_pid < 0)
    return result;

  ChromeProcessFilter filter(browser_pid);
  base::NamedProcessIterator it(chrome::kBrowserProcessExecutableName, &filter);

  const ProcessEntry* process_entry;
  while (process_entry = it.NextProcessEntry())
    result.push_back(process_entry->th32ProcessID);

  return result;
}
