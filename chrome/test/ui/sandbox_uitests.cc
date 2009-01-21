// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <string>

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/ui/ui_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class SandboxTest : public UITest {
 protected:
  // Launches chrome with the --test-sandbox=security_tests.dll flag.
  SandboxTest() : UITest() {
    launch_arguments_.AppendSwitchWithValue(switches::kTestSandbox,
                                            L"security_tests.dll");
  }
};

// Verifies that chrome is running properly.
TEST_F(SandboxTest, ExecuteDll) {
  EXPECT_EQ(1, GetTabCount());
}

