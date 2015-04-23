// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "base/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(BlacklistTest, Generic) {
  FilePath path;
  Blacklist blacklist(path);

  // Empty blacklist should not match any URL.
  EXPECT_FALSE(blacklist.findMatch(GURL()));
  EXPECT_FALSE(blacklist.findMatch(GURL("http://www.google.com")));

  std::string cookie1(
      "PREF=ID=14a549990453e42a:TM=1245183232:LM=1245183232:S=Occ7khRVIEE36Ao5;"
      " expires=Thu, 16-Jun-2011 20:13:52 GMT; path=/; domain=.google.com");
  std::string cookie2(
      "PREF=ID=14a549990453e42a:TM=1245183232:LM=1245183232:S=Occ7khRVIEE36Ao5;"
      " path=/; domain=.google.com");

  // No expiry, should be equal to itself after stripping.
  EXPECT_TRUE(cookie2 == Blacklist::StripCookieExpiry(cookie2));

  // Expiry, should be equal to non-expiry version after stripping.
  EXPECT_TRUE(cookie2 == Blacklist::StripCookieExpiry(cookie1));
}
