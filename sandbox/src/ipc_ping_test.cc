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

#include "testing/gtest/include/gtest/gtest.h"
#include "sandbox/src/sandbox.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/target_services.h"
#include "sandbox/tests/common/controller.h"

namespace sandbox {

// Tests that the IPC is working by issuing a special IPC that is not exposed
// in the public API.
SBOX_TESTS_COMMAND int IPC_Ping(int argc, wchar_t **argv) {
  if (argc != 1)
    return SBOX_TEST_FAILED;

  TargetServices* ts = SandboxFactory::GetTargetServices();
  if (NULL == ts)
    return SBOX_TEST_FAILED;

  // Downcast because we have internal knowledge of the object returned.
  TargetServicesBase* ts_base = reinterpret_cast<TargetServicesBase*>(ts);

  int version = 0;
  if (L'1' == argv[0][0])
    version = 1;
  else
    version = 2;

  if (!ts_base->TestIPCPing(version))
    return SBOX_TEST_FAILED;

  ::Sleep(1);
  if (!ts_base->TestIPCPing(version))
    return SBOX_TEST_FAILED;

  return SBOX_TEST_SUCCEEDED;
}

// The IPC ping test should work before and after the token drop.
TEST(IPCTest, IPCPingTestSimple) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(EVERY_STATE);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"IPC_Ping 1"));
}

TEST(IPCTest, IPCPingTestWithOutput) {
  TestRunner runner;
  runner.SetTimeout(2000);
  runner.SetTestState(EVERY_STATE);
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"IPC_Ping 2"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"IPC_Ping 2"));
}

}  // namespace sandbox
