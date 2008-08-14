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

#include <windows.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "base/win_util.h"

// The test is somewhat silly, because the Vista bots some have UAC enabled
// and some have it disabled. At least we check that it does not crash.
TEST(BaseWinUtilTest, TestIsUACEnabled) {
  if (win_util::GetWinVersion() == win_util::WINVERSION_VISTA) {
    win_util::UserAccountControlIsEnabled();
  } else {
    EXPECT_TRUE(win_util::UserAccountControlIsEnabled());
  }
}

TEST(BaseWinUtilTest, TestGetUserSidString) {
  std::wstring user_sid;
  EXPECT_TRUE(win_util::GetUserSidString(&user_sid));
  EXPECT_TRUE(!user_sid.empty());
}

TEST(BaseWinUtilTest, TestGetNonClientMetrics) {
  NONCLIENTMETRICS metrics = {0};
  win_util::GetNonClientMetrics(&metrics);
  EXPECT_TRUE(metrics.cbSize > 0);
  EXPECT_TRUE(metrics.iScrollWidth > 0);
  EXPECT_TRUE(metrics.iScrollHeight > 0);
}
