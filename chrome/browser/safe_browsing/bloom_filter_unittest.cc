// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "chrome/browser/safe_browsing/bloom_filter.h"

#include <limits.h>

#include <set>

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

uint32 GenHash() {
  return static_cast<uint32>(base::RandInt(INT_MIN, INT_MAX));
}

}

TEST(SafeBrowsing, BloomFilter) {
  // rand_util isn't random enough on Win2K, see bug 1076619.
  if (win_util::GetWinVersion() == win_util::WINVERSION_2000)
    return;

  // Use a small number for unit test so it's not slow.
  int count = 1000;//100000;

  // Build up the bloom filter.
  BloomFilter filter(count * 10);

  typedef std::set<int> Values;
  Values values;
  for (int i = 0; i < count; ++i) {
    uint32 value = GenHash();
    values.insert(value);
    filter.Insert(value);
  }

  // Check serialization works.
  char* data_copy = new char[filter.size()];
  memcpy(data_copy, filter.data(), filter.size());
  BloomFilter filter_copy(data_copy, filter.size());

  // Check no false negatives by ensuring that every time we inserted exists.
  for (Values::iterator i = values.begin(); i != values.end(); ++i) {
    EXPECT_TRUE(filter_copy.Exists(*i));
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

    if (filter_copy.Exists(value))
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
