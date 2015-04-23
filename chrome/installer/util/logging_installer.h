// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_LOGGING_INSTALLER_H__
#define CHROME_INSTALLER_LOGGING_INSTALLER_H__

#include <string>

#include "base/logging.h"

class CommandLine;

namespace installer {

// Call to initialize logging for Chrome installer.
void InitInstallerLogging(const CommandLine& command_line);

// Call when done using logging for Chrome installer.
void EndInstallerLogging();

// Returns the full path of the log file.
std::wstring GetLogFilePath(const CommandLine& command_line);

} // namespace installer

#endif
