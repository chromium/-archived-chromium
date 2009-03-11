// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webview.h"
#include "webkit/tools/test_shell/test_shell_test.h"

class WebFrameTest : public TestShellTest {
 public:
};

TEST_F(WebFrameTest, GetContentAsPlainText) {
  WebView* view = test_shell_->webView();
  WebFrame* frame = view->GetMainFrame();

  // Generate a simple test case.
  const char simple_source[] = "<div>Foo bar</div><div></div>baz";
  GURL test_url("http://foo/");
  frame->LoadHTMLString(simple_source, test_url);
  test_shell_->WaitTestFinished();

  // Make sure it comes out OK.
  const std::wstring expected(ASCIIToWide("Foo bar\nbaz"));
  std::wstring text;
  frame->GetContentAsPlainText(std::numeric_limits<int>::max(), &text);
  EXPECT_EQ(expected, text);

  // Try reading the same one with clipping of the text.
  const int len = 5;
  frame->GetContentAsPlainText(len, &text);
  EXPECT_EQ(expected.substr(0, len), text);

  // Now do a new test with a subframe.
  const char outer_frame_source[] = "Hello<iframe></iframe> world";
  frame->LoadHTMLString(outer_frame_source, test_url);
  test_shell_->WaitTestFinished();

  // Load something into the subframe.
  WebFrame* subframe = frame->GetChildFrame(L"/html/body/iframe");
  ASSERT_TRUE(subframe);
  subframe->LoadHTMLString("sub<p>text", test_url);
  test_shell_->WaitTestFinished();

  frame->GetContentAsPlainText(std::numeric_limits<int>::max(), &text);
  EXPECT_EQ("Hello world\n\nsub\ntext", WideToUTF8(text));

  // Get the frame text where the subframe separator falls on the boundary of
  // what we'll take. There used to be a crash in this case.
  frame->GetContentAsPlainText(12, &text);
  EXPECT_EQ("Hello world", WideToUTF8(text));
}
