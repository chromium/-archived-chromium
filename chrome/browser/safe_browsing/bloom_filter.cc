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

#include "chrome/browser/safe_browsing/bloom_filter.h"

#include <windows.h>


BloomFilter::BloomFilter(int bit_size) {
  byte_size_ = bit_size / 8 + 1;
  bit_size_ = byte_size_ * 8;
  data_.reset(new char[byte_size_]);
  ZeroMemory(data_.get(), byte_size_);
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
