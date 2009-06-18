// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_CLIENT_AUTH_CACHE_H_
#define NET_BASE_SSL_CLIENT_AUTH_CACHE_H_

#include <string>
#include <map>

#include "base/ref_counted.h"
#include "net/base/x509_certificate.h"

namespace net {

// The SSLClientAuthCache class is a simple cache structure to store SSL
// client certificates. Provides lookup, insertion, and deletion of entries.
// The parameter for doing lookups, insertions, and deletions is the server's
// host and port.
//
// TODO(wtc): This class is based on FtpAuthCache.  We can extract the common
// code to a template class.
class SSLClientAuthCache {
 public:
  SSLClientAuthCache() {}
  ~SSLClientAuthCache() {}

  // Check if we have a client certificate for SSL server at |server|.
  // Returns the client certificate (if found) or NULL (if not found).
  X509Certificate* Lookup(const std::string& server);

  // Add a client certificate for |server| to the cache. If there is already
  // a client certificate for |server|, it will be overwritten. Both parameters
  // are IN only.
  void Add(const std::string& server, X509Certificate* client_cert);

  // Remove the client certificate for |server| from the cache, if one exists.
  void Remove(const std::string& server);

 private:
  typedef std::string AuthCacheKey;
  typedef scoped_refptr<X509Certificate> AuthCacheValue;
  typedef std::map<AuthCacheKey, AuthCacheValue> AuthCacheMap;

  // internal representation of cache, an STL map.
  AuthCacheMap cache_;
};

}  // namespace net

#endif  // NET_BASE_SSL_CLIENT_AUTH_CACHE_H_
