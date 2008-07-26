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

#include "base/message_loop.h"
#include "base/perftimer.h"
#include "chrome/common/chrome_paths.cc"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(darin): share code with base/run_all_perftests.cc

int main(int argc, char **argv) {
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
