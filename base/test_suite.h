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

#ifndef BASE_TEST_SUITE_H__
#define BASE_TEST_SUITE_H__

// Defines a basic test suite framework for running gtest based tests.  You can
// instantiate this class in your main function and call its Run method to run
// any gtest based tests that are linked into your executable.

#include <windows.h>

#include "base/command_line.h"
#include "base/debug_on_start.h"
#include "base/icu_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/multiprocess_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestSuite {
 public:
  TestSuite(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
  }

  virtual ~TestSuite() {
    // Flush any remaining messages.  This ensures that any accumulated Task
    // objects get destroyed before we exit, which avoids noise in purify
    // leak-test results.
    message_loop_.Quit();
    message_loop_.Run();
  }

  int Run() {
    Initialize();

    // Check to see if we are being run as a client process.
    std::wstring client_func =
        parsed_command_line_.GetSwitchValue(kRunClientProcess);
    if (!client_func.empty()) {
      // Convert our function name to a usable string for GetProcAddress.
      std::string func_name(client_func.begin(), client_func.end());

      // Get our module handle and search for an exported function
      // which we can use as our client main.
      MultiProcessTest::ChildFunctionPtr func =
          reinterpret_cast<MultiProcessTest::ChildFunctionPtr>(
              GetProcAddress(GetModuleHandle(NULL), func_name.c_str()));
      if (func)
        return func();
      return -1;
    }
    return RUN_ALL_TESTS();
  }

 protected:
  // All fatal log messages (e.g. DCHECK failures) imply unit test failures
  static void UnitTestAssertHandler(const std::string& str) {
    FAIL() << str;
  }

  // Disable crash dialogs so that it doesn't gum up the buildbot
  virtual void SuppressErrorDialogs() {
    UINT new_flags = SEM_FAILCRITICALERRORS |
                     SEM_NOGPFAULTERRORBOX |
                     SEM_NOOPENFILEERRORBOX;

    // Preserve existing error mode, as discussed at http://t/dmea
    UINT existing_flags = SetErrorMode(new_flags);
    SetErrorMode(existing_flags | new_flags);
  }

  virtual void Initialize() {
    // In some cases, we do not want to see standard error dialogs.
    if (!IsDebuggerPresent() &&
        !parsed_command_line_.HasSwitch(L"show-error-dialogs")) {
      SuppressErrorDialogs();
      logging::SetLogAssertHandler(UnitTestAssertHandler);
    }

    icu_util::Initialize();
  }

  CommandLine parsed_command_line_;
  MessageLoop message_loop_;
};

#endif  // BASE_TEST_SUITE_H__
