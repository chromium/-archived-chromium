// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/command_line.h"
#include "base/perftimer.h"
#include "base/string_util.h"
#include "base/test_suite.h"

class PerfTestSuite : public TestSuite {
 public:
  PerfTestSuite(int argc, char** argv) : TestSuite(argc, argv) {
  }

  virtual void Initialize() {
    // Initialize the perf timer log
    std::string log_file =
        WideToUTF8(CommandLine().GetSwitchValue(L"log-file"));
    if (log_file.empty())
      log_file = "perf_test.log";
    ASSERT_TRUE(InitPerfLog(log_file.c_str()));

    // Raise to high priority to have more precise measurements. Since we don't
    // aim at 1% precision, it is not necessary to run at realtime level.
    if (!IsDebuggerPresent())
      SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    TestSuite::Initialize();
  }

  virtual void Shutdown() {
    TestSuite::Shutdown();

    FinalizePerfLog();
  }
};

int main(int argc, char** argv) {
  return PerfTestSuite(argc, argv).Run();
}
