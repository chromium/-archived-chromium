// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include <string>

#undef LOG

#include "base/file_util.h"
#include "base/string_util.h"
#include "webkit/api/public/WebDataSource.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebVector.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webframe.h"
#include "webkit/glue/webview.h"
#include "webkit/tools/test_shell/test_shell_test.h"

using WebKit::WebDataSource;
using WebKit::WebURL;
using WebKit::WebVector;

typedef TestShellTest IFrameRedirectTest;

// Tests that loading a page in an iframe from javascript results in
// a redirect from about:blank.
TEST_F(IFrameRedirectTest, Test) {
  FilePath iframes_data_dir_ = data_dir_;
  iframes_data_dir_ = iframes_data_dir_.AppendASCII("test_shell");
  iframes_data_dir_ = iframes_data_dir_.AppendASCII("iframe_redirect");
  ASSERT_TRUE(file_util::PathExists(iframes_data_dir_));

  std::wstring test_url = GetTestURL(iframes_data_dir_, "main.html");

  test_shell_->LoadURL(test_url.c_str());
  test_shell_->WaitTestFinished();

  WebFrame* iframe = test_shell_->webView()->GetFrameWithName(L"ifr");
  ASSERT_TRUE(iframe != NULL);
  WebDataSource* iframe_ds = iframe->GetDataSource();
  ASSERT_TRUE(iframe_ds != NULL);
  WebVector<WebURL> redirects;
  iframe_ds->redirectChain(redirects);
  ASSERT_FALSE(redirects.isEmpty());
  ASSERT_TRUE(GURL(redirects[0]) == GURL("about:blank"));
}
