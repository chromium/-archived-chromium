// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_auth_cache.h"

#include "base/string_util.h"
#include "googleurl/src/gurl.h"

namespace net {

AuthData* FtpAuthCache::Lookup(const GURL& origin) {
  AuthCacheMap::iterator iter = cache_.find(MakeKey(origin));
  return (iter == cache_.end()) ? NULL : iter->second;
}

void FtpAuthCache::Add(const GURL& origin, AuthData* value) {
  cache_[MakeKey(origin)] = value;

  // TODO(eroman): enforce a maximum number of entries.
}

void FtpAuthCache::Remove(const GURL& origin) {
  cache_.erase(MakeKey(origin));
}

// static
FtpAuthCache::AuthCacheKey FtpAuthCache::MakeKey(const GURL& origin) {
  DCHECK(origin.SchemeIs("ftp"));
  DCHECK(origin.GetOrigin() == origin);
  return origin.spec();
}

}  // namespace net

