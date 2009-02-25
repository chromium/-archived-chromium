// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/win_util.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/test/mini_installer_test/mini_installer_test_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "chrome_mini_installer.h"

namespace {
class MiniInstallTest : public testing::Test {
   protected:
    virtual void SetUp() {
      ChromeMiniInstaller userinstall(mini_installer_constants::kUserInstall);
      userinstall.UnInstall();
      if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA) {
        ChromeMiniInstaller systeminstall(
                            mini_installer_constants::kSystemInstall);
        systeminstall.UnInstall();
      }
    }

    virtual void TearDown() {
      ChromeMiniInstaller installer(mini_installer_constants::kUserInstall);
      installer.CloseProcesses(installer_util::kChromeExe);
    }
  };
};

TEST_F(MiniInstallTest, MiniInstallerOverChromeMetaInstallerTest) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall);
  installer.OverInstall();
}

TEST_F(MiniInstallTest, MiniInstallerSystemInstallTest) {
  if (win_util::GetWinVersion() < win_util::WINVERSION_VISTA) {
    ChromeMiniInstaller installer(mini_installer_constants::kSystemInstall);
    installer.InstallMiniInstaller();
  }
}

TEST_F(MiniInstallTest, MiniInstallerUserInstallTest) {
  ChromeMiniInstaller installer(mini_installer_constants::kUserInstall);
  installer.InstallMiniInstaller();
}

