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

#include "sandbox/tests/common/controller.h"

#include <string>

#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/sandbox_utils.h"

namespace {

// Set this value to 1 to avoid timeouts while debugging the tests.
#define RUN_WITHOUT_TIMEOUTS 0

static const int kDefaultTimeout = 3000;

// Grabbed from chrome/common/string_util.h
template <class char_type>
inline char_type* WriteInto(
    std::basic_string<char_type, std::char_traits<char_type>,
                      std::allocator<char_type> >* str,
    size_t length_including_null) {
  str->reserve(length_including_null);
  str->resize(length_including_null - 1);
  return &((*str)[0]);
}

// Grabbed from chrome/common/string_util.cc
std::string WideToMultiByte(const std::wstring& wide, UINT code_page) {
  if (wide.length() == 0)
    return std::string();

  // compute the length of the buffer we'll need
  int charcount = WideCharToMultiByte(code_page, 0, wide.c_str(), -1,
                                      NULL, 0, NULL, NULL);
  if (charcount == 0)
    return std::string();

  // convert
  std::string mb;
  WideCharToMultiByte(code_page, 0, wide.c_str(), -1,
                      WriteInto(&mb, charcount), charcount, NULL, NULL);

  return mb;
}

// Grabbed from chrome/common/string_util.cc
std::string WideToUTF8(const std::wstring& wide) {
  return WideToMultiByte(wide, CP_UTF8);
}

}  // namespace

