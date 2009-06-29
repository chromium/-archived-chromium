// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_HOST_CACHE_H_
#define NET_BASE_HOST_CACHE_H_

#include <string>

#include "base/hash_tables.h"
#include "base/ref_counted.h"
#include "base/time.h"
#include "net/base/address_list.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace net {

// Cache used by HostResolver to map hostnames to their resolved result.
class HostCache {
 public:
  // Stores the latest address list that was looked up for a hostname.
  struct Entry : public base::RefCounted<Entry> {
    Entry(int error, const AddressList& addrlist, base::TimeTicks expiration);
    ~Entry();

    // The resolve results for this entry.
    int error;
    AddressList addrlist;

    // The time when this entry expires.
    base::TimeTicks expiration;
  };

  // Constructs a HostCache whose entries are valid for |cache_duration_ms|
  // milliseconds. The cache will store up to |max_entries|.
  HostCache(size_t max_entries, size_t cache_duration_ms);

  ~HostCache();

  // Returns a pointer to the entry for |hostname|, which is valid at time
  // |now|. If there is no such entry, returns NULL.
  const Entry* Lookup(const std::string& hostname, base::TimeTicks now) const;

  // Overwrites or creates an entry for |hostname|. Returns the pointer to the
  // entry, or NULL on failure (fails if caching is disabled).
  // (|error|, |addrlist|) is the value to set, and |now| is the current
  // timestamp.
  Entry* Set(const std::string& hostname,
             int error,
             const AddressList addrlist,
             base::TimeTicks now);

  // Returns true if this HostCache can contain no entries.
  bool caching_is_disabled() const {
    return max_entries_ == 0;
  }

  // Returns the number of entries in the cache.
  size_t size() const {
    return entries_.size();
  }

 private:
  FRIEND_TEST(HostCacheTest, Compact);
  FRIEND_TEST(HostCacheTest, NoCache);

  typedef base::hash_map<std::string, scoped_refptr<Entry> > EntryMap;

  // Returns true if this cache entry's result is valid at time |now|.
  static bool CanUseEntry(const Entry* entry, const base::TimeTicks now);

  // Prunes entries from the cache to bring it below max entry bound. Entries
  // matching |pinned_entry| will NOT be pruned.
  void Compact(base::TimeTicks now, const Entry* pinned_entry);

  // Bound on total size of the cache.
  size_t max_entries_;

  // Time to live for cache entries in milliseconds.
  size_t cache_duration_ms_;

  // Map from hostname (presumably in lowercase canonicalized format) to
  // a resolved result entry.
  EntryMap entries_;

  DISALLOW_COPY_AND_ASSIGN(HostCache);
};

}  // namespace net

#endif  // NET_BASE_HOST_CACHE_H_
