// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/auth_cache.h"

#include "base/string_util.h"
#include "googleurl/src/gurl.h"

namespace net {

// Create an AuthCacheKey from url and auth_info.
//
// The cache key is made up of two components, separated by a slash /.
// 1. The host (proxy or server) requesting authentication.  For a server,
//    this component also includes the scheme (protocol) and port (if not
//    the default port for the protocol) to distinguish between multiple
//    servers running on the same computer.
// 2. The realm.
//
// The format of the cache key for proxy auth is:
//     proxy-host/auth-realm
// The format of the cache key for server auth is:
//     url-scheme://url-host[:url-port]/auth-realm

// static
AuthCache::AuthCacheKey AuthCache::HttpKey(
    const GURL& url,
    const AuthChallengeInfo& auth_info) {
  AuthCacheKey auth_cache_key;
  if (auth_info.is_proxy) {
    auth_cache_key = WideToASCII(auth_info.host);
    auth_cache_key.append("/");
  } else {
    // Take scheme, host, and port from the url.
    auth_cache_key = url.GetOrigin().spec();
    // This ends with a "/".
  }
  auth_cache_key.append(WideToUTF8(auth_info.realm));
  return auth_cache_key;
}

AuthData* AuthCache::Lookup(const AuthCacheKey& key) {
  AuthCacheMap::iterator iter = cache_.find(key);
  return (iter == cache_.end()) ? NULL : iter->second;
}

}  // namespace net

