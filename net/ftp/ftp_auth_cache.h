// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FTP_FTP_AUTH_CACHE_H_
#define NET_FTP_FTP_AUTH_CACHE_H_

#include <string>
#include <map>

#include "net/base/auth.h"

class GURL;

namespace net {

// The FtpAuthCache class is a simple cache structure to store authentication
// information for ftp. Provides lookup, insertion, and deletion of entries.
// The parameter for doing lookups, insertions, and deletions is a GURL of the
// server's address (not a full URL with path, since FTP auth isn't per path).
// For example:
//   GURL("ftp://myserver") -- OK (implied port of 21)
//   GURL("ftp://myserver:21") -- OK
//   GURL("ftp://myserver/PATH") -- WRONG, paths not allowed
class FtpAuthCache {
 public:
  FtpAuthCache() {}
  ~FtpAuthCache() {}

  // Check if we have authentication data for ftp server at |origin|.
  // Returns the address of corresponding AuthData object (if found) or NULL
  // (if not found).
  AuthData* Lookup(const GURL& origin);

  // Add an entry for |origin| to the cache. If there is already an
  // entry for |origin|, it will be overwritten. Both parameters are IN only.
  void Add(const GURL& origin, AuthData* value);

  // Remove the entry for |origin| from the cache, if one exists.
  void Remove(const GURL& origin);

 private:
  typedef std::string AuthCacheKey;
  typedef scoped_refptr<AuthData> AuthCacheValue;
  typedef std::map<AuthCacheKey,AuthCacheValue> AuthCacheMap;

  // Get the key in hash table |cache_| where entries for ftp server |origin|
  // should be saved.
  static AuthCacheKey MakeKey(const GURL& origin);

  // internal representation of cache, an STL map.
  AuthCacheMap cache_;
};

}  // namespace net

#endif  // NET_FTP_FTP_AUTH_CACHE_H_

