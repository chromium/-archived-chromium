// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_mini_installer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class MiniInstallTest : public testing::Test {
   protected:
    virtual void SetUp() {
      // Currently no setup required
    }

    virtual void TearDown() {
      // Currently no tear down required
    }
  };
};

// This test is disabled. See bug 1199175.
TEST_F(MiniInstallTest, DISABLED_MiniInstallerCleanInstallTest) {
  ChromeMiniInstaller installer;
  installer.InstallMiniInstaller();
  installer.UnInstall();
}

// This test is disabled. See bug 1199175.
TEST_F(MiniInstallTest, DISABLED_MiniInstallerOverChromeSetupDevTest) {
  ChromeMiniInstaller installer;
  installer.OverInstall();
  installer.UnInstall();
}

