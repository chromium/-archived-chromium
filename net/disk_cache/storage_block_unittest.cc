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

#include "base/file_util.h"
#include "net/disk_cache/storage_block.h"
#include "net/disk_cache/storage_block-inl.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(DiskCacheTest, StorageBlock_LoadStore) {
  std::wstring filename = GetCachePath();
  file_util::AppendToPath(&filename, L"a_test");
  scoped_refptr<disk_cache::MappedFile> file(new disk_cache::MappedFile);
  ASSERT_TRUE(CreateCacheTestFile(filename.c_str()));
  ASSERT_TRUE(file->Init(filename, 8192));

  disk_cache::CacheEntryBlock entry1(file, disk_cache::Addr(0xa0010001));
  memset(entry1.Data(), 0, sizeof(disk_cache::EntryStore));
  entry1.Data()->hash = 0xaa5555aa;
  entry1.Data()->rankings_node = 0xa0010002;

  EXPECT_TRUE(entry1.Store());
  entry1.Data()->hash = 0x88118811;
  entry1.Data()->rankings_node = 0xa0040009;

  EXPECT_TRUE(entry1.Load());
  EXPECT_EQ(0xaa5555aa, entry1.Data()->hash);
  EXPECT_EQ(0xa0010002, entry1.Data()->rankings_node);
}

TEST(DiskCacheTest, StorageBlock_SetData) {
  std::wstring filename = GetCachePath();
  file_util::AppendToPath(&filename, L"a_test");
  scoped_refptr<disk_cache::MappedFile> file(new disk_cache::MappedFile);
  ASSERT_TRUE(CreateCacheTestFile(filename.c_str()));
  ASSERT_TRUE(file->Init(filename, 8192));

  disk_cache::CacheEntryBlock entry1(file, disk_cache::Addr(0xa0010001));
  entry1.Data()->hash = 0xaa5555aa;

  disk_cache::CacheEntryBlock entry2(file, disk_cache::Addr(0xa0010002));
  EXPECT_TRUE(entry2.Load());
  EXPECT_TRUE(entry2.Data() != NULL);
  EXPECT_EQ(0, entry2.Data()->hash);

  EXPECT_TRUE(entry2.Data() != entry1.Data());
  entry2.SetData(entry1.Data());
  EXPECT_EQ(0xaa5555aa, entry2.Data()->hash);
  EXPECT_TRUE(entry2.Data() == entry1.Data());
}

TEST(DiskCacheTest, StorageBlock_SetModified) {
  std::wstring filename = GetCachePath();
  file_util::AppendToPath(&filename, L"a_test");
  scoped_refptr<disk_cache::MappedFile> file(new disk_cache::MappedFile);
  ASSERT_TRUE(CreateCacheTestFile(filename.c_str()));
  ASSERT_TRUE(file->Init(filename, 8192));

  disk_cache::CacheEntryBlock* entry1 =
      new disk_cache::CacheEntryBlock(file, disk_cache::Addr(0xa0010003));
  EXPECT_TRUE(entry1->Load());
  EXPECT_EQ(0, entry1->Data()->hash);
  entry1->Data()->hash = 0x45687912;
  entry1->set_modified();
  delete entry1;

  disk_cache::CacheEntryBlock entry2(file, disk_cache::Addr(0xa0010003));
  EXPECT_TRUE(entry2.Load());
  EXPECT_EQ(0x45687912, entry2.Data()->hash);
}
