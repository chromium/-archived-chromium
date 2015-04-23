// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/browser/browser_test_runner.h"

#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"

namespace {

const wchar_t* const kGTestListTestsFlag = L"gtest_list_tests";
const wchar_t* const kGTestRunDisabledTestsFlag =
    L"gtest_also_run_disabled_tests";

// Retrieves the list of tests to run by running gtest with the
// --gtest_list_tests flag in a forked process and parsing its output.
// |command_line| should contain the command line used to start the browser
// test launcher, it is expected that it does not contain the
// --gtest_list_tests flag already.
// Note: we cannot implement this in-process for InProcessBrowserTestRunner as
// GTest prints to the stdout and there are no good way of temporarily
// redirecting outputs.
bool GetTestList(const CommandLine& command_line,
                 std::vector<std::string>* test_list) {
  DCHECK(!command_line.HasSwitch(kGTestListTestsFlag));

  // Run ourselves with the --gtest_list_tests option and read the output.
  CommandLine new_command_line(command_line);
  new_command_line.AppendSwitch(kGTestListTestsFlag);
  std::string output;
  if (!base::GetAppOutput(new_command_line, &output))
    return false;

  // The output looks like:
  // TestCase.
  //   Test1
  //   Test2
  // OtherTestCase.
  //   FooTest
  // ...
  std::vector<std::string> lines;
  SplitString(output, '\n', &lines);

  std::string test_case;
  for (std::vector<std::string>::const_iterator iter = lines.begin();
       iter != lines.end(); ++iter) {
    std::string line = *iter;
    if (line.empty())
      continue;  // Just ignore empty lines if any.

    if (line[line.size() - 1] == '.') {
      // This is a new test case.
      test_case = line;
      continue;
    }

    if (!command_line.HasSwitch(kGTestRunDisabledTestsFlag) &&
        line.find("DISABLED") != std::string::npos)
      continue;  // Skip disabled tests.

    // We are dealing with a test.
    test_list->push_back(test_case + line);
  }
  return true;
}

}  // namespace

namespace browser_tests {

BrowserTestRunner::BrowserTestRunner() {
}

BrowserTestRunner::~BrowserTestRunner() {
}

bool RunTests(const BrowserTestRunnerFactory& browser_test_runner_factory) {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  DCHECK(!command_line->HasSwitch(kGTestListTestsFlag));

  // First let's get the list of tests we need to run.
  std::vector<std::string> test_list;
  if (!GetTestList(*command_line, &test_list)) {
    printf("Failed to retrieve the tests to run.\n");
    return false;
  }

  if (test_list.empty()) {
    printf("No tests to run.\n");
    return false;
  }

  int test_run_count = 0;
  std::vector<std::string> failed_tests;
  for (std::vector<std::string>::const_iterator iter = test_list.begin();
       iter != test_list.end(); ++iter) {
    std::string test_name = *iter;
    scoped_ptr<BrowserTestRunner> test_runner(
        browser_test_runner_factory.CreateBrowserTestRunner());
    if (!test_runner.get() || !test_runner->Init())
      return false;
    test_run_count++;
    if (!test_runner->RunTest(test_name.c_str())) {
      if (std::find(failed_tests.begin(), failed_tests.end(), test_name) ==
          failed_tests.end()) {
        failed_tests.push_back(*iter);
      }
    }
  }

  printf("%d test%s run\n", test_run_count, test_run_count > 1 ? "s" : "");
  printf("%d test%s failed\n", static_cast<int>(failed_tests.size()),
                               failed_tests.size() > 1 ? "s" : "");
  if (failed_tests.empty())
    return true;

  printf("Failing tests:\n");
  for (std::vector<std::string>::const_iterator iter = failed_tests.begin();
       iter != failed_tests.end(); ++iter) {
    printf("%s\n", iter->c_str());
  }

  return false;
}

}  // namespace
