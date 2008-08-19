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
