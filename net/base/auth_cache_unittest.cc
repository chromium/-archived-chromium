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

#include "googleurl/src/gurl.h"
#include "net/base/auth_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class AuthCacheTest : public testing::Test {
};

}  // namespace

TEST(AuthCacheTest, HttpKey) {
  scoped_refptr<AuthChallengeInfo> auth_info = new AuthChallengeInfo;
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

  for (int i = 0; i < arraysize(url); i++) {
    std::string key = AuthCache::HttpKey(GURL(url[i]), *auth_info);
    EXPECT_EQ(expected[i], key);
  }
}
