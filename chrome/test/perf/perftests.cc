// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/perftimer.h"
#include "base/process_util.h"
#include "chrome/common/chrome_paths.cc"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(darin): share code with base/run_all_perftests.cc

int main(int argc, char **argv) {
  base::EnableTerminationOnHeapCorruption();
  chrome::RegisterPathProvider();
  MessageLoop main_message_loop;

  testing::InitGoogleTest(&argc, argv);

  const char log_file_switch[] = "-o";
  const char* log_filename = NULL;
  for (int i = 1; i < argc; i++)  {
    if (strcmp(argv[i], log_file_switch) == 0) {
      // found the switch for the log file, use the next arg
      if (i >= argc - 1) {
        fprintf(stderr, "Log file not specified");
        return 1;
      }
      log_filename = argv[i + 1];
    }
  }
  if (!log_filename) {
    // use the default filename
    log_filename = "perf_test.log";
  }
  printf("Using output file \"%s\" (change with %s <filename>)\n",
         log_filename, log_file_switch);

  if (!InitPerfLog(log_filename)) {
    fprintf(stderr, "Unable to open log file\n");
    return 1;
  }

  // Raise to high priority to have more precise measurements. Since we don't
  // aim at 1% precision, it is not necessary to run at realtime level.
    if (!IsDebuggerPresent()) {
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
  }

  int result = RUN_ALL_TESTS();

  FinalizePerfLog();
  return result;
}

