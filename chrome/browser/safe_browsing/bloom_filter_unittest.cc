// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "chrome/browser/safe_browsing/bloom_filter.h"

#include <limits.h>

#include <set>
#include <vector>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/ref_counted.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

uint32 GenHash() {
  return static_cast<uint32>(base::RandUint64());
}

}

TEST(SafeBrowsingBloomFilter, BloomFilterUse) {
  // Use a small number for unit test so it's not slow.
  uint32 count = 1000;

  // Build up the bloom filter.
  scoped_refptr<BloomFilter> filter =
      new BloomFilter(count * BloomFilter::kBloomFilterSizeRatio);

  typedef std::set<int> Values;
  Values values;
  for (uint32 i = 0; i < count; ++i) {
    uint32 value = GenHash();
    values.insert(value);
    filter->Insert(value);
  }

  // Check serialization works.
  char* data_copy = new char[filter->size()];
  memcpy(data_copy, filter->data(), filter->size());
  scoped_refptr<BloomFilter> filter_copy =
      new BloomFilter(data_copy, filter->size(), filter->hash_keys_);

  // Check no false negatives by ensuring that every time we inserted exists.
  for (Values::iterator i = values.begin(); i != values.end(); ++i) {
    EXPECT_TRUE(filter_copy->Exists(*i));
  }

  // Check false positive error rate by checking the same number of items that
  // we inserted, but of different values, and calculating what percentage are
  // "found".
  uint32 found_count = 0;
  uint32 checked = 0;
  while (true) {
    uint32 value = GenHash();
    if (values.find(value) != values.end())
      continue;

    if (filter_copy->Exists(value))
      found_count++;

    checked ++;
    if (checked == count)
      break;
  }

  // The FP rate should be 1.2%.  Keep a large margin of error because we don't
  // want to fail this test because we happened to randomly pick a lot of FPs.
  double fp_rate = found_count * 100.0 / count;
  CHECK(fp_rate < 5.0);

  LOG(INFO) << "For safe browsing bloom filter of size " << count <<
      ", the FP rate was " << fp_rate << " %";
}

// Test that we can read and write the bloom filter file.
TEST(SafeBrowsingBloomFilter, BloomFilterFile) {
  // Create initial filter.
  const int kTestEntries = BloomFilter::kBloomFilterMinSize;
  scoped_refptr<BloomFilter> filter_write =
      new BloomFilter(kTestEntries * BloomFilter::kBloomFilterSizeRatio);

  for (int i = 0; i < kTestEntries; ++i)
    filter_write->Insert(GenHash());

  // Remove any left over test filters and serialize.
  FilePath filter_path;
  PathService::Get(base::DIR_TEMP, &filter_path);
  filter_path = filter_path.AppendASCII("SafeBrowsingTestFilter");
  file_util::Delete(filter_path, false);
  ASSERT_FALSE(file_util::PathExists(filter_path));
  ASSERT_TRUE(filter_write->WriteFile(filter_path));

  // Create new empty filter and load from disk.
  BloomFilter* filter = BloomFilter::LoadFile(filter_path);
  ASSERT_TRUE(filter != NULL);
  scoped_refptr<BloomFilter> filter_read = filter;

  // Check data consistency.
  EXPECT_EQ(filter_write->hash_keys_.size(), filter_read->hash_keys_.size());

  for (int i = 0; i < static_cast<int>(filter_write->hash_keys_.size()); ++i)
    EXPECT_EQ(filter_write->hash_keys_[i], filter_read->hash_keys_[i]);

  EXPECT_EQ(filter_write->size(), filter_read->size());

  EXPECT_TRUE(memcmp(filter_write->data(),
                     filter_read->data(),
                     filter_read->size()) == 0);

  file_util::Delete(filter_path, false);
}

