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

#include "net/base/cookie_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

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

}  // namespace

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