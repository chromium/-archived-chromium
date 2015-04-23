// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_config_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using base::TimeTicks;

namespace {

class SSLConfigServiceTest : public testing::Test {
};

} // namespace

TEST(SSLConfigServiceTest, GetNowTest) {
  // Verify that the constructor sets the correct default values.
  net::SSLConfig config;
  EXPECT_EQ(false, config.rev_checking_enabled);
  EXPECT_EQ(false, config.ssl2_enabled);
  EXPECT_EQ(true, config.ssl3_enabled);
  EXPECT_EQ(true, config.tls1_enabled);

  bool rv = net::SSLConfigService::GetSSLConfigNow(&config);
  EXPECT_TRUE(rv);
}

TEST(SSLConfigServiceTest, SetTest) {
  // Save the current settings so we can restore them after the tests.
  net::SSLConfig config_save;
  bool rv = net::SSLConfigService::GetSSLConfigNow(&config_save);
  EXPECT_TRUE(rv);

  net::SSLConfig config;

  // Test SetRevCheckingEnabled.
  net::SSLConfigService::SetRevCheckingEnabled(true);
  rv = net::SSLConfigService::GetSSLConfigNow(&config);
  EXPECT_TRUE(rv);
  EXPECT_TRUE(config.rev_checking_enabled);

  net::SSLConfigService::SetRevCheckingEnabled(false);
  rv = net::SSLConfigService::GetSSLConfigNow(&config);
  EXPECT_TRUE(rv);
  EXPECT_FALSE(config.rev_checking_enabled);

  net::SSLConfigService::SetRevCheckingEnabled(
      config_save.rev_checking_enabled);

  // Test SetSSL2Enabled.
  net::SSLConfigService::SetSSL2Enabled(true);
  rv = net::SSLConfigService::GetSSLConfigNow(&config);
  EXPECT_TRUE(rv);
  EXPECT_TRUE(config.ssl2_enabled);

  net::SSLConfigService::SetSSL2Enabled(false);
  rv = net::SSLConfigService::GetSSLConfigNow(&config);
  EXPECT_TRUE(rv);
  EXPECT_FALSE(config.ssl2_enabled);

  net::SSLConfigService::SetSSL2Enabled(config_save.ssl2_enabled);
}

TEST(SSLConfigServiceTest, GetTest) {
  TimeTicks now = TimeTicks::Now();
  TimeTicks now_1 = now + TimeDelta::FromSeconds(1);
  TimeTicks now_11 = now + TimeDelta::FromSeconds(11);

  net::SSLConfig config, config_1, config_11;
  net::SSLConfigService config_service(now);
  config_service.GetSSLConfigAt(&config, now);

  // Flip rev_checking_enabled.
  net::SSLConfigService::SetRevCheckingEnabled(!config.rev_checking_enabled);

  config_service.GetSSLConfigAt(&config_1, now_1);
  EXPECT_EQ(config.rev_checking_enabled, config_1.rev_checking_enabled);

  config_service.GetSSLConfigAt(&config_11, now_11);
  EXPECT_EQ(!config.rev_checking_enabled, config_11.rev_checking_enabled);

  // Restore the original value.
  net::SSLConfigService::SetRevCheckingEnabled(config.rev_checking_enabled);
}
