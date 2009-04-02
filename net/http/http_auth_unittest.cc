// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/ref_counted.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

namespace net {

TEST(HttpAuthTest, ChooseBestChallenge) {
  static const struct {
    const char* headers;
    const char* challenge_realm;
  } tests[] = {
    {
      "Y: Digest realm=\"X\", nonce=\"aaaaaaaaaa\"\n"
      "www-authenticate: Basic realm=\"BasicRealm\"\n",

      // Basic is the only challenge type, pick it.
      "BasicRealm",
    },
    {
      "Y: Digest realm=\"FooBar\", nonce=\"aaaaaaaaaa\"\n"
      "www-authenticate: Fake realm=\"FooBar\"\n",

      // Fake is the only challenge type, but it is unsupported.
      "",
    },
    {
      "www-authenticate: Basic realm=\"FooBar\"\n"
      "www-authenticate: Fake realm=\"FooBar\"\n"
      "www-authenticate: nonce=\"aaaaaaaaaa\"\n"
      "www-authenticate: Digest realm=\"DigestRealm\", nonce=\"aaaaaaaaaa\"\n",

      // Pick Digset over Basic
      "DigestRealm",
    },
    {
      "Y: Digest realm=\"X\", nonce=\"aaaaaaaaaa\"\n"
      "www-authenticate:\n",

      // Handle null header value.
      "",
    }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    // Make a HttpResponseHeaders object.
    std::string headers_with_status_line("HTTP/1.1 401 Unauthorized\n");
    headers_with_status_line += tests[i].headers;
    scoped_refptr<net::HttpResponseHeaders> headers(
       new net::HttpResponseHeaders(
            net::HttpUtil::AssembleRawHeaders(
                headers_with_status_line.c_str(),
                headers_with_status_line.length())));

    scoped_refptr<HttpAuthHandler> handler;
    HttpAuth::ChooseBestChallenge(headers.get(),
                                  HttpAuth::AUTH_SERVER,
                                  &handler);

    if (handler) {
      EXPECT_STREQ(tests[i].challenge_realm, handler->realm().c_str());
    } else {
      EXPECT_STREQ("", tests[i].challenge_realm);
    }
  }
}

TEST(HttpAuthTest, ChooseBestChallengeConnectionBased) {
  static const struct {
    const char* headers;
    const char* challenge_realm;
  } tests[] = {
    {
      "WWW-Authenticate: Negotiate\r\n"
      "WWW-Authenticate: NTLM\r\n",

      // We don't support Negotiate, so pick NTLM.  Either way, realm is
      // empty.
      "",
    },
    {
      "WWW-Authenticate: NTLM "
      "TlRMTVNTUAACAAAADAAMADgAAAAFgokCTroKF1e/DRcAAAAAAAAAALo"
      "AugBEAAAABQEoCgAAAA9HAE8ATwBHAEwARQACAAwARwBPAE8ARwBMAE"
      "UAAQAaAEEASwBFAEUAUwBBAFIAQQAtAEMATwBSAFAABAAeAGMAbwByA"
      "HAALgBnAG8AbwBnAGwAZQAuAGMAbwBtAAMAQABhAGsAZQBlAHMAYQBy"
      "AGEALQBjAG8AcgBwAC4AYQBkAC4AYwBvAHIAcAAuAGcAbwBvAGcAbAB"
      "lAC4AYwBvAG0ABQAeAGMAbwByAHAALgBnAG8AbwBnAGwAZQAuAGMAbw"
      "BtAAAAAAA=\r\n",

      // Realm is empty.
      "",
    }
  };

  scoped_refptr<HttpAuthHandler> handler;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    // Make a HttpResponseHeaders object.
    std::string headers_with_status_line("HTTP/1.1 401 Unauthorized\n");
    headers_with_status_line += tests[i].headers;
    scoped_refptr<net::HttpResponseHeaders> headers(
       new net::HttpResponseHeaders(
            net::HttpUtil::AssembleRawHeaders(
                headers_with_status_line.c_str(),
                headers_with_status_line.length())));

    scoped_refptr<HttpAuthHandler> old_handler = handler;
    HttpAuth::ChooseBestChallenge(headers.get(),
                                  HttpAuth::AUTH_SERVER,
                                  &handler);

    EXPECT_TRUE(handler != NULL);
    // Since NTLM is connection-based, we should continue to use the existing
    // handler rather than creating a new one.
    if (i != 0)
      EXPECT_EQ(old_handler, handler);

    EXPECT_STREQ(tests[i].challenge_realm, handler->realm().c_str());
  }
}

TEST(HttpAuthTest, ChallengeTokenizer) {
  std::string challenge_str = "Basic realm=\"foobar\"";
  HttpAuth::ChallengeTokenizer challenge(challenge_str.begin(),
                                         challenge_str.end());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("Basic"), challenge.scheme());
  EXPECT_TRUE(challenge.GetNext());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("realm"), challenge.name());
  EXPECT_EQ(std::string("foobar"), challenge.unquoted_value());
  EXPECT_EQ(std::string("\"foobar\""), challenge.value());
  EXPECT_TRUE(challenge.value_is_quoted());
  EXPECT_FALSE(challenge.GetNext());
}

// Use a name=value property with no quote marks.
TEST(HttpAuthTest, ChallengeTokenizerNoQuotes) {
  std::string challenge_str = "Basic realm=foobar@baz.com";
  HttpAuth::ChallengeTokenizer challenge(challenge_str.begin(),
                                         challenge_str.end());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("Basic"), challenge.scheme());
  EXPECT_TRUE(challenge.GetNext());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("realm"), challenge.name());
  EXPECT_EQ(std::string("foobar@baz.com"), challenge.value());
  EXPECT_EQ(std::string("foobar@baz.com"), challenge.unquoted_value());
  EXPECT_FALSE(challenge.value_is_quoted());
  EXPECT_FALSE(challenge.GetNext());
}

