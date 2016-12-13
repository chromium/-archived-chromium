// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Error codes reported by self tests or to UMA.

#ifndef NET_DISK_CACHE_ERRORS_H__
#define NET_DISK_CACHE_ERRORS_H__

namespace disk_cache {

enum {
  ERR_INIT_FAILED = -1,
  ERR_INVALID_TAIL = -2,
  ERR_INVALID_HEAD = -3,
  ERR_INVALID_PREV = -4,
  ERR_INVALID_NEXT = -5,
  ERR_INVALID_ENTRY = -6,
  ERR_INVALID_ADDRESS = -7,
  ERR_INVALID_LINKS = -8,
  ERR_NUM_ENTRIES_MISMATCH = -9,
  ERR_READ_FAILURE = -10,
  ERR_PREVIOUS_CRASH = -11,
  ERR_STORAGE_ERROR = -12,
  ERR_INVALID_MASK = -13
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_ERRORS_H__
