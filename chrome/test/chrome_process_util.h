// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROME_PROCESS_UTIL_H_
#define CHROME_TEST_CHROME_PROCESS_UTIL_H_

#include <vector>

#include "base/file_path.h"
#include "base/process_util.h"

typedef std::vector<base::ProcessId> ChromeProcessList;

// Returns PID of browser process running with user data dir |data_dir|.
// Returns -1 on error.
base::ProcessId ChromeBrowserProcessId(const FilePath& data_dir);

// Returns a vector of PIDs of chrome processes (main and renderers etc)
// associated with user data dir |data_dir|. On error returns empty vector.
ChromeProcessList GetRunningChromeProcesses(const FilePath& data_dir);

// Attempts to terminate all chrome processes associated with |data_dir|.
void TerminateAllChromeProcesses(const FilePath& data_dir);

#endif  // CHROME_TEST_CHROME_PROCESS_UTIL_H_
