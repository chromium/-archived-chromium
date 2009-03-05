// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/basictypes.h"
#include "net/http/http_auth_handler_digest.h"

namespace net {

TEST(HttpAuthHandlerDigestTest, ParseChallenge) {
  static const struct {
    // The challenge string.
    const char* challenge;
    // Expected return value of ParseChallenge.
    bool parsed_success;
    // The expected values that were parsed.
    const char* parsed_realm;
    const char* parsed_nonce;
    const char* parsed_domain;
    const char* parsed_opaque;
    bool parsed_stale;
    int parsed_algorithm;
    int parsed_qop;
  } tests[] = {
    {
      "Digest nonce=\"xyz\", realm=\"Thunder Bluff\"",
      true,
      "Thunder Bluff",
      "xyz",
      "",
      "",
      false,
      HttpAuthHandlerDigest::ALGORITHM_UNSPECIFIED,
      HttpAuthHandlerDigest::QOP_UNSPECIFIED
    },

    {// Check that when algorithm has an unsupported value, parsing fails.
      "Digest nonce=\"xyz\", algorithm=\"awezum\", realm=\"Thunder\"",
      false,
      // The remaining values don't matter (but some have been set already).
      "",
      "xyz",
      "",
      "",
      false,
      HttpAuthHandlerDigest::ALGORITHM_UNSPECIFIED,
      HttpAuthHandlerDigest::QOP_UNSPECIFIED
    },

    { // Check that algorithm's value is case insensitive.
      "Digest nonce=\"xyz\", algorithm=\"mD5\", realm=\"Oblivion\"",
      true,
      "Oblivion",
      "xyz",
      "",
      "",
      false,
      HttpAuthHandlerDigest::ALGORITHM_MD5,
      HttpAuthHandlerDigest::QOP_UNSPECIFIED
    },

    { // Check that md5-sess is recognized, as is single QOP
      "Digest nonce=\"xyz\", algorithm=\"md5-sess\", "
          "realm=\"Oblivion\", qop=\"auth\"",
      true,
      "Oblivion",
      "xyz",
      "",
      "",
      false,
      HttpAuthHandlerDigest::ALGORITHM_MD5_SESS,
      HttpAuthHandlerDigest::QOP_AUTH
    },

    { // The realm can't be missing.
      "Digest nonce=\"xyz\"",
      false, // FAILED parse.
      "",
      "xyz",
      "",
      "",
      false,
      HttpAuthHandlerDigest::ALGORITHM_UNSPECIFIED,
      HttpAuthHandlerDigest::QOP_UNSPECIFIED
    }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string challenge(tests[i].challenge);

    scoped_refptr<HttpAuthHandlerDigest> digest = new HttpAuthHandlerDigest;
    bool ok = digest->ParseChallenge(challenge.begin(), challenge.end());

    EXPECT_EQ(tests[i].parsed_success, ok);
    EXPECT_STREQ(tests[i].parsed_realm, digest->realm_.c_str());
    EXPECT_STREQ(tests[i].parsed_nonce, digest->nonce_.c_str());
    EXPECT_STREQ(tests[i].parsed_domain, digest->domain_.c_str());
    EXPECT_STREQ(tests[i].parsed_opaque, digest->opaque_.c_str());
    EXPECT_EQ(tests[i].parsed_stale, digest->stale_);
    EXPECT_EQ(tests[i].parsed_algorithm, digest->algorithm_);
    EXPECT_EQ(tests[i].parsed_qop, digest->qop_);
  }
}

TEST(HttpAuthHandlerDigestTest, AssembleCredentials) {
  static const struct {
    const char* req_method;
    const char* req_path;
    const char* challenge;
    const char* username;
    const char* password;
    const char* cnonce;
    int nonce_count;
    const char* expected_creds;
  } tests[] = {
    { // MD5 with username/password
      "GET",
      "/test/drealm1",

      // Challenge
      "Digest realm=\"DRealm1\", "
      "nonce=\"claGgoRXBAA=7583377687842fdb7b56ba0555d175baa0b800e3\", "
      "algorithm=MD5, qop=\"auth\"",

      "foo", "bar", // username/password
      "082c875dcb2ca740", // cnonce
      1, // nc

      // Authorization
      "Digest username=\"foo\", realm=\"DRealm1\", "
      "nonce=\"claGgoRXBAA=7583377687842fdb7b56ba0555d175baa0b800e3\", "
      "uri=\"/test/drealm1\", algorithm=MD5, "
      "response=\"bcfaa62f1186a31ff1b474a19a17cf57\", "
      "qop=auth, nc=00000001, cnonce=\"082c875dcb2ca740\""
    },

    { // MD5 with username but empty password. username has space in it.
      "GET",
      "/test/drealm1/",

      // Challenge
      "Digest realm=\"DRealm1\", "
      "nonce=\"Ure30oRXBAA=7eca98bbf521ac6642820b11b86bd2d9ed7edc70\", "
      "algorithm=MD5, qop=\"auth\"",

      "foo bar", "", // Username/password
      "082c875dcb2ca740", // cnonce
      1, // nc

      // Authorization
      "Digest username=\"foo bar\", realm=\"DRealm1\", "
      "nonce=\"Ure30oRXBAA=7eca98bbf521ac6642820b11b86bd2d9ed7edc70\", "
      "uri=\"/test/drealm1/\", algorithm=MD5, "
      "response=\"93c9c6d5930af3b0eb26c745e02b04a0\", "
      "qop=auth, nc=00000001, cnonce=\"082c875dcb2ca740\""
    },

    { // MD5 with no username.
      "GET",
      "/test/drealm1/",

      // Challenge
      "Digest realm=\"DRealm1\", "
      "nonce=\"7thGplhaBAA=41fb92453c49799cf353c8cd0aabee02d61a98a8\", "
      "algorithm=MD5, qop=\"auth\"",

      "", "pass", // Username/password
      "6509bc74daed8263", // cnonce
      1, // nc

      // Authorization
      "Digest username=\"\", realm=\"DRealm1\", "
      "nonce=\"7thGplhaBAA=41fb92453c49799cf353c8cd0aabee02d61a98a8\", "
      "uri=\"/test/drealm1/\", algorithm=MD5, "
      "response=\"bc597110f41a62d07f8b70b6977fcb61\", "
      "qop=auth, nc=00000001, cnonce=\"6509bc74daed8263\""
    },

    { // MD5 with no username and no password.
      "GET",
      "/test/drealm1/",

      // Challenge
      "Digest realm=\"DRealm1\", "
      "nonce=\"s3MzvFhaBAA=4c520af5acd9d8d7ae26947529d18c8eae1e98f4\", "
      "algorithm=MD5, qop=\"auth\"",

      "", "", // Username/password
      "1522e61005789929", // cnonce
      1, // nc

      // Authorization
      "Digest username=\"\", realm=\"DRealm1\", "
      "nonce=\"s3MzvFhaBAA=4c520af5acd9d8d7ae26947529d18c8eae1e98f4\", "
      "uri=\"/test/drealm1/\", algorithm=MD5, "
      "response=\"22cfa2b30cb500a9591c6d55ec5590a8\", "
      "qop=auth, nc=00000001, cnonce=\"1522e61005789929\""
    },

    { // No algorithm, and no qop.
      "GET",
      "/",

      // Challenge
      "Digest realm=\"Oblivion\", nonce=\"nonce-value\"",

      "FooBar", "pass", // Username/password
      "", // cnonce
      1, // nc

      // Authorization
      "Digest username=\"FooBar\", realm=\"Oblivion\", "
      "nonce=\"nonce-value\", uri=\"/\", "
      "response=\"f72ff54ebde2f928860f806ec04acd1b\""
    },

    { // MD5-sess
      "GET",
      "/",

      // Challenge
      "Digest realm=\"Baztastic\", nonce=\"AAAAAAAA\", "
      "algorithm=\"md5-sess\", qop=auth",

      "USER", "123", // Username/password
      "15c07961ed8575c4", // cnonce
      1, // nc

      // Authorization
      "Digest username=\"USER\", realm=\"Baztastic\", "
      "nonce=\"AAAAAAAA\", uri=\"/\", algorithm=MD5-sess, "
      "response=\"cbc1139821ee7192069580570c541a03\", "
      "qop=auth, nc=00000001, cnonce=\"15c07961ed8575c4\""
    }
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    scoped_refptr<HttpAuthHandlerDigest> digest = new HttpAuthHandlerDigest;
    std::string challenge = tests[i].challenge;
    EXPECT_TRUE(digest->InitFromChallenge(
        challenge.begin(), challenge.end(), HttpAuth::AUTH_SERVER));

    std::string creds = digest->AssembleCredentials(tests[i].req_method,
        tests[i].req_path, tests[i].username, tests[i].password,
        tests[i].cnonce, tests[i].nonce_count);

    EXPECT_STREQ(tests[i].expected_creds, creds.c_str());
  }
}

} // namespace net
