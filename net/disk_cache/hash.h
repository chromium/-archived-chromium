// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_HASH_H__
#define NET_DISK_CACHE_HASH_H__

#include <string>

#include "base/basictypes.h"

namespace disk_cache {

// From http://www.azillionmonkeys.com/qed/hash.html
// This is the hash used on WebCore/platform/stringhash
uint32 SuperFastHash(const char * data, int len);

inline uint32 Hash(const char* key, size_t length) {
  return SuperFastHash(key, static_cast<int>(length));
}

inline uint32 Hash(const std::string& key) {
  if (key.empty())
    return 0;
  return SuperFastHash(key.data(), static_cast<int>(key.size()));
}

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_HASH_H__
