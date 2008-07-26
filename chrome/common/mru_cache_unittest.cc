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

#include "base/basictypes.h"
#include "chrome/common/mru_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

int cached_item_live_count = 0;

struct CachedItem {
  CachedItem() : value(0) {
    cached_item_live_count++;
  }

  CachedItem(int new_value) : value(new_value) {
    cached_item_live_count++;
  }

  CachedItem(const CachedItem& other) : value(other.value) {
    cached_item_live_count++;
  }

  ~CachedItem() {
    cached_item_live_count--;
  }

  int value;
};

}  // namespace

TEST(MRUCacheTest, Basic) {
  typedef MRUCache<int, CachedItem> Cache;
  Cache cache(Cache::NO_AUTO_EVICT);

  // Check failure conditions
  {
    CachedItem test_item;
    EXPECT_TRUE(cache.Get(0) == cache.end());
    EXPECT_TRUE(cache.Peek(0) == cache.end());
  }

  static const int kItem1Key = 5;
  CachedItem item1(10);
  Cache::iterator inserted_item = cache.Put(kItem1Key, item1);
  EXPECT_EQ(1, cache.size());

  // Check that item1 was properly inserted.
  {
    Cache::iterator found = cache.Get(kItem1Key);
    EXPECT_TRUE(inserted_item == cache.begin());
    EXPECT_TRUE(found != cache.end());

    found = cache.Peek(kItem1Key);
    EXPECT_TRUE(found != cache.end());

    EXPECT_EQ(kItem1Key, found->first);
    EXPECT_EQ(item1.value, found->second.value);
  }

  static const int kItem2Key = 7;
  CachedItem item2(12);
  cache.Put(kItem2Key, item2);
  EXPECT_EQ(2, cache.size());

  // Check that item1 is the oldest since item2 was added afterwards.
  {
    Cache::reverse_iterator oldest = cache.rbegin();
    ASSERT_TRUE(oldest != cache.rend());
    EXPECT_EQ(kItem1Key, oldest->first);
    EXPECT_EQ(item1.value, oldest->second.value);
  }

  // Check that item1 is still accessible by key.
  {
    Cache::iterator test_item = cache.Get(kItem1Key);
    ASSERT_TRUE(test_item != cache.end());
    EXPECT_EQ(kItem1Key, test_item->first);
    EXPECT_EQ(item1.value, test_item->second.value);
  }

  // Check that retrieving item1 pushed item2 to oldest.
  {
    Cache::reverse_iterator oldest = cache.rbegin();
    ASSERT_TRUE(oldest != cache.rend());
    EXPECT_EQ(kItem2Key, oldest->first);
    EXPECT_EQ(item2.value, oldest->second.value);
  }

  // Remove the oldest item and check that item1 is now the only member.
  {
    Cache::reverse_iterator next = cache.Erase(cache.rbegin());

    EXPECT_EQ(1, cache.size());

    EXPECT_TRUE(next == cache.rbegin());
    EXPECT_EQ(kItem1Key, next->first);
    EXPECT_EQ(item1.value, next->second.value);

    cache.Erase(cache.begin());
    EXPECT_EQ(0, cache.size());
  }
}

TEST(MRUCacheTest, GetVsPeek) {
  typedef MRUCache<int, CachedItem> Cache;
  Cache cache(Cache::NO_AUTO_EVICT);

  static const int kItem1Key = 1;
  CachedItem item1(10);
  cache.Put(kItem1Key, item1);

  static const int kItem2Key = 2;
  CachedItem item2(20);
  cache.Put(kItem2Key, item2);

  // This should do nothing since the size is bigger than the number of items.
  cache.ShrinkToSize(100);

  // Check that item1 starts out as oldest
  {
    Cache::reverse_iterator iter = cache.rbegin();
    ASSERT_TRUE(iter != cache.rend());
    EXPECT_EQ(kItem1Key, iter->first);
    EXPECT_EQ(item1.value, iter->second.value);
  }

  // Check that Peek doesn't change ordering
  {
    Cache::iterator peekiter = cache.Peek(kItem1Key);
    ASSERT_TRUE(peekiter != cache.end());

    Cache::reverse_iterator iter = cache.rbegin();
    ASSERT_TRUE(iter != cache.rend());
    EXPECT_EQ(kItem1Key, iter->first);
    EXPECT_EQ(item1.value, iter->second.value);
  }
}

TEST(MRUCacheTest, KeyReplacement) {
  typedef MRUCache<int, CachedItem> Cache;
  Cache cache(Cache::NO_AUTO_EVICT);

  static const int kItem1Key = 1;
  CachedItem item1(10);
  cache.Put(kItem1Key, item1);

  static const int kItem2Key = 2;
  CachedItem item2(20);
  cache.Put(kItem2Key, item2);

  static const int kItem3Key = 3;
  CachedItem item3(30);
  cache.Put(kItem3Key, item3);

  static const int kItem4Key = 4;
  CachedItem item4(40);
  cache.Put(kItem4Key, item4);

  CachedItem item5(50);
  cache.Put(kItem3Key, item5);

  EXPECT_EQ(4, cache.size());
  for (int i = 0; i < 3; ++i) {
    Cache::reverse_iterator iter = cache.rbegin();
    ASSERT_TRUE(iter != cache.rend());
  }

  // Make it so only the most important element is there.
  cache.ShrinkToSize(1);

  Cache::iterator iter = cache.begin();
  EXPECT_EQ(kItem3Key, iter->first);
  EXPECT_EQ(item5.value, iter->second.value);
}

// Make sure that the owning version release its pointers properly.
TEST(MRUCacheTest, Owning) {
  typedef OwningMRUCache<int, CachedItem*> Cache;
  Cache cache(Cache::NO_AUTO_EVICT);

  int initial_count = cached_item_live_count;

  // First insert and item and then overwrite it.
  static const int kItem1Key = 1;
  cache.Put(kItem1Key, new CachedItem(20));
  cache.Put(kItem1Key, new CachedItem(22));

  // There should still be one item, and one extra live item.
  Cache::iterator iter = cache.Get(kItem1Key);
  EXPECT_EQ(1, cache.size());
  EXPECT_TRUE(iter != cache.end());
  EXPECT_EQ(initial_count + 1, cached_item_live_count);

  // Now remove it.
  cache.Erase(cache.begin());
  EXPECT_EQ(initial_count, cached_item_live_count);

  // Now try another cache that goes out of scope to make sure its pointers
  // go away.
  {
    Cache cache2(Cache::NO_AUTO_EVICT);
    cache2.Put(1, new CachedItem(20));
    cache2.Put(2, new CachedItem(20));
  }

  // There should be no objects leaked.
  EXPECT_EQ(initial_count, cached_item_live_count);
}

TEST(MRUCacheTest, AutoEvict) {
  typedef OwningMRUCache<int, CachedItem*> Cache;
  static const Cache::size_type kMaxSize = 3;

  int initial_count = cached_item_live_count;

  {
    Cache cache(kMaxSize);

    static const int kItem1Key = 1, kItem2Key = 2, kItem3Key = 3, kItem4Key = 4;
    cache.Put(kItem1Key, new CachedItem(20));
    cache.Put(kItem2Key, new CachedItem(21));
    cache.Put(kItem3Key, new CachedItem(22));
    cache.Put(kItem4Key, new CachedItem(23));

    // The cache should only have kMaxSize items in it even though we inserted
    // more.
    EXPECT_EQ(kMaxSize, cache.size());
  }

  // There should be no objects leaked.
  EXPECT_EQ(initial_count, cached_item_live_count);
}