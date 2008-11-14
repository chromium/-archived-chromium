// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "chrome/browser/safe_browsing/bloom_filter.h"

#include <limits.h>

#include <set>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/ref_counted.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

uint32 GenHash() {
  return static_cast<uint32>(base::RandUint64());
}

}

TEST(SafeBrowsing, BloomFilter) {
  // Use a small number for unit test so it's not slow.
  uint32 count = 1000;

  // Build up the bloom filter.
  scoped_refptr<BloomFilter> filter = new BloomFilter(count * 10);

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
      new BloomFilter(data_copy, filter->size());

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
