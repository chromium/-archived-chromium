// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_HANDLER_H_
#define NET_HTTP_HTTP_AUTH_HANDLER_H_

#include <string>

#include "base/ref_counted.h"
#include "net/http/http_auth.h"

namespace net {

class HttpRequestInfo;
class ProxyInfo;

// HttpAuthHandler is the interface for the authentication schemes
// (basic, digest, ...)
// The registry mapping auth-schemes to implementations is hardcoded in
// HttpAuth::CreateAuthHandler().
class HttpAuthHandler : public base::RefCounted<HttpAuthHandler> {
 public:
  virtual ~HttpAuthHandler() { }

  // Initialize the handler by parsing a challenge string.
  bool InitFromChallenge(std::string::const_iterator begin,
                         std::string::const_iterator end,
                         HttpAuth::Target target);

  // Lowercase name of the auth scheme
  const std::string& scheme() const {
    return scheme_;
  }

  // The realm value that was parsed during Init().
  const std::string& realm() const {
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

  // Returns true if the authentication scheme does not send the username and
  // password in the clear.
  bool encrypts_identity() const {
    return (properties_ & ENCRYPTS_IDENTITY) != 0;
  }

  // Returns true if the authentication scheme is connection-based, for
  // example, NTLM.  A connection-based authentication scheme does not support
  // preemptive authentication, and must use the same handler object
  // throughout the life of an HTTP transaction.
  bool is_connection_based() const {
    return (properties_ & IS_CONNECTION_BASED) != 0;
  }

  // Returns true if the response to the current authentication challenge
  // requires an identity.
  // TODO(wtc): Find a better way to handle a multi-round challenge-response
  // sequence used by a connection-based authentication scheme.
  virtual bool NeedsIdentity() { return true; }

  // Generate the Authorization header value.
  virtual std::string GenerateCredentials(const std::wstring& username,
                                          const std::wstring& password,
                                          const HttpRequestInfo* request,
                                          const ProxyInfo* proxy) = 0;

 protected:
  enum Property {
    ENCRYPTS_IDENTITY = 1 << 0,
    IS_CONNECTION_BASED = 1 << 1,
  };

  // Initialize the handler by parsing a challenge string.
  // Implementations are expcted to initialize the following members:
  // scheme_, realm_, score_, properties_
  virtual bool Init(std::string::const_iterator challenge_begin,
                    std::string::const_iterator challenge_end) = 0;

  // The lowercase auth-scheme {"basic", "digest", "ntlm", ...}
  std::string scheme_;

  // The realm.
  std::string realm_;

  // The score for this challenge. Higher numbers are better.
  int score_;

  // Whether this authentication request is for a proxy server, or an
  // origin server.
  HttpAuth::Target target_;

  // A bitmask of the properties of the authentication scheme.
  int properties_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_HANDLER_H_
