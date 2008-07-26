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

#include "net/base/ssl_config_service.h"
#include "testing/gtest/include/gtest/gtest.h"

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
