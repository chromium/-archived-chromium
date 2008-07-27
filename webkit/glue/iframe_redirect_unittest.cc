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

#include "base/file_util.h"
#include "base/string_util.h"
#include "webkit/glue/webdatasource.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webview.h"
#include "webkit/tools/test_shell/test_shell_test.h"

typedef TestShellTest IFrameRedirectTest;

// Tests that loading a page in an iframe from javascript results in
// a redirect from about:blank.
TEST_F(IFrameRedirectTest, Test) {
  std::wstring iframes_data_dir_ = data_dir_;

  file_util::AppendToPath(&iframes_data_dir_, L"test_shell");
  file_util::AppendToPath(&iframes_data_dir_, L"iframe_redirect");
  ASSERT_TRUE(file_util::PathExists(iframes_data_dir_));

  std::wstring test_url = GetTestURL(iframes_data_dir_, L"main.html");

  test_shell_->LoadURL(test_url.c_str());
  test_shell_->WaitTestFinished();

  WebFrame* iframe = test_shell_->webView()->GetFrameWithName(L"ifr");
  ASSERT_TRUE(iframe != NULL);
  WebDataSource* iframe_ds = iframe->GetDataSource();
  ASSERT_TRUE(iframe_ds != NULL);
  const std::vector<GURL>& redirects = iframe_ds->GetRedirectChain();
  ASSERT_FALSE(redirects.empty());
  ASSERT_TRUE(redirects[0] == GURL("about:blank"));
}
