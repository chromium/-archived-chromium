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

#include "net/base/cookie_policy.h"

#include "base/logging.h"
#include "googleurl/src/gurl.h"
#include "net/base/registry_controlled_domain.h"

namespace net {

bool CookiePolicy::CanGetCookies(const GURL& url, const GURL& policy_url) {
  switch (type_) {
    case CookiePolicy::ALLOW_ALL_COOKIES:
      return true;
    case CookiePolicy::BLOCK_THIRD_PARTY_COOKIES:
      return true;
    case CookiePolicy::BLOCK_ALL_COOKIES:
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

bool CookiePolicy::CanSetCookie(const GURL& url, const GURL& policy_url) {
  switch (type_) {
    case CookiePolicy::ALLOW_ALL_COOKIES:
      return true;
    case CookiePolicy::BLOCK_THIRD_PARTY_COOKIES:
      if (policy_url.is_empty())
        return true;  // Empty policy URL should indicate a first-party request

      return net::RegistryControlledDomainService::SameDomainOrHost(url, policy_url);
    case CookiePolicy::BLOCK_ALL_COOKIES:
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

CookiePolicy::CookiePolicy() : type_(CookiePolicy::ALLOW_ALL_COOKIES) {
}

}  // namespace net
