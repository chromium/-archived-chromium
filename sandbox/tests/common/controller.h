// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_TESTS_COMMON_CONTROLLER_H__
#define SANDBOX_TESTS_COMMON_CONTROLLER_H__

#include <windows.h>
#include <string>

#include "sandbox/src/sandbox.h"

namespace sandbox {

// See winerror.h for details.
#define SEVERITY_INFO_FLAGS   0x40000000
#define SEVERITY_ERROR_FLAGS  0xC0000000
#define CUSTOMER_CODE         0x20000000
#define SBOX_TESTS_FACILITY   0x05B10000

// All the possible error codes returned by the child process in
// the sandbox.
enum SboxTestResult {
  SBOX_TEST_FIRST_RESULT = CUSTOMER_CODE | SBOX_TESTS_FACILITY,
  SBOX_TEST_SUCCEEDED,
  SBOX_TEST_PING_OK,
  SBOX_TEST_FIRST_INFO = SBOX_TEST_FIRST_RESULT | SEVERITY_INFO_FLAGS,
  SBOX_TEST_DENIED,     // Access was denied.
  SBOX_TEST_NOT_FOUND,  // The resource was not found.
  SBOX_TEST_FIRST_ERROR = SBOX_TEST_FIRST_RESULT | SEVERITY_ERROR_FLAGS,
  SBOX_TEST_INVALID_PARAMETER,
  SBOX_TEST_FAILED_TO_RUN_TEST,
  SBOX_TEST_FAILED_TO_EXECUTE_COMMAND,
  SBOX_TEST_TIMED_OUT,
  SBOX_TEST_FAILED,
  SBOX_TEST_LAST_RESULT
};

inline bool IsSboxTestsResult(SboxTestResult result) {
  unsigned int code = static_cast<unsigned int>(result);
  unsigned int first = static_cast<unsigned int>(SBOX_TEST_FIRST_RESULT);
  unsigned int last = static_cast<unsigned int>(SBOX_TEST_LAST_RESULT);
  return (code > first) && (code < last);
}

enum SboxTestsState {
  MIN_STATE = 1,
  BEFORE_INIT,
  BEFORE_REVERT,
  AFTER_REVERT,
  EVERY_STATE,
  MAX_STATE
};

#define SBOX_TESTS_API __declspec(dllexport)
#define SBOX_TESTS_COMMAND extern "C" SBOX_TESTS_API

extern "C" {
typedef int (*CommandFunction)(int argc, wchar_t **argv);
}

// Class to facilitate the launch of a test inside the sandbox.
class TestRunner {
 public:
  TestRunner(JobLevel job_level, TokenLevel startup_token,
             TokenLevel main_token);

  TestRunner();

  ~TestRunner();

  // Adds a rule to the policy. The parameters are the same as the AddRule
  // function in the sandbox.
  bool AddRule(TargetPolicy::SubSystem subsystem,
               TargetPolicy::Semantics semantics,
               const wchar_t* pattern);

  // Adds a filesystem rules with the path of a file in system32. The function
  // appends "pattern" to "system32" and then call AddRule. Return true if the
  // function succeeds.
  bool AddRuleSys32(TargetPolicy::Semantics semantics, const wchar_t* pattern);

  // Adds a filesystem rules to the policy. Returns true if the functions
  // succeeds.
  bool AddFsRule(TargetPolicy::Semantics semantics, const wchar_t* pattern);

  // Starts a child process in the sandbox and ask it to run |command|. Returns
  // a SboxTestResult. By default, the test runs AFTER_REVERT.
  int RunTest(const wchar_t* command);

  // Sets the timeout value for the child to run the command and return.
  void SetTimeout(DWORD timeout_ms);

  // Sets the desired state for the test to run.
  void SetTestState(SboxTestsState desired_state);

  // Returns the pointers to the policy object. It can be used to modify
  // the policy manually.
  TargetPolicy* GetPolicy();

 private:
  // Initializes the data in the object. Sets is_init_ to tree if the
  // function succeeds. This is meant to be called from the constructor.
  void Init(JobLevel job_level, TokenLevel startup_token,
            TokenLevel main_token);

  // The actual runner.
  int InternalRunTest(const wchar_t* command);

  BrokerServices* broker_;
  TargetPolicy* policy_;
  DWORD timeout_;
  SboxTestsState state_;
  bool is_init_;
};

// Returns the broker services.
BrokerServices* GetBroker();

// Constructs a full path to a file inside the system32 folder.
std::wstring MakePathToSys32(const wchar_t* name, bool is_obj_man_path);

// Runs the given test on the target process.
int DispatchCall(int argc, wchar_t **argv);

}  // namespace sandbox

#endif  // SANDBOX_TESTS_COMMON_CONTROLLER_H__
