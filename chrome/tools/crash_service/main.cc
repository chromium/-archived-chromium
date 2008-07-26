// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/tools/crash_service/crash_service.h"

#include <windows.h>
#include <stdlib.h>
#include <tchar.h>

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