namespace sandbox {

// Utility function that constructs a full path to a file inside the system32
// folder.
std::wstring MakePathToSys32(const wchar_t* name, bool is_obj_man_path) {
  wchar_t windows_path[MAX_PATH] = {0};
  if (0 == ::GetSystemWindowsDirectoryW(windows_path, MAX_PATH))
    return std::wstring();

  std::wstring full_path(windows_path);
  if (full_path.empty())
    return full_path;

  if (is_obj_man_path)
    full_path.insert(0, L"\\??\\");

  full_path += L"\\system32\\";
  full_path += name;
  return full_path;
}

BrokerServices* GetBroker() {
  static BrokerServices* broker = SandboxFactory::GetBrokerServices();
  static bool is_initialized = false;

  if (!broker) {
    return NULL;
  }

  if (!is_initialized) {
    if (SBOX_ALL_OK != broker->Init())
      return NULL;

    is_initialized = true;
  }

  return broker;
}

TestRunner::TestRunner(JobLevel job_level, TokenLevel startup_token,
                       TokenLevel main_token) : is_init_(false) {
  Init(job_level, startup_token, main_token);
}

TestRunner::TestRunner() : is_init_(false) {
  Init(JOB_LOCKDOWN, USER_RESTRICTED_SAME_ACCESS, USER_LOCKDOWN);
}

void TestRunner::Init(JobLevel job_level, TokenLevel startup_token,
                      TokenLevel main_token) {
  broker_ = NULL;
  policy_ = NULL;
  timeout_ = kDefaultTimeout;
  state_ = AFTER_REVERT;

  broker_ = GetBroker();
  if (!broker_)
    return;

  policy_ = broker_->CreatePolicy();
  if (!policy_)
    return;

  policy_->SetJobLevel(job_level, 0);
  policy_->SetTokenLevel(startup_token, main_token);

  is_init_ = true;
}

TargetPolicy* TestRunner::GetPolicy() {
  return policy_;
}

TestRunner::~TestRunner() {
  if (policy_)
    policy_->Release();
}

bool TestRunner::AddRule(TargetPolicy::SubSystem subsystem,
                         TargetPolicy::Semantics semantics,
                         const wchar_t* pattern) {
  if (!is_init_)
    return false;

  return (SBOX_ALL_OK == policy_->AddRule(subsystem, semantics, pattern));
}

bool TestRunner::AddRuleSys32(TargetPolicy::Semantics semantics,
                              const wchar_t* pattern) {
  if (!is_init_)
    return false;

  std::wstring win32_path = MakePathToSys32(pattern, false);
  if (win32_path.empty())
    return false;

  return AddRule(TargetPolicy::SUBSYS_FILES, semantics, win32_path.c_str());
}

bool TestRunner::AddFsRule(TargetPolicy::Semantics semantics,
                           const wchar_t* pattern) {
  if (!is_init_)
    return false;

  return AddRule(TargetPolicy::SUBSYS_FILES, semantics, pattern);
}

int TestRunner::RunTest(const wchar_t* command) {
  if (MAX_STATE > 10)
    return SBOX_TEST_INVALID_PARAMETER;

  wchar_t state_number[2];
  state_number[0] = L'0' + state_;
  state_number[1] = L'\0';
  std::wstring full_command(state_number);
  full_command += L" ";
  full_command += command;

  return InternalRunTest(full_command.c_str());
}

int TestRunner::InternalRunTest(const wchar_t* command) {
  if (!is_init_)
    return SBOX_TEST_FAILED_TO_RUN_TEST;

  // Get the path to the sandboxed app.
  wchar_t prog_name[MAX_PATH];
  GetModuleFileNameW(NULL, prog_name, MAX_PATH);

  // Launch the app.
  ResultCode result = SBOX_ALL_OK;
  PROCESS_INFORMATION target = {0};

  std::wstring arguments(L"\"");
  arguments += prog_name;
  arguments += L"\" -child ";
  arguments += command;

  result = broker_->SpawnTarget(prog_name, arguments.c_str(), policy_,
                                &target);

  if (SBOX_ALL_OK != result)
    return SBOX_TEST_FAILED_TO_RUN_TEST;

  ::ResumeThread(target.hThread);

#if RUN_WITHOUT_TIMEOUTS
  timeout_ = INFINITE;
#endif

  if (WAIT_TIMEOUT == ::WaitForSingleObject(target.hProcess, timeout_)) {
    ::TerminateProcess(target.hProcess, SBOX_TEST_TIMED_OUT);
    ::CloseHandle(target.hProcess);
    ::CloseHandle(target.hThread);
    return SBOX_TEST_TIMED_OUT;
  }

  DWORD exit_code = SBOX_TEST_LAST_RESULT;
  if (!::GetExitCodeProcess(target.hProcess, &exit_code)) {
    ::CloseHandle(target.hProcess);
    ::CloseHandle(target.hThread);
    return SBOX_TEST_FAILED_TO_RUN_TEST;
  }

  ::CloseHandle(target.hProcess);
  ::CloseHandle(target.hThread);

  return exit_code;
}

void TestRunner::SetTimeout(DWORD timeout_ms) {
  timeout_ = timeout_ms;
}

void TestRunner::SetTestState(SboxTestsState desired_state) {
  state_ = desired_state;
}

// This is the main procedure for the target (child) application. We'll find out
// the target test and call it.
// We expect the arguments to be:
//  argv[1] = "-child"
//  argv[2] = SboxTestsState when to run the command
//  argv[3] = command to run
//  argv[4...] = command arguments
int DispatchCall(int argc, wchar_t **argv) {
  if (argc < 4)
    return SBOX_TEST_INVALID_PARAMETER;

  // We hard code two tests to avoid dispatch failures.
  if (0 == _wcsicmp(argv[3], L"wait")) {
      Sleep(INFINITE);
      return SBOX_TEST_TIMED_OUT;
  }

  if (0 == _wcsicmp(argv[3], L"ping"))
      return SBOX_TEST_PING_OK;

  SboxTestsState state = static_cast<SboxTestsState>(_wtoi(argv[2]));
  if ((state <= MIN_STATE) || (state >= MAX_STATE))
    return SBOX_TEST_INVALID_PARAMETER;

  HMODULE module;
  if (!GetModuleHandleHelper(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                 GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             reinterpret_cast<wchar_t*>(&DispatchCall),
                             &module))
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  std::string command_name = WideToUTF8(argv[3]);
  CommandFunction command = reinterpret_cast<CommandFunction>(
                                ::GetProcAddress(module, command_name.c_str()));

  if (!command)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  if (BEFORE_INIT == state)
    return command(argc - 4, argv + 4);
  else if (EVERY_STATE == state)
    command(argc - 4, argv + 4);

  TargetServices* target = SandboxFactory::GetTargetServices();
  if (!target)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  if (SBOX_ALL_OK != target->Init())
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  if (BEFORE_REVERT == state)
    return command(argc - 4, argv + 4);
  else if (EVERY_STATE == state)
    command(argc - 4, argv + 4);

  target->LowerToken();

  return command(argc - 4, argv + 4);
}

}  // namespace sandbox
