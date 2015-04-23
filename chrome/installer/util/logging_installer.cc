// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "chrome/installer/util/logging_installer.h"

#include "base/command_line.h"
#include "base/file_path.h"
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
  FilePath log_path;

  if (PathService::Get(base::DIR_TEMP, &log_path)) {
    log_path = log_path.Append(log_filename);
    return log_path.ToWStringHack();
  } else {
    return log_filename;
  }
}

}  // namespace installer
