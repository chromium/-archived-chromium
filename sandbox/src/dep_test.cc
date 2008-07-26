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

#include "sandbox/src/dep.h"

#include "sandbox/src/sandbox_utils.h"
#include "sandbox/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace {

class DepTest : public testing::Test {
 public:
  static bool IsTestCaseDisabled() {
    OSVERSIONINFOEX version_info;
    version_info.dwOSVersionInfoSize = sizeof version_info;
    GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&version_info));

    // Windows 2000 doesn't support DEP at all.
    if (version_info.dwMajorVersion == 5 && version_info.dwMinorVersion == 0)
      return true;

    // Windows XP Service Pack 0 and 1 don't support DEP at all.
    if (version_info.dwMajorVersion == 5 && version_info.dwMinorVersion == 1
        && version_info.wServicePackMajor < 2)
      return true;

    // Bug 1212371 Vista SP0 DEP support is half-baked. Nobody seem to have
    // noticed!
    if (version_info.dwMajorVersion == 6 &&
        version_info.wServicePackMajor == 0)
      return true;

    return false;
  }
};

BYTE kReturnCode[] = {
  // ret
  0xC3,
};

typedef void (*NullFunction)();

// This doesn't fail on Vista Service Pack 0 but it does on XP SP2 and Vista
// SP1. I guess this is a bug in Vista SP0 w.r.t .data PE section. Needs
// investigation to be sure it is a bug and not an error on my part.
bool GenerateDepException() {
  bool result = false;
  __try {
    void* code = kReturnCode;
    // Call this code.
    reinterpret_cast<NullFunction>(code)();
  } __except(EXCEPTION_EXECUTE_HANDLER) {
    result = true;
  }
  return result;
}

bool GenerateDepAtl7Exception() {
  // TODO(maruel):  bug 1207762 Somehow test ATL7
  return GenerateDepException();
}

SBOX_TESTS_COMMAND int CheckDepLevel(int argc, wchar_t **argv) {
  if (1 != argc)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  int flag = _wtoi(argv[0]);
  switch (flag) {
    case 1:
      // DEP is completely disabled.
      if (!SetCurrentProcessDEP(DEP_DISABLED)) {
        if (!IsXPSP2OrLater())
          // That's fine.
          return SBOX_TEST_SUCCEEDED;
        return SBOX_TEST_DENIED;
      }
      if (GenerateDepException())
        return SBOX_TEST_FAILED;
      if (GenerateDepAtl7Exception())
        return SBOX_TEST_FAILED;
      return SBOX_TEST_SUCCEEDED;
    case 2:
      // DEP is enabled with ATL7 thunk support.
      if (!SetCurrentProcessDEP(DEP_ENABLED_ATL7_COMPAT)) {
        if (!IsXPSP2OrLater())
          // That's fine.
          return SBOX_TEST_SUCCEEDED;
        return SBOX_TEST_DENIED;
      }
      if (!GenerateDepException())
        return SBOX_TEST_FAILED;
      if (GenerateDepAtl7Exception())
        return SBOX_TEST_FAILED;
      return SBOX_TEST_SUCCEEDED;
    case 3:
      // DEP is enabled.
      if (!SetCurrentProcessDEP(DEP_ENABLED)) {
        if (!IsXPSP2OrLater())
          // That's fine.
          return SBOX_TEST_SUCCEEDED;
        return SBOX_TEST_DENIED;
      }
      if (!GenerateDepException())
        return SBOX_TEST_FAILED;
      if (!GenerateDepAtl7Exception())
        return SBOX_TEST_FAILED;
      return SBOX_TEST_SUCCEEDED;
    case 4:
      // DEP can't be disabled.
      if (!SetCurrentProcessDEP(DEP_ENABLED)) {
        if (!IsXPSP2OrLater())
          // That's fine.
          return SBOX_TEST_SUCCEEDED;
      }
      if (SetCurrentProcessDEP(DEP_DISABLED)) {
        return SBOX_TEST_DENIED;
      }
      // Verify that it is still enabled.
      if (!GenerateDepException())
        return SBOX_TEST_FAILED;
      if (!GenerateDepAtl7Exception())
        return SBOX_TEST_FAILED;
      return SBOX_TEST_SUCCEEDED;
    case 5:
      // DEP can't be disabled.
      if (!SetCurrentProcessDEP(DEP_ENABLED_ATL7_COMPAT)) {
        if (!IsXPSP2OrLater())
          // That's fine.
          return SBOX_TEST_SUCCEEDED;
      }
      if (SetCurrentProcessDEP(DEP_DISABLED)) {
        return SBOX_TEST_DENIED;
      }
      // Verify that it is still enabled.
      if (!GenerateDepException())
        return SBOX_TEST_FAILED;
      if (!GenerateDepAtl7Exception())
        return SBOX_TEST_FAILED;
      return SBOX_TEST_SUCCEEDED;
    case 6:
      // DEP can't be disabled.
      if (!SetCurrentProcessDEP(DEP_ENABLED)) {
        if (!IsXPSP2OrLater())
          // That's fine.
          return SBOX_TEST_SUCCEEDED;
      }
      if (SetCurrentProcessDEP(DEP_ENABLED_ATL7_COMPAT)) {
        return SBOX_TEST_DENIED;
      }
      // Verify that it is still enabled.
      if (!GenerateDepException())
        return SBOX_TEST_FAILED;
      if (!GenerateDepAtl7Exception())
        return SBOX_TEST_FAILED;
      return SBOX_TEST_SUCCEEDED;
    default:
      return SBOX_TEST_INVALID_PARAMETER;
  }
  return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
}

}  // namespace

// This test is disabled. See bug 1275842
TEST_F(DepTest, DISABLED_TestDepDisable) {
  TestRunner runner(JOB_UNPROTECTED, USER_INTERACTIVE, USER_INTERACTIVE);

  runner.SetTimeout(INFINITE);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckDepLevel 1"));
  // TODO(maruel):  bug 1207762 Somehow test ATL7
  // EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckDepLevel 2"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckDepLevel 3"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckDepLevel 4"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckDepLevel 5"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckDepLevel 6"));
}

}  // namespace sandbox
