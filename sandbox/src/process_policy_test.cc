// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <memory>

#include "base/scoped_handle_win.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "sandbox/src/sandbox.h"
#include "sandbox/src/sandbox_policy.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/sandbox_utils.h"
#include "sandbox/tests/common/controller.h"

namespace {

// While the shell API provides better calls than this home brew function
// we use GetSystemWindowsDirectoryW which does not query the registry so
// it is safe to use after revert.
std::wstring MakeFullPathToSystem32(const wchar_t* name) {
  wchar_t windows_path[MAX_PATH] = {0};
  ::GetSystemWindowsDirectoryW(windows_path, MAX_PATH);
  std::wstring full_path(windows_path);
  if (full_path.empty()) {
    return full_path;
  }
  full_path += L"\\system32\\";
  full_path += name;
  return full_path;
}

// Creates a process with the |exe| and |command| parameter using the
// unicode and ascii version of the api.
sandbox::SboxTestResult CreateProcessHelper(const std::wstring &exe,
                                            const std::wstring &command) {
  PROCESS_INFORMATION pi;
  STARTUPINFOW si = {sizeof(si)};

  wchar_t *exe_name = NULL;
  if (!exe.empty())
    exe_name = const_cast<wchar_t*>(exe.c_str());

  wchar_t *cmd_line = NULL;
  if (!command.empty())
    cmd_line = const_cast<wchar_t*>(command.c_str());

  // Create the process with the unicode version of the API.
  sandbox::SboxTestResult ret1 = sandbox::SBOX_TEST_FAILED;
  if (!::CreateProcessW(exe_name, cmd_line, NULL, NULL, FALSE, 0, NULL,
                        NULL, &si, &pi)) {
    DWORD last_error = GetLastError();
    if ((ERROR_NOT_ENOUGH_QUOTA == last_error) ||
        (ERROR_ACCESS_DENIED == last_error) ||
        (ERROR_FILE_NOT_FOUND == last_error)) {
      ret1 = sandbox::SBOX_TEST_DENIED;
    } else {
      ret1 = sandbox::SBOX_TEST_FAILED;
    }
  } else {
    ret1 = sandbox::SBOX_TEST_SUCCEEDED;
  }

  // Do the same with the ansi version of the api
  STARTUPINFOA sia = {sizeof(sia)};
  sandbox::SboxTestResult ret2 = sandbox::SBOX_TEST_FAILED;

  std::string narrow_cmd_line = sandbox::WideToMultiByte(cmd_line);
  if (!::CreateProcessA(
        exe_name ? sandbox::WideToMultiByte(exe_name).c_str() : NULL,
        cmd_line ? const_cast<char*>(narrow_cmd_line.c_str()) : NULL,
        NULL, NULL, FALSE, 0, NULL, NULL, &sia, &pi)) {
    DWORD last_error = GetLastError();
    if ((ERROR_NOT_ENOUGH_QUOTA == last_error) ||
        (ERROR_ACCESS_DENIED == last_error) ||
        (ERROR_FILE_NOT_FOUND == last_error)) {
      ret2 = sandbox::SBOX_TEST_DENIED;
    } else {
      ret2 = sandbox::SBOX_TEST_FAILED;
    }
  } else {
    ret2 = sandbox::SBOX_TEST_SUCCEEDED;
  }

  if (ret1 == ret2)
    return ret1;

  return sandbox::SBOX_TEST_FAILED;
}

}  // namespace

