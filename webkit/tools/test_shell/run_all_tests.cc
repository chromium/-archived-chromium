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

// Run all of our test shell tests.  This is just an entry point
// to kick off gTest's RUN_ALL_TESTS().

#include <windows.h>
#include <commctrl.h>

#include "base/at_exit.h"
#include "base/icu_util.h"
#include "base/message_loop.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_shell_test.h"
#include "testing/gtest/include/gtest/gtest.h"

const char* TestShellTest::kJavascriptDelayExitScript = 
  "<script>"
    "window.layoutTestController.waitUntilDone();"
    "window.addEventListener('load', function() {"
    "  var x = document.body.clientWidth;"  // Force a document layout
    "  window.layoutTestController.notifyDone();"
    "});"
  "</script>";

int main(int argc, char* argv[]) {
  // Some unittests may use base::Singleton<>, thus we need to instanciate
  // the AtExitManager or else we will leak objects.
  base::AtExitManager at_exit_manager;  

  TestShell::InitLogging(true);  // suppress error dialogs

  // Initialize test shell in non-interactive mode, which will let us load one
  // request than automatically quit.
  TestShell::InitializeTestShell(false);

  // Some of the individual tests wind up calling TestShell::WaitTestFinished
  // which has a timeout in it.  For these tests, we don't care about a timeout
  // so just set it to be a really large number.  This is necessary because
  // when running under Purify, we were hitting those timeouts.
  TestShell::SetFileTestTimeout(USER_TIMER_MAXIMUM);

  // Allocate a message loop for this thread.  Although it is not used
  // directly, its constructor sets up some necessary state.
  MessageLoop main_message_loop;

  // Load ICU data tables
  icu_util::Initialize();

  INITCOMMONCONTROLSEX InitCtrlEx;

  InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
  InitCtrlEx.dwICC  = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&InitCtrlEx);

  // Run the actual tests
  testing::InitGoogleTest(&argc, argv);
  int result = RUN_ALL_TESTS();

  TestShell::ShutdownTestShell();
  TestShell::CleanupLogging();
  return result;
}
