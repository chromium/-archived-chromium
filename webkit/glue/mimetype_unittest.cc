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

#include <string>

#undef LOG

#include "base/string_util.h"
#include "net/url_request/url_request_unittest.h"
#include "webkit/glue/unittest_test_server.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webview.h"
#include "webkit/tools/test_shell/test_shell_test.h"

namespace {

class MimeTypeTests : public TestShellTest {
 public:
  void LoadURL(const GURL& url) {
    test_shell_->LoadURL(UTF8ToWide(url.spec()).c_str());
    test_shell_->WaitTestFinished();
  }
  
  void CheckMimeType(const char* mimetype, const std::wstring& expected) {
    std::string path("contenttype?");
    GURL url = server_->TestServerPage(path + mimetype);
    LoadURL(url);
    WebFrame* frame = test_shell_->webView()->GetMainFrame();
    EXPECT_EQ(expected, webkit_glue::DumpDocumentText(frame));
  }
  
  scoped_ptr<UnittestTestServer> server_;
};

TEST_F(MimeTypeTests, MimeTypeTests) {
  server_.reset(new UnittestTestServer);

  std::wstring expected_src(L"<html>\n<body>\n"
      L"<p>HTML text</p>\n</body>\n</html>\n");
  
  // These files should all be displayed as plain text.
  const char* plain_text[] = {
    "text/css",
    "text/foo",
    "text/richext",
    "text/rtf",
    "application/x-javascript",
  };
  for (int i = 0; i < arraysize(plain_text); ++i) {
    CheckMimeType(plain_text[i], expected_src);
  }

  // These should all be displayed as html content.
  const char* html_src[] = {
    "text/html",
    "text/xml",
    "text/xsl",
    "application/xhtml+xml",
  };
  for (int i = 0; i < arraysize(html_src); ++i) {
    CheckMimeType(html_src[i], L"HTML text");
  }

  // These shouldn't be rendered as text or HTML, but shouldn't download
  // either.
  const char* not_text[] = {
    "image/png",
    "image/gif",
    "image/jpeg",
    "image/bmp",
  };
  for (int i = 0; i < arraysize(not_text); ++i) {
    CheckMimeType(not_text[i], L"");
    test_shell_->webView()->StopLoading();
  }

  // TODO(tc): make sure other mime types properly go to download (e.g.,
  // image/foo).

  server_.reset(NULL);
}

}  // namespace
