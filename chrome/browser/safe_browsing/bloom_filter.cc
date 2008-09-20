// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/bloom_filter.h"

#include <string.h>

BloomFilter::BloomFilter(int bit_size) {
  byte_size_ = bit_size / 8 + 1;
  bit_size_ = byte_size_ * 8;
  data_.reset(new char[byte_size_]);
  memset(data_.get(), 0, byte_size_);
}

BloomFilter::BloomFilter(char* data, int size) {
  byte_size_ = size;
  bit_size_ = byte_size_ * 8;
  data_.reset(data);
}

BloomFilter::~BloomFilter() {
}

void BloomFilter::Insert(int hash_int) {
  uint32 hash;
  memcpy(&hash, &hash_int, sizeof(hash));
  for (int i = 0; i < 4; ++i) {
    hash = RotateLeft(hash);
    uint32 index = hash % bit_size_;

    int byte = index / 8;
    int bit = index % 8;
    data_.get()[byte] |= 1 << bit;
  }
}

bool BloomFilter::Exists(int hash_int) const {
  uint32 hash;
  memcpy(&hash, &hash_int, sizeof(hash));
  for (int i = 0; i < 4; ++i) {
    hash = RotateLeft(hash);
    uint32 index = hash % bit_size_;

    int byte = index / 8;
    int bit = index % 8;
    char data = data_.get()[byte];
    if (!(data & (1 << bit)))
      return false;
  }

  return true;
}

uint32 BloomFilter::RotateLeft(uint32 hash) {
  uint32 left_byte = hash >> 24;
  hash = hash << 8;
  hash |= left_byte;
  return hash;
}
