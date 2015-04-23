// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process_util.h"

#include "chrome/test/browser/browser_test_runner.h"
#include "chrome/test/unit/chrome_test_suite.h"

// This version of the browser test launcher forks a new process for each test
// it runs.

namespace {

const wchar_t* const kGTestListTestsFlag = L"gtest_list_tests";
const wchar_t* const kChildProcessFlag = L"child";

class OutOfProcBrowserTestRunner : public browser_tests::BrowserTestRunner {
 public:
  OutOfProcBrowserTestRunner() {
  }

  virtual ~OutOfProcBrowserTestRunner() {
  }

  bool Init() {
    return true;
  }

  // Returns true if the test succeeded, false if it failed.
  bool RunTest(const std::string& test_name) {
    const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
    CommandLine new_cmd_line(cmd_line->argv());
    // Always enable disabled tests.  This method is not called with disabled
    // tests unless this flag was specified to the browser test executable.
    new_cmd_line.AppendSwitch(L"gtest_also_run_disabled_tests");
    new_cmd_line.AppendSwitchWithValue(L"gtest_filter", ASCIIToWide(test_name));
    new_cmd_line.AppendSwitch(kChildProcessFlag);

    base::ProcessHandle process_handle;
    bool r = base::LaunchApp(new_cmd_line, false, false, &process_handle);
    if (!r)
      return false;

    int exit_code = 0;
    r = base::WaitForExitCode(process_handle, &exit_code);
    if (!r)
      return false;

    return exit_code == 0;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OutOfProcBrowserTestRunner);
};

class OutOfProcBrowserTestRunnerFactory
    : public browser_tests::BrowserTestRunnerFactory {
 public:
  OutOfProcBrowserTestRunnerFactory() { }

  virtual browser_tests::BrowserTestRunner* CreateBrowserTestRunner() const {
    return new OutOfProcBrowserTestRunner();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OutOfProcBrowserTestRunnerFactory);
};

}  // namespace

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(kChildProcessFlag))
    return ChromeTestSuite(argc, argv).Run();

  if (command_line->HasSwitch(kGTestListTestsFlag))
    return ChromeTestSuite(argc, argv).Run();

  OutOfProcBrowserTestRunnerFactory test_runner_factory;
  return browser_tests::RunTests(test_runner_factory) ? 0 : 1;
}
