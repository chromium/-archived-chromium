// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for GoogleChromeDistribution class.

#include <windows.h>

#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_chrome_distribution.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class GoogleChromeDistributionTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Currently no setup required.
  }

  virtual void TearDown() {
    // Currently no tear down required.
  }
};
}  // namespace

#if defined(GOOGLE_CHROME_BUILD)
TEST_F(GoogleChromeDistributionTest, GetNewGoogleUpdateApKeyTest) {
  GoogleChromeDistribution* dist = static_cast<GoogleChromeDistribution*>(
      BrowserDistribution::GetDistribution());
  installer_util::InstallStatus s = installer_util::FIRST_INSTALL_SUCCESS;
  installer_util::InstallStatus f = installer_util::INSTALL_FAILED;

  // Incremental Installer that worked.
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, s, L""), L"");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, s, L"1.1"), L"1.1");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, s, L"1.1-dev"), L"1.1-dev");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, s, L"-full"), L"");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, s, L"1.1-full"), L"1.1");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, s, L"1.1-dev-full"),
            L"1.1-dev");

  // Incremental Installer that failed.
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, f, L""), L"-full");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, f, L"1.1"), L"1.1-full");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, f, L"1.1-dev"),
            L"1.1-dev-full");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, f, L"-full"), L"-full");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, f, L"1.1-full"), L"1.1-full");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(true, f, L"1.1-dev-full"),
            L"1.1-dev-full");

  // Full Installer that worked.
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, s, L""), L"");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, s, L"1.1"), L"1.1");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, s, L"1.1-dev"), L"1.1-dev");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, s, L"-full"), L"");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, s, L"1.1-full"), L"1.1");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, s, L"1.1-dev-full"),
            L"1.1-dev");

  // Full Installer that failed.
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, f, L""), L"");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, f, L"1.1"), L"1.1");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, f, L"1.1-dev"), L"1.1-dev");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, f, L"-full"), L"");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, f, L"1.1-full"), L"1.1");
  EXPECT_EQ(dist->GetNewGoogleUpdateApKey(false, f, L"1.1-dev-full"),
            L"1.1-dev");
}
#endif

