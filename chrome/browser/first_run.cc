// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run.h"

#include "build/build_config.h"

// TODO(port): move more code in back from the first_run_win.cc module.

#if defined(OS_WIN)
#include "chrome/installer/util/install_util.h"
#endif

#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"

namespace {

// The kSentinelFile file absence will tell us it is a first run.
#if defined(OS_WIN)
const char kSentinelFile[] = "First Run";
#else
// On other platforms de intentionally use a different file name, so
// when the remainder of this file is implemented, we can switch to
// the proper file name and users will get the first run interface again.
const char kSentinelFile[] = "First Run Dev";
#endif

// Gives the full path to the sentinel file. The file might not exist.
bool GetFirstRunSentinelFilePath(FilePath* path) {
  FilePath first_run_sentinel;

#if defined(OS_WIN)
  FilePath exe_path;
  if (!PathService::Get(base::DIR_EXE, &exe_path))
    return false;
  if (InstallUtil::IsPerUserInstall(exe_path.value().c_str())) {
    first_run_sentinel = exe_path;
  } else {
    if (!PathService::Get(chrome::DIR_USER_DATA, &first_run_sentinel))
      return false;
  }
#else
  // TODO(port): logic as above.  Not important for our "First Run Dev" file.
  if (!PathService::Get(chrome::DIR_USER_DATA, &first_run_sentinel))
    return false;
#endif

  first_run_sentinel = first_run_sentinel.AppendASCII(kSentinelFile);
  *path = first_run_sentinel;
  return true;
}

}  // namespace

// TODO(port): Mac should share this code.
#if !defined(OS_MACOSX)
bool FirstRun::IsChromeFirstRun() {
  // A troolean, 0 means not yet set, 1 means set to true, 2 set to false.
  static int first_run = 0;
  if (first_run != 0)
    return first_run == 1;

  FilePath first_run_sentinel;
  if (!GetFirstRunSentinelFilePath(&first_run_sentinel) ||
      file_util::PathExists(first_run_sentinel)) {
    first_run = 2;
    return false;
  }
  first_run = 1;
  return true;
}
#endif

bool FirstRun::RemoveSentinel() {
  FilePath first_run_sentinel;
  if (!GetFirstRunSentinelFilePath(&first_run_sentinel))
    return false;
  return file_util::Delete(first_run_sentinel, false);
}

bool FirstRun::CreateSentinel() {
  FilePath first_run_sentinel;
  if (!GetFirstRunSentinelFilePath(&first_run_sentinel))
    return false;
  return file_util::WriteFile(first_run_sentinel, "", 0) != -1;
}
