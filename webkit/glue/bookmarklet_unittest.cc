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

TEST_F(BookmarkletTest, Redirect) {
  test_shell_->LoadURL(L"javascript:location.href='data:text/plain,SUCCESS'");
  test_shell_->WaitTestFinished();
  std::wstring text = test_shell_->GetDocumentText();
  EXPECT_EQ(L"SUCCESS", text);
}

TEST_F(BookmarkletTest, RedirectVoided) {
  // This test should be redundant with the Redirect test above.  The point
  // here is to emphasize that in either case the assignment to location during
  // the evaluation of the script should suppress loading the script result.
  // Here, because of the void() wrapping there is no script result.
  test_shell_->LoadURL(L"javascript:void(location.href='data:text/plain,SUCCESS')");
  test_shell_->WaitTestFinished();
  std::wstring text = test_shell_->GetDocumentText();
  EXPECT_EQ(L"SUCCESS", text);
}

TEST_F(BookmarkletTest, NonEmptyResult) {
  std::wstring text;

  // TODO(darin): This test fails in a JSC build.  WebCore+JSC does not really
  // need to support this usage until WebCore supports javascript: URLs that
  // generate content (https://bugs.webkit.org/show_bug.cgi?id=14959).  It is
  // important to note that Safari does not support bookmarklets, and this is
  // really an edge case.  Our behavior with V8 is consistent with FF and IE.
#if 0
  test_shell_->LoadURL(L"javascript:false");
  MessageLoop::current()->RunAllPending();
  text = test_shell_->GetDocumentText();
  EXPECT_EQ(L"false", text);
#endif

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

}  // namespace
