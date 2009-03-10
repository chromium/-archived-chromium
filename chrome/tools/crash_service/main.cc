// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/tools/crash_service/crash_service.h"

#include <windows.h>
#include <stdlib.h>
#include <tchar.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"

namespace {

const wchar_t kStandardLogFile[] = L"operation_log.txt";

bool GetCrashServiceDirectory(std::wstring* dir) {
  std::wstring temp_dir;
  if (!file_util::GetTempDir(&temp_dir))
    return false;
  file_util::AppendToPath(&temp_dir, L"chrome_crashes");
  if (!file_util::PathExists(temp_dir)) {
    if (!file_util::CreateDirectory(temp_dir))
      return false;
  }
  *dir = temp_dir;
  return true;
}

}  // namespace.

int __stdcall wWinMain(HINSTANCE instance, HINSTANCE, wchar_t* cmd_line,
                       int show_mode) {
  // Manages the destruction of singletons.
  base::AtExitManager exit_manager;

  CommandLine::Init(0, NULL);

  // We use/create a directory under the user's temp folder, for logging.
  std::wstring operating_dir;
  GetCrashServiceDirectory(&operating_dir);
  std::wstring log_file(operating_dir);
  file_util::AppendToPath(&log_file, kStandardLogFile);

  // Logging to a file with pid, tid and timestamp.
  logging::InitLogging(log_file.c_str(), logging::LOG_ONLY_TO_FILE,
                       logging::LOCK_LOG_FILE, logging::APPEND_TO_OLD_LOG_FILE);
  logging::SetLogItems(true, true, true, false);

  LOG(INFO) << "session start. cmdline is [" << cmd_line << "]";

  CrashService crash_service(operating_dir);
  if (!crash_service.Initialize(::GetCommandLineW()))
    return 1;

  LOG(INFO) << "ready to process crash requests";

  // Enter the message loop.
  int retv = crash_service.ProcessingLoop();
  // Time to exit.
  LOG(INFO) << "session end. return code is " << retv;
  return retv;
}
