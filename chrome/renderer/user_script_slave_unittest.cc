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

TEST(UserScriptSlaveTest, Parse1) {
  const std::string text(
    "// This is my awesome script\n"
    "// It does stuff.\n"
    "// ==UserScript==   trailing garbage\n"
    "// @name foobar script\n"
    "// @namespace http://www.google.com/\n"
    "// @include *mail.google.com*\n"
    "// \n"
    "// @othergarbage\n"
    "// @include *mail.yahoo.com*\r\n"
    "// @include  \t *mail.msn.com*\n" // extra spaces after "@include" OK
    "//@include not-recognized\n" // must have one space after "//"
    "// ==/UserScript==  trailing garbage\n"
    "\n"
    "\n"
    "alert('hoo!');\n");

  UserScript script("foo");
  script.Parse(text);
  EXPECT_EQ(3U, script.include_patterns_.size());
  EXPECT_EQ(text, script.GetBody());
  EXPECT_TRUE(script.MatchesUrl(GURL("http://mail.google.com")));
  EXPECT_TRUE(script.MatchesUrl(GURL("http://mail.google.com/foo")));
  EXPECT_TRUE(script.MatchesUrl(GURL("http://mail.yahoo.com/bar")));
  EXPECT_TRUE(script.MatchesUrl(GURL("http://mail.msn.com/baz")));
  EXPECT_FALSE(script.MatchesUrl(GURL("http://www.hotmail.com")));
}

TEST(UserScriptSlaveTest, Parse2) {
  const std::string text("default to @include *");

  UserScript script("foo");
  script.Parse(text);
  EXPECT_EQ(1U, script.include_patterns_.size());
  EXPECT_EQ(text, script.GetBody());
  EXPECT_TRUE(script.MatchesUrl(GURL("foo")));
  EXPECT_TRUE(script.MatchesUrl(GURL("bar")));
}

TEST(UserScriptSlaveTest, Parse3) {
  const std::string text(
    "// ==UserScript==\n"
    "// @include *foo*\n"
    "// ==/UserScript=="); // no trailing newline

  UserScript script("foo");
  script.Parse(text);
  EXPECT_EQ(1U, script.include_patterns_.size());
  EXPECT_EQ(text, script.GetBody());
  EXPECT_TRUE(script.MatchesUrl(GURL("http://foo.com/bar")));
  EXPECT_FALSE(script.MatchesUrl(GURL("http://baz.org")));
}
