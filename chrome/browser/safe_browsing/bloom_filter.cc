// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/bloom_filter.h"

#include <string.h>

#include "base/logging.h"
#include "base/rand_util.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"

namespace {

// The Jenkins 96 bit mix function:
// http://www.concentric.net/~Ttwang/tech/inthash.htm
uint32 HashMix(uint64 hash_key, uint32 c) {
  uint32 a = static_cast<uint32>(hash_key)       & 0xFFFFFFFF;
  uint32 b = static_cast<uint32>(hash_key >> 32) & 0xFFFFFFFF;

  a = a - b;  a = a - c;  a = a ^ (c >> 13);
  b = b - c;  b = b - a;  b = b ^ (a << 8);
  c = c - a;  c = c - b;  c = c ^ (b >> 13);
  a = a - b;  a = a - c;  a = a ^ (c >> 12);
  b = b - c;  b = b - a;  b = b ^ (a << 16);
  c = c - a;  c = c - b;  c = c ^ (b >> 5);
  a = a - b;  a = a - c;  a = a ^ (c >> 3);
  b = b - c;  b = b - a;  b = b ^ (a << 10);
  c = c - a;  c = c - b;  c = c ^ (b >> 15);

  return c;
}

}  // namespace

BloomFilter::BloomFilter(int bit_size) {
  for (int i = 0; i < kNumHashKeys; ++i)
    hash_keys_.push_back(base::RandUint64());
  byte_size_ = bit_size / 8 + 1;
  bit_size_ = byte_size_ * 8;
  data_.reset(new char[byte_size_]);
  memset(data_.get(), 0, byte_size_);
}

BloomFilter::BloomFilter(char* data, int size, const std::vector<uint64>& keys)
    : hash_keys_(keys) {
  byte_size_ = size;
  bit_size_ = byte_size_ * 8;
  data_.reset(data);
}

BloomFilter::~BloomFilter() {
}

void BloomFilter::Insert(int hash_int) {
  uint32 hash;
  memcpy(&hash, &hash_int, sizeof(hash));
  for (int i = 0; i < static_cast<int>(hash_keys_.size()); ++i) {
    uint32 mix = HashMix(hash_keys_[i], hash);
    uint32 index = mix % bit_size_;
    int byte = index / 8;
    int bit = index % 8;
    data_.get()[byte] |= 1 << bit;
  }
}

bool BloomFilter::Exists(int hash_int) const {
  uint32 hash;
  memcpy(&hash, &hash_int, sizeof(hash));
  for (int i = 0; i < static_cast<int>(hash_keys_.size()); ++i) {
    uint32 mix = HashMix(hash_keys_[i], hash);
    uint32 index = mix % bit_size_;
    int byte = index / 8;
    int bit = index % 8;
    char data = data_.get()[byte];
    if (!(data & (1 << bit)))
      return false;
  }

  return true;
}

// static.
BloomFilter* BloomFilter::LoadFile(const FilePath& filter_name) {
  net::FileStream filter;

  if (filter.Open(filter_name,
                  base::PLATFORM_FILE_OPEN |
                  base::PLATFORM_FILE_READ) != net::OK)
    return NULL;

  // Make sure we have a file version that we can understand.
  int file_version;
  int bytes_read = filter.Read(reinterpret_cast<char*>(&file_version),
                               sizeof(file_version), NULL);
  if (bytes_read != sizeof(file_version) || file_version != kFileVersion)
    return NULL;

  // Get all the random hash keys.
  int num_keys;
  bytes_read = filter.Read(reinterpret_cast<char*>(&num_keys),
                           sizeof(num_keys), NULL);
  if (bytes_read != sizeof(num_keys) || num_keys < 1 || num_keys > kNumHashKeys)
    return NULL;

  std::vector<uint64> hash_keys;
  for (int i = 0; i < num_keys; ++i) {
    uint64 key;
    bytes_read = filter.Read(reinterpret_cast<char*>(&key), sizeof(key), NULL);
    if (bytes_read != sizeof(key))
      return NULL;
    hash_keys.push_back(key);
  }

  // Read in the filter data, with sanity checks on min and max sizes.
  int64 remaining64 = filter.Available();
  if (remaining64 < kBloomFilterMinSize || remaining64 > kBloomFilterMaxSize)
    return NULL;

  int byte_size = static_cast<int>(remaining64);
  scoped_array<char> data(new char[byte_size]);
  bytes_read = filter.Read(data.get(), byte_size, NULL);
  if (bytes_read != byte_size)
    return NULL;

  // We've read everything okay, commit the data.
  return new BloomFilter(data.release(), byte_size, hash_keys);
}

bool BloomFilter::WriteFile(const FilePath& filter_name) {
  net::FileStream filter;

  if (filter.Open(filter_name,
                  base::PLATFORM_FILE_WRITE |
                  base::PLATFORM_FILE_CREATE_ALWAYS) != net::OK)
    return false;

  // Write the version information.
  int version = kFileVersion;
  int bytes_written = filter.Write(reinterpret_cast<char*>(&version),
                                   sizeof(version), NULL);
  if (bytes_written != sizeof(version))
    return false;

  // Write the number of random hash keys.
  int num_keys = static_cast<int>(hash_keys_.size());
  bytes_written = filter.Write(reinterpret_cast<char*>(&num_keys),
                               sizeof(num_keys), NULL);
  if (bytes_written != sizeof(num_keys))
    return false;

  for (int i = 0; i < num_keys; ++i) {
    bytes_written = filter.Write(reinterpret_cast<char*>(&hash_keys_[i]),
                                 sizeof(hash_keys_[i]), NULL);
    if (bytes_written != sizeof(hash_keys_[i]))
      return false;
  }

  // Write the filter data.
  bytes_written = filter.Write(data_.get(), byte_size_, NULL);
  if (bytes_written != byte_size_)
    return false;

  return true;
}

