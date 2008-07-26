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
#include "sandbox/src/sandbox_policy.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/tests/common/controller.h"

namespace sandbox {


SBOX_TESTS_COMMAND int NamedPipe_Create(int argc, wchar_t **argv) {
  if (argc != 1) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
  if ((NULL != argv) && (NULL == argv[0])) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  HANDLE pipe = ::CreateNamedPipeW(argv[0],
                                   PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, 1, 4096,
                                   4096, 2000, NULL);
  if (INVALID_HANDLE_VALUE == pipe)
    return SBOX_TEST_DENIED;

  OVERLAPPED overlapped = {0};
  overlapped.hEvent = ::CreateEvent(NULL, TRUE, TRUE, NULL);
  BOOL result = ::ConnectNamedPipe(pipe, &overlapped);
  ::CloseHandle(pipe);
  ::CloseHandle(overlapped.hEvent);

  if (!result) {
    DWORD error = ::GetLastError();
    if (ERROR_PIPE_CONNECTED != error &&
        ERROR_IO_PENDING != error) {
          return SBOX_TEST_FAILED;
    }
  }

  return SBOX_TEST_SUCCEEDED;
}

// Tests if we can create a pipe in the sandbox. On XP, the sandbox can create
// a pipe without any help but it fails on Vista, this is why we do not test
// the "denied" case.
TEST(NamedPipePolicyTest, CreatePipe) {
  TestRunner runner;
  // TODO(nsylvain): This policy is wrong because "*" is a valid char in a
  // namedpipe name. Here we apply it like a wildcard. http://b/893603
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_NAMED_PIPES,
                             TargetPolicy::NAMEDPIPES_ALLOW_ANY,
                              L"\\\\.\\pipe\\test*"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"NamedPipe_Create \\\\.\\pipe\\testbleh"));
}

// The same test as CreatePipe but this time using strict interceptions.
TEST(NamedPipePolicyTest, CreatePipeStrictInterceptions) {
  TestRunner runner;
  runner.GetPolicy()->SetStrictInterceptions();

  // TODO(nsylvain): This policy is wrong because "*" is a valid char in a
  // namedpipe name. Here we apply it like a wildcard. http://b/893603
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_NAMED_PIPES,
                             TargetPolicy::NAMEDPIPES_ALLOW_ANY,
                              L"\\\\.\\pipe\\test*"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"NamedPipe_Create \\\\.\\pipe\\testbleh"));
}

}  // namespace sandbox
