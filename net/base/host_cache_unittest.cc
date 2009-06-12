// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_cache.h"

#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {
static const int kMaxCacheEntries = 10;
static const int kCacheDurationMs = 10000;  // 10 seconds.
}

TEST(HostCacheTest, Basic) {
  HostCache cache(kMaxCacheEntries, kCacheDurationMs);

  // Start at t=0.
  base::TimeTicks now;

  const HostCache::Entry* entry1 = NULL;  // Entry for foobar.com.
  const HostCache::Entry* entry2 = NULL;  // Entry for foobar2.com.

  EXPECT_EQ(0U, cache.size());

  // Add an entry for "foobar.com" at t=0.
  EXPECT_EQ(NULL, cache.Lookup("foobar.com", base::TimeTicks()));
  cache.Set("foobar.com", OK, AddressList(), now);
  entry1 = cache.Lookup("foobar.com", base::TimeTicks());
  EXPECT_FALSE(NULL == entry1);
  EXPECT_EQ(1U, cache.size());

  // Advance to t=5.
  now += base::TimeDelta::FromSeconds(5);

  // Add an entry for "foobar2.com" at t=5.
  EXPECT_EQ(NULL, cache.Lookup("foobar2.com", base::TimeTicks()));
  cache.Set("foobar2.com", OK, AddressList(), now);
  entry2 = cache.Lookup("foobar2.com", base::TimeTicks());
  EXPECT_FALSE(NULL == entry1);
  EXPECT_EQ(2U, cache.size());

  // Advance to t=9
  now += base::TimeDelta::FromSeconds(4);

  // Verify that the entries we added are still retrievable, and usable.
  EXPECT_EQ(entry1, cache.Lookup("foobar.com", now));
  EXPECT_EQ(entry2, cache.Lookup("foobar2.com", now));

  // Advance to t=10; entry1 is now expired.
  now += base::TimeDelta::FromSeconds(1);

  EXPECT_EQ(NULL, cache.Lookup("foobar.com", now));
  EXPECT_EQ(entry2, cache.Lookup("foobar2.com", now));

  // Update entry1, so it is no longer expired.
  cache.Set("foobar.com", OK, AddressList(), now);
  // Re-uses existing entry storage.
  EXPECT_EQ(entry1, cache.Lookup("foobar.com", now));
  EXPECT_EQ(2U, cache.size());

  // Both entries should still be retrievable and usable.
  EXPECT_EQ(entry1, cache.Lookup("foobar.com", now));
  EXPECT_EQ(entry2, cache.Lookup("foobar2.com", now));

  // Advance to t=20; both entries are now expired.
  now += base::TimeDelta::FromSeconds(10);

  EXPECT_EQ(NULL, cache.Lookup("foobar.com", now));
  EXPECT_EQ(NULL, cache.Lookup("foobar2.com", now));
}

// Try caching entries for a failed resolve attempt.
TEST(HostCacheTest, NegativeEntry) {
  HostCache cache(kMaxCacheEntries, kCacheDurationMs);

  // Set t=0.
  base::TimeTicks now;

  EXPECT_EQ(NULL, cache.Lookup("foobar.com", base::TimeTicks()));
  cache.Set("foobar.com", ERR_NAME_NOT_RESOLVED, AddressList(), now);
  EXPECT_EQ(1U, cache.size());

  // We disallow use of negative entries.
  EXPECT_EQ(NULL, cache.Lookup("foobar.com", now));

  // Now overwrite with a valid entry, and then overwrite with negative entry
  // again -- the valid entry should be kicked out.
  cache.Set("foobar.com", OK, AddressList(), now);
  EXPECT_FALSE(NULL == cache.Lookup("foobar.com", now));
  cache.Set("foobar.com", ERR_NAME_NOT_RESOLVED, AddressList(), now);
  EXPECT_EQ(NULL, cache.Lookup("foobar.com", now));
}

