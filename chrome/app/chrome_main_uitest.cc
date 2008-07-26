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

#include "chrome/common/chrome_constants.h"
#include "chrome/test/ui/ui_test.h"

typedef UITest ChromeMainTest;

// Launch the app, then close the app.
TEST_F(ChromeMainTest, AppLaunch) {
  // If we make it here at all, we've succeeded in retrieving the app window
  // in UITest::SetUp()--otherwise we'd fail with an exception in SetUp().

  if (UITest::in_process_renderer()) {
    EXPECT_EQ(1, UITest::GetBrowserProcessCount());
  } else {
    // We should have two instances of the browser process alive -
    // one is the Browser and the other is the Renderer.
    EXPECT_EQ(2, UITest::GetBrowserProcessCount());
  }
}

// Make sure that the testing interface is there and giving reasonable answers.
TEST_F(ChromeMainTest, AppTestingInterface) {
  int window_count = 0;
  EXPECT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  EXPECT_EQ(1, window_count);

  EXPECT_EQ(1, GetTabCount());
}

// Make sure that the second invocation creates a new window.
TEST_F(ChromeMainTest, SecondLaunch) {
  include_testing_id_ = false;
  use_existing_browser_ = true;

  LaunchBrowser(std::wstring(), false);

  int window_count;
  ASSERT_TRUE(automation()->WaitForWindowCountToChange(1, &window_count,
                                                       kWaitForActionMsec));
  ASSERT_EQ(2, window_count);
}
