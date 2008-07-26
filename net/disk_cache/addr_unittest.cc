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

#include "net/disk_cache/addr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {

TEST(DiskCacheTest, CacheAddr_Size) {
  Addr addr1(0);
  EXPECT_FALSE(addr1.is_initialized());

  // The object should not be more expensive than the actual address.
  EXPECT_EQ(sizeof(uint32), sizeof(addr1));
}

TEST(DiskCacheTest, CacheAddr_ValidValues) {
  Addr addr2(BLOCK_1K, 3, 5, 25);
  EXPECT_EQ(BLOCK_1K, addr2.file_type());
  EXPECT_EQ(3, addr2.num_blocks());
  EXPECT_EQ(5, addr2.FileNumber());
  EXPECT_EQ(25, addr2.start_block());
  EXPECT_EQ(1024, addr2.BlockSize());
}

TEST(DiskCacheTest, CacheAddr_InvalidValues) {
  Addr addr3(BLOCK_4K, 0x44, 0x41508, 0x952536);
  EXPECT_EQ(BLOCK_4K, addr3.file_type());
  EXPECT_EQ(4, addr3.num_blocks());
  EXPECT_EQ(8, addr3.FileNumber());
  EXPECT_EQ(0x2536, addr3.start_block());
  EXPECT_EQ(4096, addr3.BlockSize());
}

}  // namespace disk_cache
