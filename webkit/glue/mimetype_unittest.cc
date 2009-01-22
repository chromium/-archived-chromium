// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  scoped_refptr<UnittestTestServer> server_;
};

TEST_F(MimeTypeTests, MimeTypeTests) {
  server_ = UnittestTestServer::CreateServer();
  ASSERT_TRUE(NULL != server_.get());

  std::wstring expected_src(L"<html>\n<body>\n"
      L"<p>HTML text</p>\n</body>\n</html>\n");

  // These files should all be displayed as plain text.
  const char* plain_text[] = {
    // It is unclear whether to display text/css or download it.
    //   Firefox 3: Display
    //   Internet Explorer 7: Download
    //   Safari 3.2: Download
    // We choose to match Safari.
    // "text/css",
    "text/javascript",
    "text/plain",
    "application/x-javascript",
  };
  for (size_t i = 0; i < arraysize(plain_text); ++i) {
    CheckMimeType(plain_text[i], expected_src);
  }

  // These should all be displayed as html content.
  const char* html_src[] = {
    "text/html",
    "text/xml",
    "text/xsl",
    "application/xhtml+xml",
  };
  for (size_t i = 0; i < arraysize(html_src); ++i) {
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
  for (size_t i = 0; i < arraysize(not_text); ++i) {
    CheckMimeType(not_text[i], L"");
    test_shell_->webView()->StopLoading();
  }

  // TODO(tc): make sure other mime types properly go to download (e.g.,
  // image/foo).

}

}  // namespace

