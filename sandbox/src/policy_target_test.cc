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

#include "base/win_util.h"
#include "sandbox/src/sandbox.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/sandbox_utils.h"
#include "sandbox/src/target_services.h"
#include "sandbox/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

// Reverts to self and verify that SetInformationToken was faked. Returns
// SBOX_TEST_SUCCEEDED if faked and SBOX_TEST_FAILED if not faked.
SBOX_TESTS_COMMAND int PolicyTargetTest_token(int argc, wchar_t **argv) {
  HANDLE thread_token;
  // Get the thread token, using impersonation.
  if (!::OpenThreadToken(GetCurrentThread(), TOKEN_IMPERSONATE |
                             TOKEN_DUPLICATE, FALSE, &thread_token))
    return ::GetLastError();

  ::RevertToSelf();
  ::CloseHandle(thread_token);

  int ret = SBOX_TEST_FAILED;
  if (::OpenThreadToken(GetCurrentThread(), TOKEN_IMPERSONATE | TOKEN_DUPLICATE,
                        FALSE, &thread_token)) {
    ret = SBOX_TEST_SUCCEEDED;
    ::CloseHandle(thread_token);
  }
  return ret;
}

// Stores the high privilege token on a static variable, change impersonation
// again to that one and verify that we are not interfering anymore with
// RevertToSelf.
SBOX_TESTS_COMMAND int PolicyTargetTest_steal(int argc, wchar_t **argv) {
  static HANDLE thread_token;
  if (!SandboxFactory::GetTargetServices()->GetState()->RevertedToSelf()) {
    if (!::OpenThreadToken(GetCurrentThread(), TOKEN_IMPERSONATE |
                               TOKEN_DUPLICATE, FALSE, &thread_token))
      return ::GetLastError();
  } else {
    if (!::SetThreadToken(NULL, thread_token))
      return ::GetLastError();

    // See if we fake the call again.
    int ret = PolicyTargetTest_token(argc, argv);
    ::CloseHandle(thread_token);
    return ret;
  }
  return 0;
}

// Opens the thread token with and without impersonation.
SBOX_TESTS_COMMAND int PolicyTargetTest_token2(int argc, wchar_t **argv) {
  HANDLE thread_token;
  // Get the thread token, using impersonation.
  if (!::OpenThreadToken(GetCurrentThread(), TOKEN_IMPERSONATE |
                             TOKEN_DUPLICATE, FALSE, &thread_token))
    return ::GetLastError();
  ::CloseHandle(thread_token);

  // Get the thread token, without impersonation.
  if (!OpenThreadToken(GetCurrentThread(), TOKEN_IMPERSONATE | TOKEN_DUPLICATE,
                       TRUE, &thread_token))
    return ::GetLastError();
  ::CloseHandle(thread_token);
  return SBOX_TEST_SUCCEEDED;
}

// Tests that we can open the current thread.
SBOX_TESTS_COMMAND int PolicyTargetTest_thread(int argc, wchar_t **argv) {
  DWORD thread_id = ::GetCurrentThreadId();
  HANDLE thread = ::OpenThread(SYNCHRONIZE, FALSE, thread_id);
  if (!thread)
    return ::GetLastError();

  ::CloseHandle(thread);
  return SBOX_TEST_SUCCEEDED;
}

// New thread entry point: do  nothing.
DWORD WINAPI PolicyTargetTest_thread_main(void* param) {
  ::Sleep(INFINITE);
  return 0;
}

// Tests that we can create a new thread, and open it.
SBOX_TESTS_COMMAND int PolicyTargetTest_thread2(int argc, wchar_t **argv) {
  // Use default values to create a new thread.
  DWORD thread_id;
  HANDLE thread = ::CreateThread(NULL, 0, &PolicyTargetTest_thread_main, 0, 0,
                                 &thread_id);
  if (!thread)
    return ::GetLastError();
  ::CloseHandle(thread);

  thread = ::OpenThread(SYNCHRONIZE, FALSE, thread_id);
  if (!thread)
    return ::GetLastError();
  ::CloseHandle(thread);

  return SBOX_TEST_SUCCEEDED;
}

