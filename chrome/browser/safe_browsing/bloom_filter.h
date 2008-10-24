// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A simple bloom filter.  It's currently limited to four hashing functions,
// which are calculated from the item's hash.

#ifndef CHROME_BROWSER_SAFE_BROWSING_BLOOM_FILTER_H_
#define CHROME_BROWSER_SAFE_BROWSING_BLOOM_FILTER_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/basictypes.h"

class BloomFilter : public base::RefCountedThreadSafe<BloomFilter> {
 public:
  // Constructs an empty filter with the given size.
  BloomFilter(int bit_size);

  // Constructs a filter from serialized data.  This object owns the memory
  // and will delete it on destruction.
  BloomFilter(char* data, int size);
  ~BloomFilter();

  void Insert(int hash);
  bool Exists(int hash) const;

  const char* data() const { return data_.get(); }
  int size() const { return byte_size_; }

 private:
  static uint32 RotateLeft(uint32 hash);

  int byte_size_;  // size in bytes
  int bit_size_;   // size in bits
  scoped_array<char> data_;
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_BLOOM_FILTER_H_
