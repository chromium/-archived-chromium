// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/login_prompt.h"
#include "googleurl/src/gurl.h"
#include "net/base/auth.h"
#include "testing/gtest/include/gtest/gtest.h"


TEST(LoginPromptTest, GetSignonRealm) {
  scoped_refptr<net::AuthChallengeInfo> auth_info = new net::AuthChallengeInfo;
  auth_info->is_proxy = false;  // server auth
  // auth_info->host is intentionally left empty.
  auth_info->scheme = L"Basic";
  auth_info->realm = L"WallyWorld";

  std::string url[] = {
    "https://www.nowhere.org/dir/index.html",
    "https://www.nowhere.org:443/dir/index.html",  // default port
    "https://www.nowhere.org:8443/dir/index.html",  // non-default port
    "https://www.nowhere.org",  // no trailing slash
    "https://foo:bar@www.nowhere.org/dir/index.html",  // username:password
    "https://www.nowhere.org/dir/index.html?id=965362",  // query
    "https://www.nowhere.org/dir/index.html#toc",  // reference
  };

  std::string expected[] = {
    "https://www.nowhere.org/WallyWorld",
    "https://www.nowhere.org/WallyWorld",
    "https://www.nowhere.org:8443/WallyWorld",
    "https://www.nowhere.org/WallyWorld",
    "https://www.nowhere.org/WallyWorld",
    "https://www.nowhere.org/WallyWorld",
    "https://www.nowhere.org/WallyWorld"
  };

  for (size_t i = 0; i < arraysize(url); i++) {
    std::string key = GetSignonRealm(GURL(url[i]), *auth_info);
    EXPECT_EQ(expected[i], key);
  }
}
