// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_HANDLER_DIGEST_H_
#define NET_HTTP_HTTP_AUTH_HANDLER_DIGEST_H_

#include "net/http/http_auth_handler.h"

// This is needed for the FRIEND_TEST() macro.
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace net {

// Code for handling http digest authentication.
class HttpAuthHandlerDigest : public HttpAuthHandler {
 public:
  virtual std::string GenerateCredentials(const std::wstring& username,
                                          const std::wstring& password,
                                          const HttpRequestInfo* request,
                                          const ProxyInfo* proxy);

 protected:
  virtual bool Init(std::string::const_iterator challenge_begin,
                    std::string::const_iterator challenge_end) {
    nonce_count_ = 0;
    return ParseChallenge(challenge_begin, challenge_end);
  }

 private:
  FRIEND_TEST(HttpAuthHandlerDigestTest, ParseChallenge);
  FRIEND_TEST(HttpAuthHandlerDigestTest, AssembleCredentials);

  // Possible values for the "algorithm" property.
  enum DigestAlgorithm {
    // No algorithm was specified. According to RFC 2617 this means
    // we should default to ALGORITHM_MD5.
    ALGORITHM_UNSPECIFIED,

    // Hashes are run for every request.
    ALGORITHM_MD5,

    // Hash is run only once during the first WWW-Authenticate handshake.
    // (SESS means session).
    ALGORITHM_MD5_SESS,
  };

  // Possible values for "qop" -- may be or-ed together if there were
  // multiple comma separated values.
  enum QualityOfProtection {
    QOP_UNSPECIFIED = 0,
    QOP_AUTH = 1 << 0,
    QOP_AUTH_INT = 1 << 1,
  };

  // Parse the challenge, saving the results into this instance.
  // Returns true on success.
  bool ParseChallenge(std::string::const_iterator challenge_begin,
                      std::string::const_iterator challenge_end);

  // Parse an individual property. Returns true on success.
  bool ParseChallengeProperty(const std::string& name,
                              const std::string& value);

  // Generates a random string, to be used for client-nonce.
  static std::string GenerateNonce();

  // Convert enum value back to string.
  static std::string QopToString(int qop);
  static std::string AlgorithmToString(int algorithm);

  // Extract the method and path of the request, as needed by
  // the 'A2' production. (path may be a hostname for proxy).
  void GetRequestMethodAndPath(const HttpRequestInfo* request,
                               const ProxyInfo* proxy,
                               std::string* method,
                               std::string* path) const;

  // Build up  the 'response' production.
  std::string AssembleResponseDigest(const std::string& method,
                                     const std::string& path,
                                     const std::string& username,
                                     const std::string& password,
                                     const std::string& cnonce,
                                     const std::string& nc) const;

  // Build up  the value for (Authorization/Proxy-Authorization).
  std::string AssembleCredentials(const std::string& method,
                                  const std::string& path,
                                  const std::string& username,
                                  const std::string& password,
                                  const std::string& cnonce,
                                  int nonce_count) const;

  // Information parsed from the challenge.
  std::string nonce_;
  std::string domain_;
  std::string opaque_;
  bool stale_;
  DigestAlgorithm algorithm_;
  int qop_; // Bitfield of QualityOfProtection

  int nonce_count_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_HANDLER_DIGEST_H_
