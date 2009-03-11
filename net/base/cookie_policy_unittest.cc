// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cookie_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "googleurl/src/gurl.h"

class CookiePolicyTest : public testing::Test {
 public:
  CookiePolicyTest()
      : url_google_("http://www.google.izzle"),
        url_google_secure_("https://www.google.izzle"),
        url_google_mail_("http://mail.google.izzle"),
        url_google_analytics_("http://www.googleanalytics.izzle") { }
 protected:
  GURL url_google_;
  GURL url_google_secure_;
  GURL url_google_mail_;
  GURL url_google_analytics_;
};

TEST_F(CookiePolicyTest, DefaultPolicyTest) {
  net::CookiePolicy cp;
  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_secure_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_mail_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_analytics_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, GURL()));

  EXPECT_TRUE(cp.CanSetCookie(url_google_, url_google_));
  EXPECT_TRUE(cp.CanSetCookie(url_google_, url_google_secure_));
  EXPECT_TRUE(cp.CanSetCookie(url_google_, url_google_mail_));
  EXPECT_TRUE(cp.CanSetCookie(url_google_, url_google_analytics_));
  EXPECT_TRUE(cp.CanSetCookie(url_google_, GURL()));
}

TEST_F(CookiePolicyTest, AllowAllCookiesTest) {
  net::CookiePolicy cp;
  cp.SetType(net::CookiePolicy::ALLOW_ALL_COOKIES);

  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_secure_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_mail_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_analytics_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, GURL()));

  EXPECT_TRUE(cp.CanSetCookie(url_google_, url_google_));
  EXPECT_TRUE(cp.CanSetCookie(url_google_, url_google_secure_));
  EXPECT_TRUE(cp.CanSetCookie(url_google_, url_google_mail_));
  EXPECT_TRUE(cp.CanSetCookie(url_google_, url_google_analytics_));
  EXPECT_TRUE(cp.CanSetCookie(url_google_, GURL()));
}

TEST_F(CookiePolicyTest, BlockThirdPartyCookiesTest) {
  net::CookiePolicy cp;
  cp.SetType(net::CookiePolicy::BLOCK_THIRD_PARTY_COOKIES);

  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_secure_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_mail_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_analytics_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, GURL()));

  EXPECT_TRUE(cp.CanSetCookie(url_google_, url_google_));
  EXPECT_TRUE(cp.CanSetCookie(url_google_, url_google_secure_));
  EXPECT_TRUE(cp.CanSetCookie(url_google_, url_google_mail_));
  EXPECT_FALSE(cp.CanSetCookie(url_google_, url_google_analytics_));
  EXPECT_TRUE(cp.CanSetCookie(url_google_, GURL()));
}

TEST_F(CookiePolicyTest, BlockAllCookiesTest) {
  net::CookiePolicy cp;
  cp.SetType(net::CookiePolicy::BLOCK_ALL_COOKIES);

  EXPECT_FALSE(cp.CanGetCookies(url_google_, url_google_));
  EXPECT_FALSE(cp.CanGetCookies(url_google_, url_google_secure_));
  EXPECT_FALSE(cp.CanGetCookies(url_google_, url_google_mail_));
  EXPECT_FALSE(cp.CanGetCookies(url_google_, url_google_analytics_));
  EXPECT_FALSE(cp.CanGetCookies(url_google_, GURL()));

  EXPECT_FALSE(cp.CanSetCookie(url_google_, url_google_));
  EXPECT_FALSE(cp.CanSetCookie(url_google_, url_google_secure_));
  EXPECT_FALSE(cp.CanSetCookie(url_google_, url_google_mail_));
  EXPECT_FALSE(cp.CanSetCookie(url_google_, url_google_analytics_));
  EXPECT_FALSE(cp.CanSetCookie(url_google_, GURL()));
}