// Tests that we can call CreateProcess.
SBOX_TESTS_COMMAND int PolicyTargetTest_process(int argc, wchar_t **argv) {
  // Use default values to create a new process.
  STARTUPINFO startup_info = {0};
  startup_info.cb = sizeof(startup_info);
  PROCESS_INFORMATION process_info;
  ::CreateProcess(L"foo.exe", L"foo.exe", NULL, NULL, FALSE, 0, NULL, NULL,
                  &startup_info, &process_info);

  return SBOX_TEST_SUCCEEDED;
}

TEST(PolicyTargetTest, SetInformationThread) {
  TestRunner runner;
  if (win_util::GetWinVersion() >= win_util::WINVERSION_XP) {
    runner.SetTestState(BEFORE_REVERT);
    EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"PolicyTargetTest_token"));
  }

  runner.SetTestState(AFTER_REVERT);
  EXPECT_EQ(ERROR_NO_TOKEN, runner.RunTest(L"PolicyTargetTest_token"));

  runner.SetTestState(EVERY_STATE);
  if (win_util::GetWinVersion() >= win_util::WINVERSION_XP)
    EXPECT_EQ(SBOX_TEST_FAILED, runner.RunTest(L"PolicyTargetTest_steal"));
}

TEST(PolicyTargetTest, OpenThreadToken) {
  TestRunner runner;
  if (win_util::GetWinVersion() >= win_util::WINVERSION_XP) {
    runner.SetTestState(BEFORE_REVERT);
    EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"PolicyTargetTest_token2"));
  }

  runner.SetTestState(AFTER_REVERT);
  EXPECT_EQ(ERROR_NO_TOKEN, runner.RunTest(L"PolicyTargetTest_token2"));
}

TEST(PolicyTargetTest, OpenThread) {
  TestRunner runner;
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"PolicyTargetTest_thread")) <<
      "Opens the current thread";

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"PolicyTargetTest_thread2")) <<
      "Creates a new thread and opens it";
}

TEST(PolicyTargetTest, OpenProcess) {
  TestRunner runner;
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"PolicyTargetTest_process")) <<
      "Opens a process";
}

// Launches the app in the sandbox and ask it to wait in an
// infinite loop. Waits for 2 seconds and then check if the
// desktop associated with the app thread is not the same as the
// current desktop.
TEST(PolicyTargetTest, DesktopPolicy) {
  BrokerServices* broker = GetBroker();
  ASSERT_TRUE(broker != NULL);

  // Get the path to the sandboxed app.
  wchar_t prog_name[MAX_PATH];
  GetModuleFileNameW(NULL, prog_name, MAX_PATH);

  std::wstring arguments(L"\"");
  arguments += prog_name;
  arguments += L"\" -child 0 wait";  // Don't care about the "state" argument.

  // Launch the app.
  ResultCode result = SBOX_ALL_OK;
  PROCESS_INFORMATION target = {0};

  TargetPolicy* policy = broker->CreatePolicy();
  policy->SetDesktop(L"desktop_for_sbox");
  policy->SetTokenLevel(USER_INTERACTIVE, USER_LOCKDOWN);
  result = broker->SpawnTarget(prog_name, arguments.c_str(), policy, &target);
  policy->Release();

  EXPECT_EQ(SBOX_ALL_OK, result);

  EXPECT_EQ(1, ::ResumeThread(target.hThread));

  EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(target.hProcess, 2000));

  EXPECT_NE(::GetThreadDesktop(target.dwThreadId),
            ::GetThreadDesktop(::GetCurrentThreadId()));

  HDESK desk = ::OpenDesktop(L"desktop_for_sbox", 0, FALSE, DESKTOP_ENUMERATE);
  EXPECT_TRUE(NULL != desk);
  EXPECT_TRUE(::CloseDesktop(desk));
  EXPECT_TRUE(::TerminateProcess(target.hProcess, 0));

  ::WaitForSingleObject(target.hProcess, INFINITE);

  EXPECT_TRUE(::CloseHandle(target.hProcess));
  EXPECT_TRUE(::CloseHandle(target.hThread));

  // Wait for the desktop to be deleted by the destructor of TargetProcess
  Sleep(2000);

  desk = ::OpenDesktop(L"desktop_for_sbox", 0, FALSE, DESKTOP_ENUMERATE);
  EXPECT_TRUE(NULL == desk);
}

}  // namespace sandbox
