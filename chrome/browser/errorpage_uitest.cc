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
#include "chrome/test/ui/ui_test.h"
#include "chrome/browser/automation/url_request_failed_dns_job.h"

class ErrorPageTest : public UITest {
};

TEST_F(ErrorPageTest, DNSError) {
  GURL test_url(URLRequestFailedDnsJob::kTestUrl);
  std::wstring test_host = UTF8ToWide(test_url.host());
  NavigateToURL(test_url);

  // Verify that the url is in the title.  Since it's set via Javascript, we
  // need to give it a chance to run.
  int i;
  std::wstring title;
  for (i = 0; i < 10; ++i) {
    Sleep(kWaitForActionMaxMsec / 10);
    title = GetActiveTabTitle();
    if (title.find(test_host) != std::wstring::npos) {
      // Success, bail out.
      break;
    }
  }

  if (i == 10) {
    FAIL() << "failed to get error page title; got " << title;
  }
};
