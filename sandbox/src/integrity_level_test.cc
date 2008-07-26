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

#include <windows.h>
#include <atlsecurity.h>

#include "base/win_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "sandbox/src/sandbox.h"
#include "sandbox/src/sandbox_policy.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/tests/common/controller.h"

namespace sandbox {


SBOX_TESTS_COMMAND int CheckIntegrityLevel(int argc, wchar_t **argv) {
  ATL::CAccessToken token;
  if (!token.GetEffectiveToken(TOKEN_READ))
    return SBOX_TEST_FAILED;

  char* buffer[100];
  DWORD buf_size = 100;
  if (!::GetTokenInformation(token.GetHandle(), TokenIntegrityLevel,
                             reinterpret_cast<void*>(buffer), buf_size,
                             &buf_size))
    return SBOX_TEST_FAILED;

  TOKEN_MANDATORY_LABEL* label =
      reinterpret_cast<TOKEN_MANDATORY_LABEL*>(buffer);

  PSID sid_low = NULL;
  if (!::ConvertStringSidToSid(L"S-1-16-4096", &sid_low))
    return SBOX_TEST_FAILED;

  BOOL is_low_sid = ::EqualSid(label->Label.Sid, sid_low);

  ::LocalFree(sid_low);

  if (is_low_sid)
    return SBOX_TEST_SUCCEEDED;

  return SBOX_TEST_DENIED;
}

TEST(IntegrityLevelTest, TestLowILReal) {
  if (win_util::WINVERSION_VISTA != win_util::GetWinVersion())
    return;

  TestRunner runner(JOB_LOCKDOWN, USER_INTERACTIVE, USER_INTERACTIVE);

  runner.SetTimeout(INFINITE);

  runner.GetPolicy()->SetIntegrityLevel(INTEGRITY_LEVEL_LOW);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckIntegrityLevel"));

  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckIntegrityLevel"));
}

TEST(DelayedIntegrityLevelTest, TestLowILDelayed) {
  if (win_util::WINVERSION_VISTA != win_util::GetWinVersion())
    return;

  TestRunner runner(JOB_LOCKDOWN, USER_INTERACTIVE, USER_INTERACTIVE);

  runner.SetTimeout(INFINITE);

  runner.GetPolicy()->SetDelayedIntegrityLevel(INTEGRITY_LEVEL_LOW);

  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"CheckIntegrityLevel"));

  runner.SetTestState(BEFORE_REVERT);
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"CheckIntegrityLevel"));
}

TEST(IntegrityLevelTest, TestNoILChange) {
  if (win_util::WINVERSION_VISTA != win_util::GetWinVersion())
    return;

  TestRunner runner(JOB_LOCKDOWN, USER_INTERACTIVE, USER_INTERACTIVE);

  runner.SetTimeout(INFINITE);

  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"CheckIntegrityLevel"));
}

}  // namespace sandbox
