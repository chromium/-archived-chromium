// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "net/disk_cache/storage_block.h"
#include "net/disk_cache/storage_block-inl.h"
#include "net/disk_cache/disk_cache_test_base.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST_F(DiskCacheTest, StorageBlock_LoadStore) {
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

TEST_F(DiskCacheTest, StorageBlock_SetData) {
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
  EXPECT_TRUE(0 == entry2.Data()->hash);

  EXPECT_TRUE(entry2.Data() != entry1.Data());
  entry2.SetData(entry1.Data());
  EXPECT_EQ(0xaa5555aa, entry2.Data()->hash);
  EXPECT_TRUE(entry2.Data() == entry1.Data());
}

TEST_F(DiskCacheTest, StorageBlock_SetModified) {
  std::wstring filename = GetCachePath();
  file_util::AppendToPath(&filename, L"a_test");
  scoped_refptr<disk_cache::MappedFile> file(new disk_cache::MappedFile);
  ASSERT_TRUE(CreateCacheTestFile(filename.c_str()));
  ASSERT_TRUE(file->Init(filename, 8192));

  disk_cache::CacheEntryBlock* entry1 =
      new disk_cache::CacheEntryBlock(file, disk_cache::Addr(0xa0010003));
  EXPECT_TRUE(entry1->Load());
  EXPECT_TRUE(0 == entry1->Data()->hash);
  entry1->Data()->hash = 0x45687912;
  entry1->set_modified();
  delete entry1;

  disk_cache::CacheEntryBlock entry2(file, disk_cache::Addr(0xa0010003));
  EXPECT_TRUE(entry2.Load());
  EXPECT_TRUE(0x45687912 == entry2.Data()->hash);
}
