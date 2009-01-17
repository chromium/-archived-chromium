// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/renderer/user_script_slave.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(UserScriptSlaveTest, EscapeGlob) {
  EXPECT_EQ("", UserScript::EscapeGlob(""));
  EXPECT_EQ("*", UserScript::EscapeGlob("*"));
  EXPECT_EQ("www.google.com", UserScript::EscapeGlob("www.google.com"));
  EXPECT_EQ("*google.com*", UserScript::EscapeGlob("*google.com*"));
  EXPECT_EQ("foo\\\\bar\\?hot=dog",
            UserScript::EscapeGlob("foo\\bar?hot=dog"));
}

TEST(UserScriptSlaveTest, Match1) {
  UserScript script("foo", "bar");
  script.AddInclude("*mail.google.com*");
  script.AddInclude("*mail.yahoo.com*");
  script.AddInclude("*mail.msn.com*");
  EXPECT_TRUE(script.MatchesUrl(GURL("http://mail.google.com")));
  EXPECT_TRUE(script.MatchesUrl(GURL("http://mail.google.com/foo")));
  EXPECT_TRUE(script.MatchesUrl(GURL("http://mail.yahoo.com/bar")));
  EXPECT_TRUE(script.MatchesUrl(GURL("http://mail.msn.com/baz")));
  EXPECT_FALSE(script.MatchesUrl(GURL("http://www.hotmail.com")));
}

TEST(UserScriptSlaveTest, Match2) {
  UserScript script("foo", "bar");
  script.AddInclude("*");
  EXPECT_TRUE(script.MatchesUrl(GURL("http://foo.com/bar")));
  EXPECT_TRUE(script.MatchesUrl(GURL("http://hot.com/dog")));
}

TEST(UserScriptSlaveTest, Match3) {
  UserScript script("foo", "bar");
  script.AddInclude("*foo*");
  EXPECT_TRUE(script.MatchesUrl(GURL("http://foo.com/bar")));
  EXPECT_FALSE(script.MatchesUrl(GURL("http://baz.org")));
}
