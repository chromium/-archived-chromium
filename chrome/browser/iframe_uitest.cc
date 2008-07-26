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

#include "base/basictypes.h"
#include "base/file_util.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"

class IFrameTest : public UITest {
 protected:
  void NavigateAndVerifyTitle(const wchar_t* url, const wchar_t* page_title) {
    std::wstring test_file = test_data_directory_;
    file_util::AppendToPath(&test_file, url);

    NavigateToURL(net_util::FilePathToFileURL(test_file));
    Sleep(kWaitForActionMsec);  // The browser lazily updates the title.

    // Make sure the navigation succeeded.
    EXPECT_EQ(std::wstring(page_title), GetActiveTabTitle());

    // UITest will check if this crashed.
  }

};

TEST_F(IFrameTest, Crash) {
  NavigateAndVerifyTitle(L"iframe.html", L"iframe test");
}

TEST_F(IFrameTest, InEmptyFrame) {
  NavigateAndVerifyTitle(L"iframe_in_empty_frame.html", L"iframe test");
}
