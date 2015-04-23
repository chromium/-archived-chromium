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

#if defined(OS_MACOSX)

// These types and API are here to fetch the information about a set of running
// processes by ID on the Mac.  There are also APIs in base, but fetching the
// information for another process requires privileges that a normal executable
// does not have.  This API fetches the data by spawning ps (which is setuid so
// it has the needed privileges) and processing its output. The API is provided
// here because we don't want code spawning processes like this in base, where
// someone writing cross platform code might use it without realizing that it's
// a heavyweight call on the Mac.

struct MacChromeProcessInfo {
  base::ProcessId pid;
  int rsz_in_kb;
  int vsz_in_kb;
};

typedef std::vector<MacChromeProcessInfo> MacChromeProcessInfoList;

// Any ProcessId that info can't be found for will be left out.
MacChromeProcessInfoList GetRunningMacProcessInfo(
                                        const ChromeProcessList &process_list);

#endif  // defined(OS_MACOSX)

#endif  // CHROME_TEST_CHROME_PROCESS_UTIL_H_
