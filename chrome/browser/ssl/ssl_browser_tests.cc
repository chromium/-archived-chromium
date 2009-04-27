// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_test_util.h"

#include "chrome/test/in_process_browser_test.h"

namespace {

const wchar_t* const kDocRoot = L"chrome/test/data";

class SSLBrowserTest : public InProcessBrowserTest {
};

}  // namespace

// TODO(jcampan): port SSLUITest to SSLBrowserTest.
IN_PROC_BROWSER_TEST_F(SSLBrowserTest, TestHTTP) {
}
