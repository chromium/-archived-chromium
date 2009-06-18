// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ssl_client_auth_cache.h"

namespace net {

X509Certificate* SSLClientAuthCache::Lookup(const std::string& server) {
  AuthCacheMap::iterator iter = cache_.find(server);
  return (iter == cache_.end()) ? NULL : iter->second;
}

void SSLClientAuthCache::Add(const std::string& server,
                             X509Certificate* value) {
  cache_[server] = value;

  // TODO(wtc): enforce a maximum number of entries.
}

void SSLClientAuthCache::Remove(const std::string& server) {
  cache_.erase(server);
}

}  // namespace net