// Use a name= property which has no value.
TEST(HttpAuthTest, ChallengeTokenizerNoValue) {
  std::string challenge_str = "Digest qop=";
  HttpAuth::ChallengeTokenizer challenge(
      challenge_str.begin(), challenge_str.end());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("Digest"), challenge.scheme());
  EXPECT_TRUE(challenge.GetNext());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("qop"), challenge.name());
  EXPECT_EQ(std::string(""), challenge.value());
  EXPECT_FALSE(challenge.value_is_quoted());
  EXPECT_FALSE(challenge.GetNext());
}

// Specify multiple properties, comma separated.
TEST(HttpAuthTest, ChallengeTokenizerMultiple) {
  std::string challenge_str =
      "Digest algorithm=md5, realm=\"Oblivion\", qop=auth-int";
  HttpAuth::ChallengeTokenizer challenge(challenge_str.begin(),
                                         challenge_str.end());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("Digest"), challenge.scheme());
  EXPECT_TRUE(challenge.GetNext());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("algorithm"), challenge.name());
  EXPECT_EQ(std::string("md5"), challenge.value());
  EXPECT_FALSE(challenge.value_is_quoted());
  EXPECT_TRUE(challenge.GetNext());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("realm"), challenge.name());
  EXPECT_EQ(std::string("Oblivion"), challenge.unquoted_value());
  EXPECT_TRUE(challenge.value_is_quoted());
  EXPECT_TRUE(challenge.GetNext());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("qop"), challenge.name());
  EXPECT_EQ(std::string("auth-int"), challenge.value());
  EXPECT_FALSE(challenge.value_is_quoted());
  EXPECT_FALSE(challenge.GetNext());
}

// Use a challenge which has no property.
TEST(HttpAuthTest, ChallengeTokenizerNoProperty) {
  std::string challenge_str = "NTLM";
  HttpAuth::ChallengeTokenizer challenge(
      challenge_str.begin(), challenge_str.end());
  EXPECT_TRUE(challenge.valid());
  EXPECT_EQ(std::string("NTLM"), challenge.scheme());
  EXPECT_FALSE(challenge.GetNext());
}

TEST(HttpAuthTest, GetChallengeHeaderName) {
  std::string name;

  name = HttpAuth::GetChallengeHeaderName(HttpAuth::AUTH_SERVER);
  EXPECT_STREQ("WWW-Authenticate", name.c_str());

  name = HttpAuth::GetChallengeHeaderName(HttpAuth::AUTH_PROXY);
  EXPECT_STREQ("Proxy-Authenticate", name.c_str());
}

TEST(HttpAuthTest, GetAuthorizationHeaderName) {
  std::string name;

  name = HttpAuth::GetAuthorizationHeaderName(HttpAuth::AUTH_SERVER);
  EXPECT_STREQ("Authorization", name.c_str());

  name = HttpAuth::GetAuthorizationHeaderName(HttpAuth::AUTH_PROXY);
  EXPECT_STREQ("Proxy-Authorization", name.c_str());
}

TEST(HttpAuthTest, CreateAuthHandler) {
  {
    scoped_refptr<HttpAuthHandler> handler;
    HttpAuth::CreateAuthHandler("Basic realm=\"FooBar\"",
                                HttpAuth::AUTH_SERVER,
                                &handler);
    EXPECT_FALSE(handler.get() == NULL);
    EXPECT_STREQ("basic", handler->scheme().c_str());
    EXPECT_STREQ("FooBar", handler->realm().c_str());
    EXPECT_EQ(HttpAuth::AUTH_SERVER, handler->target());
    EXPECT_FALSE(handler->encrypts_identity());
    EXPECT_FALSE(handler->is_connection_based());
  }
  {
    scoped_refptr<HttpAuthHandler> handler;
    HttpAuth::CreateAuthHandler("UNSUPPORTED realm=\"FooBar\"",
                                HttpAuth::AUTH_SERVER,
                                &handler);
    EXPECT_TRUE(handler.get() == NULL);
  }
  {
    scoped_refptr<HttpAuthHandler> handler;
    HttpAuth::CreateAuthHandler("Digest realm=\"FooBar\", nonce=\"xyz\"",
                                HttpAuth::AUTH_PROXY,
                                &handler);
    EXPECT_FALSE(handler.get() == NULL);
    EXPECT_STREQ("digest", handler->scheme().c_str());
    EXPECT_STREQ("FooBar", handler->realm().c_str());
    EXPECT_EQ(HttpAuth::AUTH_PROXY, handler->target());
    EXPECT_TRUE(handler->encrypts_identity());
    EXPECT_FALSE(handler->is_connection_based());
  }
  {
    scoped_refptr<HttpAuthHandler> handler;
    HttpAuth::CreateAuthHandler("NTLM",
                                HttpAuth::AUTH_SERVER,
                                &handler);
    EXPECT_FALSE(handler.get() == NULL);
    EXPECT_STREQ("ntlm", handler->scheme().c_str());
    EXPECT_STREQ("", handler->realm().c_str());
    EXPECT_EQ(HttpAuth::AUTH_SERVER, handler->target());
    EXPECT_TRUE(handler->encrypts_identity());
    EXPECT_TRUE(handler->is_connection_based());
  }
}

}  // namespace net
