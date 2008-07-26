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

#include <windows.h>

#include "chrome/installer/util/logging_installer.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

// This should be true for the period between the end of
// InitInstallerLogging() and the beginning of EndInstallerLogging().
bool installer_logging_ = false;

void InitInstallerLogging(const CommandLine& command_line) {

  if (installer_logging_)
    return;

  if (command_line.HasSwitch(installer_util::switches::kDisableLogging)) {
    installer_logging_ = true;
    return;
  }

  logging::InitLogging(GetLogFilePath(command_line).c_str(),
                       logging::LOG_ONLY_TO_FILE,
                       logging::LOCK_LOG_FILE,
                       logging::DELETE_OLD_LOG_FILE);

  if (command_line.HasSwitch(installer_util::switches::kVerboseLogging)) {
    logging::SetMinLogLevel(logging::LOG_INFO);
  } else {
    logging::SetMinLogLevel(logging::LOG_ERROR);
  }

  installer_logging_ = true;
}

void EndInstallerLogging() {
  logging::CloseLogFile();

  installer_logging_ = false;
}

std::wstring GetLogFilePath(const CommandLine& command_line) {
  if (command_line.HasSwitch(installer_util::switches::kLogFile)) {
    return command_line.GetSwitchValue(installer_util::switches::kLogFile);
  }

  const std::wstring log_filename(L"chrome_installer.log");
  std::wstring log_path;

  if (PathService::Get(base::DIR_TEMP, &log_path)) {
    file_util::AppendToPath(&log_path, log_filename);
    return log_path;
  } else {
    return log_filename;
  }
}

}  // namespace installer
