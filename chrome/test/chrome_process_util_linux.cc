// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chrome_process_util.h"

#include <stdio.h>

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/string_util.h"

base::ProcessId ChromeBrowserProcessId(const FilePath& data_dir) {
  char fuser_output[256];

  FilePath socket_name = data_dir.Append("SingletonSocket");
  // TODO(phajdan.jr): Do better quoting around the socket name.
  std::string cmd = "fuser \"" + socket_name.value() + "\"";
  FILE* fuser_pipe = popen(cmd.c_str(), "r");
  if (!fuser_pipe) {
    DLOG(ERROR) << "Error launching fuser.";
    return -1;
  }

  char* rv = fgets(fuser_output, 256, fuser_pipe);
  pclose(fuser_pipe);

  if (!rv)
    return -1;

  std::string trimmed_output;
  TrimWhitespace(fuser_output, TRIM_ALL, &trimmed_output);

  for (size_t i = 0; i < trimmed_output.size(); ++i) {
    if (trimmed_output[i] == ' '){
       DLOG(ERROR) << "Expected exactly 1 process to have socket open: " <<
                      fuser_output;
       return -1;
    }
  }

  int pid;
  if (!StringToInt(trimmed_output, &pid)) {
    DLOG(ERROR) << "Unexpected fuser output: " << fuser_output;
    return -1;
  }
  return pid;
}
