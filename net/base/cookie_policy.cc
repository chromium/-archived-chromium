// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/cookie_policy.h"

#include "base/logging.h"
#include "googleurl/src/gurl.h"
#include "net/base/registry_controlled_domain.h"

namespace net {

bool CookiePolicy::CanGetCookies(const GURL& url,
                                 const GURL& first_party_for_cookies) {
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

bool CookiePolicy::CanSetCookie(const GURL& url,
                                const GURL& first_party_for_cookies) {
  switch (type_) {
    case CookiePolicy::ALLOW_ALL_COOKIES:
      return true;
    case CookiePolicy::BLOCK_THIRD_PARTY_COOKIES:
      if (first_party_for_cookies.is_empty())
        return true;  // Empty first-party URL indicates a first-party request.
      return net::RegistryControlledDomainService::SameDomainOrHost(
          url, first_party_for_cookies);
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
