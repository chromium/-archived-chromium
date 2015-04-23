// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/compat_checks.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test that we detect the incompatible SEP version. The very last digit
// of the version does not matter but must be present.
TEST(CompatTests, SymantecSEP) {
  EXPECT_FALSE(HasIncompatibleSymantecEndpointVersion(L"11.0.3001.0"));
  EXPECT_TRUE(HasIncompatibleSymantecEndpointVersion(L"11.0.3000.1"));
  EXPECT_TRUE(HasIncompatibleSymantecEndpointVersion(L"11.0.2999.1"));
  EXPECT_TRUE(HasIncompatibleSymantecEndpointVersion(L"10.1.5000.1"));
  EXPECT_TRUE(HasIncompatibleSymantecEndpointVersion(L"9.5.1000.0"));

  EXPECT_FALSE(HasIncompatibleSymantecEndpointVersion(L""));
  EXPECT_FALSE(HasIncompatibleSymantecEndpointVersion(L"11.0.3000"));
  EXPECT_FALSE(HasIncompatibleSymantecEndpointVersion(L"11.0.3000.1.2"));
  EXPECT_FALSE(HasIncompatibleSymantecEndpointVersion(L"11.b.3000.1"));

}
