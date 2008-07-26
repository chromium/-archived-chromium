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

// Some tests for the framework itself.

#include "testing/gtest/include/gtest/gtest.h"
#include "sandbox/src/sandbox.h"
#include "sandbox/src/target_services.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/tests/common/controller.h"

namespace sandbox {

// Returns the current process state.
SBOX_TESTS_COMMAND int IntegrationTestsTest_state(int argc, wchar_t **argv) {
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled())
    return BEFORE_INIT;

  if (!SandboxFactory::GetTargetServices()->GetState()->RevertedToSelf())
    return BEFORE_REVERT;

  return AFTER_REVERT;
}

// Returns the current process state, keeping track of it.
SBOX_TESTS_COMMAND int IntegrationTestsTest_state2(int argc, wchar_t **argv) {
  static SboxTestsState state = MIN_STATE;
  if (!SandboxFactory::GetTargetServices()->GetState()->InitCalled()) {
    if (MIN_STATE == state)
      state = BEFORE_INIT;
    return state;
  }

  if (!SandboxFactory::GetTargetServices()->GetState()->RevertedToSelf()) {
    if (BEFORE_INIT == state)
      state = BEFORE_REVERT;
    return state;
  }

  if (BEFORE_REVERT == state)
    state =  AFTER_REVERT;
  return state;
}

// Returns the number of arguments
SBOX_TESTS_COMMAND int IntegrationTestsTest_args(int argc, wchar_t **argv) {
  for (int i = 0; i < argc; i++) {
    wchar_t argument[20];
    size_t argument_bytes = wcslen(argv[i]) * sizeof(wchar_t);
    memcpy(argument, argv[i], __min(sizeof(argument), argument_bytes));
  }

  return argc;
}

TEST(IntegrationTestsTest, CallsBeforeInit) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(BEFORE_INIT);
  ASSERT_EQ(BEFORE_INIT, runner.RunTest(L"IntegrationTestsTest_state"));
}

TEST(IntegrationTestsTest, CallsBeforeRevert) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(BEFORE_REVERT);
  ASSERT_EQ(BEFORE_REVERT, runner.RunTest(L"IntegrationTestsTest_state"));
}

TEST(IntegrationTestsTest, CallsAfterRevert) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(AFTER_REVERT);
  ASSERT_EQ(AFTER_REVERT, runner.RunTest(L"IntegrationTestsTest_state"));
}

TEST(IntegrationTestsTest, CallsEveryState) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(EVERY_STATE);
  ASSERT_EQ(AFTER_REVERT, runner.RunTest(L"IntegrationTestsTest_state2"));
}

TEST(IntegrationTestsTest, ForwardsArguments) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(BEFORE_INIT);
  ASSERT_EQ(1, runner.RunTest(L"IntegrationTestsTest_args first"));
  ASSERT_EQ(4, runner.RunTest(L"IntegrationTestsTest_args first second third "
                              L"fourth"));
}

}  // namespace sandbox
