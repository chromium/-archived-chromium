// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A simple bloom filter. It uses a large number (20) of hashes to reduce the
// possibility of false positives. The bloom filter's hashing uses random keys
// in order to minimize the chance that a false positive for one user is a false
// positive for all.
//
// The bloom filter manages it serialization to disk with the following file
// format:
//         4 byte version number
//         4 byte number of hash keys (n)
//     n * 8 bytes of hash keys
// Remaining bytes are the filter data.

#ifndef CHROME_BROWSER_SAFE_BROWSING_BLOOM_FILTER_H_
#define CHROME_BROWSER_SAFE_BROWSING_BLOOM_FILTER_H_

#include <vector>

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class BloomFilter : public base::RefCountedThreadSafe<BloomFilter> {
 public:
  // Constructs an empty filter with the given size.
  explicit BloomFilter(int bit_size);

  // Constructs a filter from serialized data. This object owns the memory and
  // will delete it on destruction.
  BloomFilter(char* data, int size, const std::vector<uint64>& keys);

  ~BloomFilter();

  void Insert(int hash);
  bool Exists(int hash) const;

  const char* data() const { return data_.get(); }
  int size() const { return byte_size_; }

  // Loading and storing the filter from / to disk.
  static BloomFilter* LoadFile(const FilePath& filter_name);
  bool WriteFile(const FilePath& filter_name);

  // How many bits to use per item. See the design doc for more information.
  static const int kBloomFilterSizeRatio = 25;

  // Force a minimum size on the bloom filter to prevent a high false positive
  // hash request rate (in bytes).
  static const int kBloomFilterMinSize = 250000;

  // Force a maximum size on the bloom filter to avoid using too much memory
  // (in bytes).
  static const int kBloomFilterMaxSize = 2 * 1024 * 1024;

 private:
  FRIEND_TEST(SafeBrowsingBloomFilter, BloomFilterUse);
  FRIEND_TEST(SafeBrowsingBloomFilter, BloomFilterFile);

  static const int kNumHashKeys = 20;
  static const int kFileVersion = 1;

  int byte_size_;  // size in bytes
  int bit_size_;   // size in bits
  scoped_array<char> data_;

  // Random keys used for hashing.
  std::vector<uint64> hash_keys_;

  DISALLOW_COPY_AND_ASSIGN(BloomFilter);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_BLOOM_FILTER_H_
