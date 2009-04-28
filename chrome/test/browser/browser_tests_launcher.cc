// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string_util.h"

namespace {

const wchar_t* const kBrowserTestDLLName = L"browser_tests.dll";
const wchar_t* const kGTestListTestsFlag = L"gtest_list_tests";
}

// TestEnvContext takes care of loading/unloading the DLL containing the tests.
class TestEnvContext {
 public:
  TestEnvContext()
      : module_(NULL),
        run_test_proc_(NULL) {
  }

  ~TestEnvContext() {
    if (!module_)
      return;
    BOOL r = ::FreeLibrary(module_);
    DCHECK(r);
    LOG(INFO) << "Unloaded " << kBrowserTestDLLName;
  }

  bool Init() {
    module_ = ::LoadLibrary(kBrowserTestDLLName);
    if (!module_) {
      LOG(ERROR) << "Failed to find " << kBrowserTestDLLName;
      return false;
    }

    run_test_proc_ = reinterpret_cast<RunTestProc>(
        ::GetProcAddress(module_, "RunTests"));
    if (!run_test_proc_) {
      LOG(ERROR) <<
          "Failed to find RunTest function in " << kBrowserTestDLLName;
      return false;
    }

    return true;
  }

  // Returns true if the test succeeded, false if it failed.
  bool RunTest(const std::string& test_name) {
    std::string filter_flag = StringPrintf("--gtest_filter=%s",
                                           test_name.c_str());
    char* argv[2];
    argv[0] = "";
    argv[1] = const_cast<char*>(filter_flag.c_str());
    return RunAsIs(2, argv) == 0;
  }

  // Calls-in to GTest with the arguments we were started with.
  int RunAsIs(int argc, char** argv) {
    return (run_test_proc_)(argc, argv);
  }

 private:
  typedef int (__cdecl *RunTestProc)(int, char**);

  HMODULE module_;
  RunTestProc run_test_proc_;
};

// Retrieves the list of tests to run.
// Simply uses the --gtest_list_tests option which honor the filter.
// Sadly there is no dry-run option (or willingness to get such an option) in
// GTest.  So we'll have to process disabled and repeat options ourselves.
bool GetTestList(const CommandLine& command_line,
                 std::vector<std::string>* test_list) {
  DCHECK(!command_line.HasSwitch(kGTestListTestsFlag));

  // Run ourselves with the --gtest_list_tests option and read the output.
  std::wstring new_command_line = command_line.command_line_string() + L" --" +
      kGTestListTestsFlag;
  std::string output;
  if (!base::GetAppOutput(new_command_line, &output))
    return false;

  // Now let's parse the returned output.
  // It looks like:
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
    // We are dealing with a test.
    test_list->push_back(test_case + line);
  }
  return true;
}

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(kGTestListTestsFlag)) {
    TestEnvContext test_context;
    if (!test_context.Init())
      return 1;
    return test_context.RunAsIs(argc, argv);
  }

  // First let's get the list of tests we need to run.
  std::vector<std::string> test_list;
  if (!GetTestList(*command_line, &test_list)) {
    printf("Failed to retrieve the tests to run.\n");
    return 0;
  }

  if (test_list.empty()) {
    printf("No tests to run.\n");
    return 0;
  }

  // Run the tests.
  int test_run_count = 0;
  std::vector<std::string> failed_tests;
  for (std::vector<std::string>::const_iterator iter = test_list.begin();
       iter != test_list.end(); ++iter) {
    std::string test_name = *iter;
    TestEnvContext test_context;
    if (!test_context.Init())
      return 1;
    test_run_count++;
    if (!test_context.RunTest(test_name.c_str())) {
      if (std::find(failed_tests.begin(), failed_tests.end(), test_name) ==
          failed_tests.end()) {
        failed_tests.push_back(*iter);
      }
    }
  }

  printf("%d test%s run\n", test_run_count, test_run_count > 1 ? "s" : "");
  printf("%d test%s failed\n", failed_tests.size(),
                               failed_tests.size() > 1 ? "s" : "");
  if (failed_tests.empty())
    return 0;

  printf("Failing tests:\n");
  for (std::vector<std::string>::const_iterator iter = failed_tests.begin();
       iter != failed_tests.end(); ++iter) {
    printf("%s\n", iter->c_str());
  }

  return 1;
}
