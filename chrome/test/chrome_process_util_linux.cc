// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chrome_process_util.h"

#include <stdio.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/common/chrome_constants.h"

base::ProcessId ChromeBrowserProcessId(const FilePath& data_dir) {
  FilePath socket_name = data_dir.Append(chrome::kSingletonSocketFilename);

  std::vector<std::string> argv;
  argv.push_back("fuser");
  argv.push_back(socket_name.value());

  std::string fuser_output;
  if (!base::GetAppOutput(CommandLine(argv), &fuser_output))
    return -1;

  std::string trimmed_output;
  TrimWhitespace(fuser_output, TRIM_ALL, &trimmed_output);

  if (trimmed_output.find(' ') != std::string::npos) {
    LOG(FATAL) << "Expected exactly 1 process to have socket open: " <<
                  fuser_output;
    return -1;
  }

  int pid;
  if (!StringToInt(trimmed_output, &pid)) {
    LOG(FATAL) << "Unexpected fuser output: " << fuser_output;
    return -1;
  }
  return pid;
}
