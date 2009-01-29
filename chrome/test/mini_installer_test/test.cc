// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_mini_installer.h"
#include "mini_installer_test_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class MiniInstallTest : public testing::Test {
   protected:
    virtual void SetUp() {
      ChromeMiniInstaller userinstall(mini_installer_constants::kUserInstall);
      userinstall.UnInstall();
      ChromeMiniInstaller systeminstall(
                          mini_installer_constants::kSystemInstall);
      systeminstall.UnInstall();
    }

    virtual void TearDown() {
      // Currently no tear down required
    }
  };
};

TEST_F(MiniInstallTest, MiniInstallerOverChromeMetaInstallerTest) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall);
  installer.OverInstall();
}

TEST_F(MiniInstallTest, MiniInstallerSystemInstallTest) {
  ChromeMiniInstaller installer(mini_installer_constants::kSystemInstall);
  installer.InstallMiniInstaller();
}

TEST_F(MiniInstallTest, MiniInstallerUserInstallTest) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall);
  installer.InstallMiniInstaller();
}

