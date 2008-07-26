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

#ifndef NET_BASE_COOKIE_POLICY_H__
#define NET_BASE_COOKIE_POLICY_H__

#include "googleurl/src/gurl.h"

// The CookiePolicy class implements third-party cookie blocking.
class CookiePolicy {
 public:
  // Consult the user's third-party cookie blocking preferences to determine
  // whether the URL's cookies can be read if the top-level window is policy_url
  bool CanGetCookies(const GURL& url, const GURL& policy_url);

  // Consult the user's third-party cookie blocking preferences to determine
  // whether the URL's cookies can be set if the top-level window is policy_url
  bool CanSetCookie(const GURL& url, const GURL& policy_url);

  enum Type {
    ALLOW_ALL_COOKIES = 0,      // do not perform any cookie blocking
    BLOCK_THIRD_PARTY_COOKIES,  // prevent third-party cookies from being sent
    BLOCK_ALL_COOKIES           // disable cookies
  };

  static bool ValidType(int32 type) {
    return type >= ALLOW_ALL_COOKIES && type <= BLOCK_ALL_COOKIES;
  }

  static Type FromInt(int32 type) {
    return static_cast<Type>(type);
  }

  // Sets the current policy to enforce. This should be called when the user's
  // preferences change.
  void SetType(Type type) { type_ = type; }

  CookiePolicy();

 private:
  Type type_;

  DISALLOW_EVIL_CONSTRUCTORS(CookiePolicy);
};

#endif // NET_BASE_COOKIE_POLICY_H__
