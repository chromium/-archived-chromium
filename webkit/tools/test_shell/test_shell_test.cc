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

#include "webkit/tools/test_shell/test_shell_test.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"

std::wstring TestShellTest::GetTestURL(std::wstring test_case_path, 
                                       const std::wstring& test_case) {
  file_util::AppendToPath(&test_case_path, test_case);
  return test_case_path;
}

void TestShellTest::SetUp() {
  // Make a test shell for use by the test.
  TestShell::RegisterWindowClass();
  CreateEmptyWindow();
  test_shell_->Show(test_shell_->webView(), NEW_WINDOW);

  // Point data_dir_ to the root of the test case dir
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &data_dir_));
  file_util::AppendToPath(&data_dir_, L"webkit");
  file_util::AppendToPath(&data_dir_, L"data");
  ASSERT_TRUE(file_util::PathExists(data_dir_));
}

void TestShellTest::TearDown() {
  // Loading a blank url clears the memory in the current page.
  test_shell_->LoadURL(L"about:blank");
  DestroyWindow(test_shell_->mainWnd());
  LayoutTestController::ClearShell();
  
  // Flush the MessageLoop of any residual tasks.
  MessageLoop::current()->RunAllPending();
}

void TestShellTest::CreateEmptyWindow() {
  TestShell::CreateNewWindow(L"about:blank", &test_shell_);
}