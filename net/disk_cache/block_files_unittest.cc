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

#include "net/disk_cache/block_files.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(DiskCacheTest, BlockFiles_Grow) {
  std::wstring path = GetCachePath();
  ASSERT_TRUE(DeleteCache(path.c_str()));

  disk_cache::BlockFiles files(path);
  ASSERT_TRUE(files.Init(true));

  // Fill up the 32-byte block file (use three files).
  for (int i = 0; i < 35000; i++) {
    disk_cache::Addr address(0);
    EXPECT_TRUE(files.CreateBlock(disk_cache::RANKINGS, 4, &address));
  }
}

// Handling of block files not properly closed.
TEST(DiskCacheTest, BlockFiles_Recover) {
  std::wstring path = GetCachePath();
  ASSERT_TRUE(DeleteCache(path.c_str()));

  disk_cache::BlockFiles files(path);
  ASSERT_TRUE(files.Init(true));

  const int kNumEntries = 2000;
  disk_cache::CacheAddr entries[kNumEntries];

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);
  for (int i = 0; i < kNumEntries; i++) {
    disk_cache::Addr address(0);
    int size = (rand() % 4) + 1;
    EXPECT_TRUE(files.CreateBlock(disk_cache::RANKINGS, size, &address));
    entries[i] = address.value();
  }

  for (int i = 0; i < kNumEntries; i++) {
    int source1 = rand() % kNumEntries;
    int source2 = rand() % kNumEntries;
    disk_cache::CacheAddr temp = entries[source1];
    entries[source1] = entries[source2];
    entries[source2] = temp;
  }

  for (int i = 0; i < kNumEntries / 2; i++) {
    disk_cache::Addr address(entries[i]);
    files.DeleteBlock(address, false);
  }

  // At this point, there are kNumEntries / 2 entries on the file, randomly
  // distributed both on location and size.

  disk_cache::Addr address(entries[kNumEntries / 2]);
  disk_cache::MappedFile* file = files.GetFile(address);
  ASSERT_TRUE(NULL != file);

  disk_cache::BlockFileHeader* header =
      reinterpret_cast<disk_cache::BlockFileHeader*>(file->buffer());
  ASSERT_TRUE(NULL != header);

  ASSERT_EQ(0, header->updating);

  int max_entries = header->max_entries;
  int empty_1 = header->empty[0];
  int empty_2 = header->empty[1];
  int empty_3 = header->empty[2];
  int empty_4 = header->empty[3];

  // Corrupt the file.
  header->max_entries = header->empty[0] = 0;
  header->empty[1] = header->empty[2] = header->empty[3] = 0;
  header->updating = -1;

  files.CloseFiles();

  ASSERT_TRUE(files.Init(false));

  // The file must have been fixed.
  file = files.GetFile(address);
  ASSERT_TRUE(NULL != file);

  header = reinterpret_cast<disk_cache::BlockFileHeader*>(file->buffer());
  ASSERT_TRUE(NULL != header);

  ASSERT_EQ(0, header->updating);

  EXPECT_EQ(max_entries, header->max_entries);
  EXPECT_EQ(empty_1, header->empty[0]);
  EXPECT_EQ(empty_2, header->empty[1]);
  EXPECT_EQ(empty_3, header->empty[2]);
  EXPECT_EQ(empty_4, header->empty[3]);
}

