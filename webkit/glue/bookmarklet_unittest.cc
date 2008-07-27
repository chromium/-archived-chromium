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
  MessageLoop::current()->Quit();
  MessageLoop::current()->Run();
  std::wstring text = test_shell_->GetDocumentText();
  EXPECT_EQ(L"false", text);

  test_shell_->LoadURL(L"javascript:'hello world'");
  MessageLoop::current()->Quit();
  MessageLoop::current()->Run();
  text = test_shell_->GetDocumentText();
  EXPECT_EQ(L"hello world", text);
}

TEST_F(BookmarkletTest, DocumentWrite) {
  test_shell_->LoadURL(
      L"javascript:document.open();"
      L"document.write('hello world');"
      L"document.close()");
  MessageLoop::current()->Quit();
  MessageLoop::current()->Run();
  std::wstring text = test_shell_->GetDocumentText();
  EXPECT_EQ(L"hello world", text);
}
