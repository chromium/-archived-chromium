// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/tools/test_shell/test_shell_test.h"

#include "base/basictypes.h"
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
  test_shell_->DestroyWindow(test_shell_->mainWnd());
  LayoutTestController::ClearShell();
  
  // Flush the MessageLoop of any residual tasks.
  MessageLoop::current()->RunAllPending();
}

void TestShellTest::CreateEmptyWindow() {
  TestShell::CreateNewWindow(L"about:blank", &test_shell_);
}
