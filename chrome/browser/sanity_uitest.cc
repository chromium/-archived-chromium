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

// This is a file for random testcases that we run into that at one point or
// another have crashed the program.

#include "chrome/test/ui/ui_test.h"

#include "base/basictypes.h"
#include "base/file_util.h"

class GoogleTest : public UITest {
 protected:
  GoogleTest() : UITest() {
    std::wstring test_file = test_data_directory_;
    file_util::AppendToPath(&test_file, L"google");
    file_util::AppendToPath(&test_file, L"google.html");
    homepage_ = test_file;
  }
};

TEST_F(GoogleTest, Crash) {
  std::wstring page_title = L"Google";

  // Make sure the navigation succeeded.
  EXPECT_EQ(page_title, GetActiveTabTitle());

  // UITest will check if this crashed.
}

class ColumnLayout : public UITest {
 protected:
  ColumnLayout() : UITest() {
    std::wstring test_file = test_data_directory_;
    file_util::AppendToPath(&test_file, L"columns.html");
    homepage_ = test_file;
  }
};

TEST_F(ColumnLayout, Crash) {
  std::wstring page_title = L"Column test";

  // Make sure the navigation succeeded.
  EXPECT_EQ(page_title, GetActiveTabTitle());

  // UIText will check if this crashed.
}
