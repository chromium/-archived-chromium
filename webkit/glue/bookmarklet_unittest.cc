// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "testing/gtest/include/gtest/gtest.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "webkit/tools/test_shell/test_shell_test.h"

namespace {

class BookmarkletTest : public TestShellTest {
 public:
  virtual void SetUp() {
    TestShellTest::SetUp();

    test_shell_->LoadURL(L"data:text/html,start page");
    test_shell_->WaitTestFinished();
  }
};

}

TEST_F(BookmarkletTest, Redirect) {
  test_shell_->LoadURL(L"javascript:location.href='data:text/plain,SUCCESS'");
  test_shell_->WaitTestFinished();
  std::wstring text = test_shell_->GetDocumentText();
  EXPECT_EQ(L"SUCCESS", text);
}

TEST_F(BookmarkletTest, NonEmptyResult) {
  test_shell_->LoadURL(L"javascript:false");
  MessageLoop::current()->RunAllPending();
  std::wstring text = test_shell_->GetDocumentText();
  EXPECT_EQ(L"false", text);

  test_shell_->LoadURL(L"javascript:'hello world'");
  MessageLoop::current()->RunAllPending();
  text = test_shell_->GetDocumentText();
  EXPECT_EQ(L"hello world", text);
}

TEST_F(BookmarkletTest, DocumentWrite) {
  test_shell_->LoadURL(
      L"javascript:document.open();"
      L"document.write('hello world');"
      L"document.close()");
  MessageLoop::current()->RunAllPending();
  std::wstring text = test_shell_->GetDocumentText();
  EXPECT_EQ(L"hello world", text);
}

