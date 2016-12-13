// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/addr.h"
#include "net/disk_cache/disk_cache_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {

TEST_F(DiskCacheTest, CacheAddr_Size) {
  Addr addr1(0);
  EXPECT_FALSE(addr1.is_initialized());

  // The object should not be more expensive than the actual address.
  EXPECT_EQ(sizeof(uint32), sizeof(addr1));
}

TEST_F(DiskCacheTest, CacheAddr_ValidValues) {
  Addr addr2(BLOCK_1K, 3, 5, 25);
  EXPECT_EQ(BLOCK_1K, addr2.file_type());
  EXPECT_EQ(3, addr2.num_blocks());
  EXPECT_EQ(5, addr2.FileNumber());
  EXPECT_EQ(25, addr2.start_block());
  EXPECT_EQ(1024, addr2.BlockSize());
}

TEST_F(DiskCacheTest, CacheAddr_InvalidValues) {
  Addr addr3(BLOCK_4K, 0x44, 0x41508, 0x952536);
  EXPECT_EQ(BLOCK_4K, addr3.file_type());
  EXPECT_EQ(4, addr3.num_blocks());
  EXPECT_EQ(8, addr3.FileNumber());
  EXPECT_EQ(0x2536, addr3.start_block());
  EXPECT_EQ(4096, addr3.BlockSize());
}

}  // namespace disk_cache