TEST(HostCacheTest, Compact) {
  // Initial entries limit is big enough to accomadate everything we add.
  net::HostCache cache(kMaxCacheEntries, kCacheDurationMs);

  EXPECT_EQ(0U, cache.size());

  // t=10
  base::TimeTicks now = base::TimeTicks() + base::TimeDelta::FromSeconds(10);

  // Add five valid entries at t=10.
  for (int i = 0; i < 5; ++i) {
    std::string hostname = StringPrintf("valid%d", i);
    cache.Set(hostname, OK, AddressList(), now);
  }
  EXPECT_EQ(5U, cache.size());

  // Add 3 expired entries at t=0.
  for (int i = 0; i < 3; ++i) {
    std::string hostname = StringPrintf("expired%d", i);
    base::TimeTicks t = now - base::TimeDelta::FromSeconds(10);
    cache.Set(hostname, OK, AddressList(), t);
  }
  EXPECT_EQ(8U, cache.size());

  // Add 2 negative entries at t=10
  for (int i = 0; i < 2; ++i) {
    std::string hostname = StringPrintf("negative%d", i);
    cache.Set(hostname, ERR_NAME_NOT_RESOLVED, AddressList(), now);
  }
  EXPECT_EQ(10U, cache.size());

  EXPECT_TRUE(ContainsKey(cache.entries_, "valid0"));
  EXPECT_TRUE(ContainsKey(cache.entries_, "valid1"));
  EXPECT_TRUE(ContainsKey(cache.entries_, "valid2"));
  EXPECT_TRUE(ContainsKey(cache.entries_, "valid3"));
  EXPECT_TRUE(ContainsKey(cache.entries_, "valid4"));
  EXPECT_TRUE(ContainsKey(cache.entries_, "expired0"));
  EXPECT_TRUE(ContainsKey(cache.entries_, "expired1"));
  EXPECT_TRUE(ContainsKey(cache.entries_, "expired2"));
  EXPECT_TRUE(ContainsKey(cache.entries_, "negative0"));
  EXPECT_TRUE(ContainsKey(cache.entries_, "negative1"));

  // Shrink the max constraints bound and compact. We expect the "negative"
  // and "expired" entries to have been dropped.
  cache.max_entries_ = 5;
  cache.Compact(now, NULL);
  EXPECT_EQ(5U, cache.entries_.size());

  EXPECT_TRUE(ContainsKey(cache.entries_, "valid0"));
  EXPECT_TRUE(ContainsKey(cache.entries_, "valid1"));
  EXPECT_TRUE(ContainsKey(cache.entries_, "valid2"));
  EXPECT_TRUE(ContainsKey(cache.entries_, "valid3"));
  EXPECT_TRUE(ContainsKey(cache.entries_, "valid4"));
  EXPECT_FALSE(ContainsKey(cache.entries_, "expired0"));
  EXPECT_FALSE(ContainsKey(cache.entries_, "expired1"));
  EXPECT_FALSE(ContainsKey(cache.entries_, "expired2"));
  EXPECT_FALSE(ContainsKey(cache.entries_, "negative0"));
  EXPECT_FALSE(ContainsKey(cache.entries_, "negative1"));

  // Shrink further -- this time the compact will start dropping valid entries
  // to make space.
  cache.max_entries_ = 3;
  cache.Compact(now, NULL);
  EXPECT_EQ(3U, cache.size());
}

// Add entries while the cache is at capacity, causing evictions.
TEST(HostCacheTest, SetWithCompact) {
  net::HostCache cache(3, kCacheDurationMs);

  // t=10
  base::TimeTicks now =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(kCacheDurationMs);

  cache.Set("host1", OK, AddressList(), now);
  cache.Set("host2", OK, AddressList(), now);
  cache.Set("expired", OK, AddressList(),
      now - base::TimeDelta::FromMilliseconds(kCacheDurationMs));

  EXPECT_EQ(3U, cache.size());

  // Should all be retrievable except "expired".
  EXPECT_FALSE(NULL == cache.Lookup("host1", now));
  EXPECT_FALSE(NULL == cache.Lookup("host2", now));
  EXPECT_TRUE(NULL == cache.Lookup("expired", now));

  // Adding the fourth entry will cause "expired" to be evicted.
  cache.Set("host3", OK, AddressList(), now);
  EXPECT_EQ(3U, cache.size());
  EXPECT_EQ(NULL, cache.Lookup("expired", now));
  EXPECT_FALSE(NULL == cache.Lookup("host1", now));
  EXPECT_FALSE(NULL == cache.Lookup("host2", now));
  EXPECT_FALSE(NULL == cache.Lookup("host3", now));

  // Add two more entries. Something should be evicted, however "host5"
  // should definitely be in there (since it was last inserted).
  cache.Set("host4", OK, AddressList(), now);
  EXPECT_EQ(3U, cache.size());
  cache.Set("host5", OK, AddressList(), now);
  EXPECT_EQ(3U, cache.size());
  EXPECT_FALSE(NULL == cache.Lookup("host5", now));
}

TEST(HostCacheTest, NoCache) {
  // Disable caching.
  HostCache cache(0, kCacheDurationMs);
  EXPECT_TRUE(cache.caching_is_disabled());

  // Set t=0.
  base::TimeTicks now;

  // Lookup and Set should have no effect.
  EXPECT_EQ(NULL, cache.Lookup("foobar.com", base::TimeTicks()));
  cache.Set("foobar.com", OK, AddressList(), now);
  EXPECT_EQ(NULL, cache.Lookup("foobar.com", base::TimeTicks()));

  EXPECT_EQ(0U, cache.size());
}

}  // namespace net
