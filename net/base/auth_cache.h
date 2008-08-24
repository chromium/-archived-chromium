// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_AUTH_CACHE_H__
#define NET_BASE_AUTH_CACHE_H__

#include <string>
#include <map>

#include "net/base/auth.h"

class GURL;

namespace net {

// The AuthCache class is a simple cache structure to store authentication
// information for ftp or http/https sites. Provides lookup, addition, and
// validation of entries.
class AuthCache {
 public:
  AuthCache() {}
  ~AuthCache() {}

  typedef std::string AuthCacheKey;

  // Return the key for looking up the auth data in the auth cache for HTTP,
  // consisting of the scheme, host, and port of the request URL and the
  // realm in the auth challenge.
  static AuthCacheKey HttpKey(const GURL& url,
                              const AuthChallengeInfo& auth_info);

  // Check if we have authentication data for given key.  The key parameter
  // is input, consisting of the hostname and any other info (such as realm)
  // appropriate for the protocol.  Return the address of corresponding
  // AuthData object (if found) or NULL (if not found).
  AuthData* Lookup(const AuthCacheKey& key);

  // Add to the cache. If key already exists, this will overwrite. Both
  // parameters are IN only.
  void Add(const AuthCacheKey& key, AuthData* value) {
    cache_[key] = value;
  }

  // Called when we have an auth failure to remove
  // the likely invalid credentials.
  void Remove(const AuthCacheKey& key) {
    cache_.erase(key);
  }

 private:
  typedef scoped_refptr<AuthData> AuthCacheValue;
  typedef std::map<AuthCacheKey,AuthCacheValue> AuthCacheMap;

  // internal representation of cache, an STL map.
  AuthCacheMap cache_;
};

}  // namespace net

#endif  // NET_BASE_AUTH_CACHE_H__

