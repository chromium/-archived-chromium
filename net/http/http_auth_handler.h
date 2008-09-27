// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_HANDLER_H_
#define NET_HTTP_HTTP_AUTH_HANDLER_H_

#include <string>

#include "net/http/http_auth.h"

namespace net {

class HttpRequestInfo;
class ProxyInfo;

// HttpAuthHandler is the interface for the authentication schemes
// (basic, digest, ...)
// The registry mapping auth-schemes to implementations is hardcoded in 
// HttpAuth::CreateAuthHandler().
class HttpAuthHandler {
 public:
  // Initialize the handler by parsing a challenge string.
  bool InitFromChallenge(std::string::const_iterator begin,
                         std::string::const_iterator end,
                         HttpAuth::Target target);

  // Lowercase name of the auth scheme
  virtual std::string scheme() const {
    return scheme_;
  }

  // The realm value that was parsed during Init().
  std::string realm() const {
    return realm_;
  }

  // Numeric rank based on the challenge's security level. Higher
  // numbers are better. Used by HttpAuth::ChooseBestChallenge().
  int score() const {
    return score_;
  }

  HttpAuth::Target target() const {
    return target_;
  }
 
  // Generate the Authorization header value.
  virtual std::string GenerateCredentials(const std::wstring& username,
                                          const std::wstring& password,
                                          const HttpRequestInfo* request,
                                          const ProxyInfo* proxy) = 0;

 protected:
   // Initialize the handler by parsing a challenge string.
   // Implementations are expcted to initialize the following members:
   // score_, realm_, scheme_
  virtual bool Init(std::string::const_iterator challenge_begin,
                    std::string::const_iterator challenge_end) = 0;

  // The lowercase auth-scheme {"basic", "digest", "ntlm", ...}
  const char* scheme_;

  // The realm.
  std::string realm_;

  // The score for this challenge. Higher numbers are better.
  int score_;

  // Whether this authentication request is for a proxy server, or an
  // origin server.
  HttpAuth::Target target_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_HANDLER_H_
