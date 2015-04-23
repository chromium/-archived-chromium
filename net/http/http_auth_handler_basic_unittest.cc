// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/basictypes.h"
#include "net/http/http_auth_handler_basic.h"

namespace net {

TEST(HttpAuthHandlerBasicTest, GenerateCredentials) {
  static const struct {
    const wchar_t* username;
    const wchar_t* password;
    const char* expected_credentials;
  } tests[] = {
    { L"foo", L"bar", "Basic Zm9vOmJhcg==" },
    // Empty username
    { L"", L"foobar", "Basic OmZvb2Jhcg==" },
    // Empty password
    { L"anon", L"", "Basic YW5vbjo=" },
    // Empty username and empty password.
    { L"", L"", "Basic Og==" },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string challenge = "Basic realm=\"Atlantis\"";
    scoped_refptr<HttpAuthHandlerBasic> basic = new HttpAuthHandlerBasic;
    basic->InitFromChallenge(challenge.begin(), challenge.end(),
                             HttpAuth::AUTH_SERVER);
    std::string credentials = basic->GenerateCredentials(tests[i].username,
                                                         tests[i].password,
                                                         NULL, NULL);
    EXPECT_STREQ(tests[i].expected_credentials, credentials.c_str());
  }
}

} // namespace net
