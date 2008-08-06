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

#ifndef NET_BASE_AUTH_H__
#define NET_BASE_AUTH_H__

#include <string>

#include "base/ref_counted.h"

namespace net {

// Holds info about an authentication challenge that we may want to display
// to the user.
class AuthChallengeInfo :
    public base::RefCountedThreadSafe<AuthChallengeInfo> {
 public:
  bool is_proxy;  // true for Proxy-Authenticate, false for WWW-Authenticate.
  std::wstring host;  // the domain name of the server asking for auth
  // (could be the proxy).
  std::wstring scheme;  // "Basic", "Digest", or whatever other method is used.
  std::wstring realm;  // the realm provided by the server, if there is one.

 private:
  friend class base::RefCountedThreadSafe<AuthChallengeInfo>;
  ~AuthChallengeInfo() {}
};

//Authentication structures
enum AuthState {
  AUTH_STATE_DONT_NEED_AUTH,
  AUTH_STATE_NEED_AUTH,
  AUTH_STATE_HAVE_AUTH,
  AUTH_STATE_CANCELED
};

class AuthData : public base::RefCountedThreadSafe<AuthData> {
 public:
  AuthState state;  // whether we need, have, or gave up on authentication.
  std::wstring scheme;  // the authentication scheme.
  std::wstring username;  // the username supplied to us for auth.
  std::wstring password;  // the password supplied to us for auth.

  // We wouldn't instantiate this class if we didn't need authentication.
  AuthData() : state(AUTH_STATE_NEED_AUTH) {}

 private:
  friend class base::RefCountedThreadSafe<AuthData>;
  ~AuthData() {}
};

}  // namespace net

#endif  // NET_BASE_AUTH_H__
