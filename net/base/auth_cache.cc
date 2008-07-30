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