namespace sandbox {

// Tries to create the process in argv[0] using 7 different ways.
// Since we also try the Ansi and Unicode version of the CreateProcess API,
// The process referenced by argv[0] will be spawned 14 times.
SBOX_TESTS_COMMAND int Process_RunApp(int argc, wchar_t **argv) {
  if (argc != 1) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
  if ((NULL != argv) && (NULL == argv[0])) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
  std::wstring path = MakeFullPathToSystem32(argv[0]);

  // TEST 1: Try with the path in the app_name.
  int result1 = CreateProcessHelper(path, std::wstring());

  // TEST 2: Try with the path in the cmd_line.
  std::wstring cmd_line = L"\"";
  cmd_line += path;
  cmd_line += L"\"";
  int result2 = CreateProcessHelper(std::wstring(), cmd_line);

  // TEST 3: Try file name in the cmd_line.
  int result3 = CreateProcessHelper(std::wstring(), argv[0]);

  // TEST 4: Try file name in the app_name and current directory sets correctly.
  std::wstring system32 = MakeFullPathToSystem32(L"");
  wchar_t current_directory[MAX_PATH + 1];
  int result4;
  bool test_succeeded = false;
  DWORD ret = ::GetCurrentDirectory(MAX_PATH, current_directory);
  if (0 != ret && ret < MAX_PATH) {
    current_directory[ret] = L'\\';
    current_directory[ret+1] = L'\0';
    if (::SetCurrentDirectory(system32.c_str())) {
      result4 = CreateProcessHelper(argv[0], std::wstring());
      if (::SetCurrentDirectory(current_directory)) {
        test_succeeded = true;
      }
    }
  }
  if (!test_succeeded)
    result4 = SBOX_TEST_FAILED;

  // TEST 5: Try with the path in the cmd_line and arguments.
  cmd_line = L"\"";
  cmd_line += path;
  cmd_line += L"\" /INSERT";
  int result5 = CreateProcessHelper(std::wstring(), cmd_line);

  // TEST 6: Try with the file_name in the cmd_line and arguments.
  cmd_line = argv[0];
  cmd_line += L" /INSERT";
  int result6 = CreateProcessHelper(std::wstring(), cmd_line);

  // TEST 7: Try with the path without the drive.
  cmd_line = path.substr(path.find(L'\\'));
  int result7 = CreateProcessHelper(std::wstring(), cmd_line);

  // Check if they all returned the same thing.
  if ((result1 == result2) && (result2 == result3) && (result3 == result4) &&
      (result4 == result5) && (result5 == result6) && (result6 == result7))
    return result1;

  return SBOX_TEST_FAILED;
}

// Creates a process and checks if it's possible to get a handle to it's token.
SBOX_TESTS_COMMAND int Process_GetChildProcessToken(int argc, wchar_t **argv) {
  if (argc != 1)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  if ((NULL != argv) && (NULL == argv[0]))
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  std::wstring path = MakeFullPathToSystem32(argv[0]);

  PROCESS_INFORMATION pi = {0};
  STARTUPINFOW si = {sizeof(si)};

  if (!::CreateProcessW(path.c_str(), NULL, NULL, NULL, FALSE, CREATE_SUSPENDED,
                        NULL, NULL, &si, &pi)) {
      return SBOX_TEST_FAILED;
  }

  ScopedHandle process(pi.hProcess);
  ScopedHandle thread(pi.hThread);

  HANDLE token = NULL;
  BOOL result = ::OpenProcessToken(process.Get(), TOKEN_IMPERSONATE, &token);
  DWORD error = ::GetLastError();

  ScopedHandle token_handle(token);

  if (!::TerminateProcess(process.Get(), 0))
    return SBOX_TEST_FAILED;

  if (result && token)
    return SBOX_TEST_SUCCEEDED;

  if (ERROR_ACCESS_DENIED == error)
    return SBOX_TEST_DENIED;

  return SBOX_TEST_FAILED;
}


SBOX_TESTS_COMMAND int Process_OpenToken(int argc, wchar_t **argv) {
  HANDLE token;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS, &token)) {
    if (ERROR_ACCESS_DENIED == ::GetLastError()) {
      return SBOX_TEST_DENIED;
    }
  } else {
    ::CloseHandle(token);
    return SBOX_TEST_SUCCEEDED;
  }

  return SBOX_TEST_FAILED;
}

TEST(ProcessPolicyTest, TestAllAccess) {
  // Check if the "all access" rule fails to be added when the token is too
  // powerful.
  TestRunner runner;

  // Check the failing case.
  runner.GetPolicy()->SetTokenLevel(USER_INTERACTIVE, USER_LOCKDOWN);
  EXPECT_EQ(SBOX_ERROR_UNSUPPORTED,
            runner.GetPolicy()->AddRule(TargetPolicy::SUBSYS_PROCESS,
                                        TargetPolicy::PROCESS_ALL_EXEC,
                                        L"this is not important"));

  // Check the working case.
  runner.GetPolicy()->SetTokenLevel(USER_INTERACTIVE, USER_INTERACTIVE);

  EXPECT_EQ(SBOX_ALL_OK,
            runner.GetPolicy()->AddRule(TargetPolicy::SUBSYS_PROCESS,
                                        TargetPolicy::PROCESS_ALL_EXEC,
                                        L"this is not important"));
}

// This test is disabled.  See bug 1305476.
TEST(ProcessPolicyTest, DISABLED_RunFindstrExe) {
  TestRunner runner;
  std::wstring exe_path = MakeFullPathToSystem32(L"findstr.exe");
  std::wstring system32 = MakeFullPathToSystem32(L"");
  ASSERT_TRUE(!exe_path.empty());
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_PROCESS,
                             TargetPolicy::PROCESS_MIN_EXEC,
                             exe_path.c_str()));

  // Need to add directory rules for the directories that we use in
  // SetCurrentDirectory.
  EXPECT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_DIR_ANY,
                               system32.c_str()));

  wchar_t current_directory[MAX_PATH];
  DWORD ret = ::GetCurrentDirectory(MAX_PATH, current_directory);
  ASSERT_TRUE(0 != ret && ret < MAX_PATH);

  wcscat_s(current_directory, MAX_PATH, L"\\");
  EXPECT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_DIR_ANY,
                               current_directory));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Process_RunApp findstr.exe"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Process_RunApp calc.exe"));
}

TEST(ProcessPolicyTest, OpenToken) {
  TestRunner runner;
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Process_OpenToken"));
}

TEST(ProcessPolicyTest, TestGetProcessTokenMinAccess) {
  TestRunner runner;
  std::wstring exe_path = MakeFullPathToSystem32(L"findstr.exe");
  ASSERT_TRUE(!exe_path.empty());
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_PROCESS,
                             TargetPolicy::PROCESS_MIN_EXEC,
                             exe_path.c_str()));

  EXPECT_EQ(SBOX_TEST_DENIED,
            runner.RunTest(L"Process_GetChildProcessToken findstr.exe"));
}

TEST(ProcessPolicyTest, TestGetProcessTokenMaxAccess) {
  TestRunner runner(JOB_UNPROTECTED, USER_INTERACTIVE, USER_INTERACTIVE);
  std::wstring exe_path = MakeFullPathToSystem32(L"findstr.exe");
  ASSERT_TRUE(!exe_path.empty());
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_PROCESS,
                             TargetPolicy::PROCESS_ALL_EXEC,
                             exe_path.c_str()));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"Process_GetChildProcessToken findstr.exe"));
}

}  // namespace sandbox
