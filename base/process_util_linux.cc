// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#include <string>
#include <sys/types.h>
#include <sys/wait.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"

namespace {

enum ParsingState {
  KEY_NAME,
  KEY_VALUE
};

}  // namespace

namespace process_util {

bool LaunchApp(const std::vector<std::string>& argv,
               bool wait, ProcessHandle* process_handle) {
  bool retval = true;

  char* argv_copy[argv.size() + 1];
  for (size_t i = 0; i < argv.size(); i++) {
    argv_copy[i] = new char[argv[i].size() + 1];
    strcpy(argv_copy[i], argv[i].c_str());
  }
  argv_copy[argv.size()] = NULL;

  int pid = vfork();
  if (pid == 0) {
    execv(argv_copy[0], argv_copy);
  } else if (pid < 0) {
    retval = false;
  } else {
    if (wait)
      waitpid(pid, 0, 0);

    if(process_handle)
      *process_handle = pid;
  }

  for (size_t i = 0; i < argv.size(); i++)
    delete[] argv_copy[i];

  return retval;
}

bool LaunchApp(const CommandLine& cl,
               bool wait, bool start_hidden, ProcessHandle* process_handle) {
  return LaunchApp(cl.argv(), wait, process_handle);
}

bool WaitForSingleProcess(ProcessHandle handle, int wait_milliseconds) {
  int status;
  waitpid(handle, &status, 0);
  return WIFEXITED(status);
}

///////////////////////////////////////////////////////////////////////////////
//// ProcessMetrics

// To have /proc/self/io file you must enable CONFIG_TASK_IO_ACCOUNTING
// in your kernel configuration.
bool ProcessMetrics::GetIOCounters(IoCounters* io_counters) {
  std::string proc_io_contents;
  if (!file_util::ReadFileToString(L"/proc/self/io", &proc_io_contents))
    return false;

  (*io_counters).OtherOperationCount = 0;
  (*io_counters).OtherTransferCount = 0;

  StringTokenizer tokenizer(proc_io_contents, ": \n");
  ParsingState state = KEY_NAME;
  std::string last_key_name;
  while (tokenizer.GetNext()) {
    switch (state) {
      case KEY_NAME:
        last_key_name = tokenizer.token();
        state = KEY_VALUE;
        break;
      case KEY_VALUE:
        DCHECK(!last_key_name.empty());
        if (last_key_name == "syscr") {
          (*io_counters).ReadOperationCount = StringToInt64(tokenizer.token());
        } else if (last_key_name == "syscw") {
          (*io_counters).WriteOperationCount = StringToInt64(tokenizer.token());
        } else if (last_key_name == "rchar") {
          (*io_counters).ReadTransferCount = StringToInt64(tokenizer.token());
        } else if (last_key_name == "wchar") {
          (*io_counters).WriteTransferCount = StringToInt64(tokenizer.token());
        }
        state = KEY_NAME;
        break;
    }
  }
  return true;
}

}  // namespace process_util
