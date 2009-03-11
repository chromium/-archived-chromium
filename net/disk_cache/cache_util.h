// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_CACHE_UTIL_H_
#define NET_DISK_CACHE_CACHE_UTIL_H_

#include <string>

#include "base/basictypes.h"

namespace disk_cache {

// Moves the cache files from the given path to another location.
// Returns true if successful, false otherwise.
bool MoveCache(const std::wstring& from_path, const std::wstring& to_path);

// Deletes the cache files stored on |path|, and optionally also attempts to
// delete the folder itself.
void DeleteCache(const std::wstring& path, bool remove_folder);

// Deletes a cache file.
bool DeleteCacheFile(const std::wstring& name);

// Blocks until |num_pending_io| IO operations complete.
void WaitForPendingIO(int* num_pending_io);

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_CACHE_UTIL_H_
