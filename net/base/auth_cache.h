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
