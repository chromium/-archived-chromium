// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/backend_impl.h"
#include "net/disk_cache/disk_cache_test_base.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/mapped_file.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;

namespace {

// Copies a set of cache files from the data folder to the test folder.
bool CopyTestCache(const std::wstring& name) {
  FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("net");
  path = path.AppendASCII("data");
  path = path.AppendASCII("cache_tests");
  path = path.Append(FilePath::FromWStringHack(name));

  std::wstring dest = GetCachePath();
  if (!DeleteCache(dest.c_str()))
    return false;
  return file_util::CopyDirectory(path, FilePath::FromWStringHack(dest), false);
}

}  // namespace

// Tests that can run with different types of caches.
class DiskCacheBackendTest : public DiskCacheTestWithCache {
 protected:
  void BackendBasics();
  void BackendKeying();
  void BackendSetSize();
  void BackendLoad();
  void BackendValidEntry();
  void BackendInvalidEntry();
  void BackendInvalidEntryRead();
  void BackendInvalidEntryWithLoad();
  void BackendTrimInvalidEntry();
  void BackendEnumerations();
  void BackendInvalidEntryEnumeration();
  void BackendFixEnumerators();
  void BackendDoomRecent();
  void BackendDoomBetween();
  void BackendTransaction(const std::wstring& name, int num_entries, bool load);
  void BackendRecoverInsert();
  void BackendRecoverRemove();
  void BackendInvalidEntry2();
  void BackendNotMarkedButDirty(const std::wstring& name);
  void BackendDoomAll();
  void BackendDoomAll2();
  void BackendInvalidRankings();
  void BackendInvalidRankings2();
  void BackendDisable();
  void BackendDisable2();
  void BackendDisable3();
};

void DiskCacheBackendTest::BackendBasics() {
  InitCache();
  disk_cache::Entry *entry1 = NULL, *entry2 = NULL;
  EXPECT_FALSE(cache_->OpenEntry("the first key", &entry1));
  ASSERT_TRUE(cache_->CreateEntry("the first key", &entry1));
  ASSERT_TRUE(NULL != entry1);
  entry1->Close();
  entry1 = NULL;

  ASSERT_TRUE(cache_->OpenEntry("the first key", &entry1));
  ASSERT_TRUE(NULL != entry1);
  entry1->Close();
  entry1 = NULL;

  EXPECT_FALSE(cache_->CreateEntry("the first key", &entry1));
  ASSERT_TRUE(cache_->OpenEntry("the first key", &entry1));
  EXPECT_FALSE(cache_->OpenEntry("some other key", &entry2));
  ASSERT_TRUE(cache_->CreateEntry("some other key", &entry2));
  ASSERT_TRUE(NULL != entry1);
  ASSERT_TRUE(NULL != entry2);
  EXPECT_EQ(2, cache_->GetEntryCount());

  disk_cache::Entry* entry3 = NULL;
  ASSERT_TRUE(cache_->OpenEntry("some other key", &entry3));
  ASSERT_TRUE(NULL != entry3);
  EXPECT_TRUE(entry2 == entry3);
  EXPECT_EQ(2, cache_->GetEntryCount());

  EXPECT_TRUE(cache_->DoomEntry("some other key"));
  EXPECT_EQ(1, cache_->GetEntryCount());
  entry1->Close();
  entry2->Close();
  entry3->Close();

  EXPECT_TRUE(cache_->DoomEntry("the first key"));
  EXPECT_EQ(0, cache_->GetEntryCount());

  ASSERT_TRUE(cache_->CreateEntry("the first key", &entry1));
  ASSERT_TRUE(cache_->CreateEntry("some other key", &entry2));
  entry1->Doom();
  entry1->Close();
  EXPECT_TRUE(cache_->DoomEntry("some other key"));
  EXPECT_EQ(0, cache_->GetEntryCount());
  entry2->Close();
}

TEST_F(DiskCacheBackendTest, Basics) {
  BackendBasics();
}

TEST_F(DiskCacheBackendTest, NewEvictionBasics) {
  SetNewEviction();
  BackendBasics();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyBasics) {
  SetMemoryOnlyMode();
  BackendBasics();
}

void DiskCacheBackendTest::BackendKeying() {
  InitCache();
  const char* kName1 = "the first key";
  const char* kName2 = "the first Key";
  disk_cache::Entry *entry1, *entry2;
  ASSERT_TRUE(cache_->CreateEntry(kName1, &entry1));

  ASSERT_TRUE(cache_->CreateEntry(kName2, &entry2));
  EXPECT_TRUE(entry1 != entry2) << "Case sensitive";
  entry2->Close();

  char buffer[30];
  base::strlcpy(buffer, kName1, arraysize(buffer));
  ASSERT_TRUE(cache_->OpenEntry(buffer, &entry2));
  EXPECT_TRUE(entry1 == entry2);
  entry2->Close();

  base::strlcpy(buffer + 1, kName1, arraysize(buffer) - 1);
  ASSERT_TRUE(cache_->OpenEntry(buffer + 1, &entry2));
  EXPECT_TRUE(entry1 == entry2);
  entry2->Close();

  base::strlcpy(buffer + 3,  kName1, arraysize(buffer) - 3);
  ASSERT_TRUE(cache_->OpenEntry(buffer + 3, &entry2));
  EXPECT_TRUE(entry1 == entry2);
  entry2->Close();

  // Now verify long keys.
  char buffer2[20000];
  memset(buffer2, 's', sizeof(buffer2));
  buffer2[1023] = '\0';
  ASSERT_TRUE(cache_->CreateEntry(buffer2, &entry2)) << "key on block file";
  entry2->Close();

  buffer2[1023] = 'g';
  buffer2[19999] = '\0';
  ASSERT_TRUE(cache_->CreateEntry(buffer2, &entry2)) << "key on external file";
  entry2->Close();
  entry1->Close();
}

TEST_F(DiskCacheBackendTest, Keying) {
  BackendKeying();
}

TEST_F(DiskCacheBackendTest, NewEvictionKeying) {
  SetNewEviction();
  BackendKeying();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyKeying) {
  SetMemoryOnlyMode();
  BackendKeying();
}

TEST_F(DiskCacheBackendTest, ExternalFiles) {
  InitCache();
  // First, lets create a file on the folder.
  std::wstring filename = GetCachePath();
  file_util::AppendToPath(&filename, L"f_000001");

  const int kSize = 50;
  scoped_refptr<net::IOBuffer> buffer1 = new net::IOBuffer(kSize);
  CacheTestFillBuffer(buffer1->data(), kSize, false);
  ASSERT_EQ(kSize, file_util::WriteFile(filename, buffer1->data(), kSize));

  // Now let's create a file with the cache.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache_->CreateEntry("key", &entry));
  ASSERT_EQ(0, entry->WriteData(0, 20000, buffer1, 0, NULL, false));
  entry->Close();

  // And verify that the first file is still there.
  scoped_refptr<net::IOBuffer> buffer2 = new net::IOBuffer(kSize);
  ASSERT_EQ(kSize, file_util::ReadFile(filename, buffer2->data(), kSize));
  EXPECT_EQ(0, memcmp(buffer1->data(), buffer2->data(), kSize));
}

void DiskCacheBackendTest::BackendSetSize() {
  SetDirectMode();
  const int cache_size = 0x10000;  // 64 kB
  SetMaxSize(cache_size);
  InitCache();

  std::string first("some key");
  std::string second("something else");
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache_->CreateEntry(first, &entry));

  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(cache_size);
  memset(buffer->data(), 0, cache_size);
  EXPECT_EQ(cache_size / 10, entry->WriteData(0, 0, buffer, cache_size / 10,
                                              NULL, false)) << "normal file";

  EXPECT_EQ(net::ERR_FAILED, entry->WriteData(1, 0, buffer, cache_size / 5,
                NULL, false)) << "file size above the limit";

  // By doubling the total size, we make this file cacheable.
  SetMaxSize(cache_size * 2);
  EXPECT_EQ(cache_size / 5, entry->WriteData(1, 0, buffer, cache_size / 5,
                                             NULL, false));

  // Let's fill up the cache!.
  SetMaxSize(cache_size * 10);
  EXPECT_EQ(cache_size * 3 / 4, entry->WriteData(0, 0, buffer,
                cache_size * 3 / 4, NULL, false));
  entry->Close();

  SetMaxSize(cache_size);

  // The cache is 95% full.

  ASSERT_TRUE(cache_->CreateEntry(second, &entry));
  EXPECT_EQ(cache_size / 10, entry->WriteData(0, 0, buffer, cache_size / 10,
                                              NULL, false)) << "trim the cache";
  entry->Close();

  EXPECT_FALSE(cache_->OpenEntry(first, &entry));
  ASSERT_TRUE(cache_->OpenEntry(second, &entry));
  EXPECT_EQ(cache_size / 10, entry->GetDataSize(0));
  entry->Close();
}

TEST_F(DiskCacheBackendTest, SetSize) {
  BackendSetSize();
}

TEST_F(DiskCacheBackendTest, NewEvictionSetSize) {
  SetNewEviction();
  BackendSetSize();
}

TEST_F(DiskCacheBackendTest, MemoryOnlySetSize) {
  SetMemoryOnlyMode();
  BackendSetSize();
}

void DiskCacheBackendTest::BackendLoad() {
  InitCache();
  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);

  disk_cache::Entry* entries[100];
  for (int i = 0; i < 100; i++) {
    std::string key = GenerateKey(true);
    ASSERT_TRUE(cache_->CreateEntry(key, &entries[i]));
  }
  EXPECT_EQ(100, cache_->GetEntryCount());

  for (int i = 0; i < 100; i++) {
    int source1 = rand() % 100;
    int source2 = rand() % 100;
    disk_cache::Entry* temp = entries[source1];
    entries[source1] = entries[source2];
    entries[source2] = temp;
  }

  for (int i = 0; i < 100; i++) {
    disk_cache::Entry* entry;
    ASSERT_TRUE(cache_->OpenEntry(entries[i]->GetKey(), &entry));
    EXPECT_TRUE(entry == entries[i]);
    entry->Close();
    entries[i]->Doom();
    entries[i]->Close();
  }
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, Load) {
  // Work with a tiny index table (16 entries)
  SetMask(0xf);
  SetMaxSize(0x100000);
  BackendLoad();
}

TEST_F(DiskCacheBackendTest, NewEvictionLoad) {
  SetNewEviction();
  // Work with a tiny index table (16 entries)
  SetMask(0xf);
  SetMaxSize(0x100000);
  BackendLoad();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyLoad) {
  // Work with a tiny index table (16 entries)
  SetMaxSize(0x100000);
  SetMemoryOnlyMode();
  BackendLoad();
}

// Before looking for invalid entries, let's check a valid entry.
void DiskCacheBackendTest::BackendValidEntry() {
  SetDirectMode();
  InitCache();

  std::string key("Some key");
  disk_cache::Entry* entry1;
  ASSERT_TRUE(cache_->CreateEntry(key, &entry1));

  const int kSize = 50;
  scoped_refptr<net::IOBuffer> buffer1 = new net::IOBuffer(kSize);
  memset(buffer1->data(), 0, kSize);
  base::strlcpy(buffer1->data(), "And the data to save", kSize);
  EXPECT_EQ(kSize, entry1->WriteData(0, 0, buffer1, kSize, NULL, false));
  entry1->Close();
  SimulateCrash();

  ASSERT_TRUE(cache_->OpenEntry(key, &entry1));

  scoped_refptr<net::IOBuffer> buffer2 = new net::IOBuffer(kSize);
  memset(buffer2->data(), 0, kSize);
  EXPECT_EQ(kSize, entry1->ReadData(0, 0, buffer2, kSize, NULL));
  entry1->Close();
  EXPECT_STREQ(buffer1->data(), buffer2->data());
}

TEST_F(DiskCacheBackendTest, ValidEntry) {
  BackendValidEntry();
}

TEST_F(DiskCacheBackendTest, NewEvictionValidEntry) {
  SetNewEviction();
  BackendValidEntry();
}

// The same logic of the previous test (ValidEntry), but this time force the
// entry to be invalid, simulating a crash in the middle.
// We'll be leaking memory from this test.
void DiskCacheBackendTest::BackendInvalidEntry() {
  // Use the implementation directly... we need to simulate a crash.
  SetDirectMode();
  InitCache();

  std::string key("Some key");
  disk_cache::Entry* entry1;
  ASSERT_TRUE(cache_->CreateEntry(key, &entry1));

  const int kSize = 50;
  scoped_refptr<net::IOBuffer> buffer1 = new net::IOBuffer(kSize);
  memset(buffer1->data(), 0, kSize);
  base::strlcpy(buffer1->data(), "And the data to save", kSize);
  EXPECT_EQ(kSize, entry1->WriteData(0, 0, buffer1, kSize, NULL, false));
  SimulateCrash();

  EXPECT_FALSE(cache_->OpenEntry(key, &entry1));
  EXPECT_EQ(0, cache_->GetEntryCount());
}

// This and the other intentionally leaky tests below are excluded from
// purify and valgrind runs by naming them in the files
//   net/data/purify/net_unittests.exe.gtest.txt and
//   net/data/valgrind/net_unittests.gtest.txt
// The scripts tools/{purify,valgrind}/chrome_tests.sh
// read those files and pass the appropriate --gtest_filter to net_unittests.
TEST_F(DiskCacheBackendTest, InvalidEntry) {
  BackendInvalidEntry();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, NewEvictionInvalidEntry) {
  SetNewEviction();
  BackendInvalidEntry();
}

// Almost the same test, but this time crash the cache after reading an entry.
// We'll be leaking memory from this test.
void DiskCacheBackendTest::BackendInvalidEntryRead() {
  // Use the implementation directly... we need to simulate a crash.
  SetDirectMode();
  InitCache();

  std::string key("Some key");
  disk_cache::Entry* entry1;
  ASSERT_TRUE(cache_->CreateEntry(key, &entry1));

  const int kSize = 50;
  scoped_refptr<net::IOBuffer> buffer1 = new net::IOBuffer(kSize);
  memset(buffer1->data(), 0, kSize);
  base::strlcpy(buffer1->data(), "And the data to save", kSize);
  EXPECT_EQ(kSize, entry1->WriteData(0, 0, buffer1, kSize, NULL, false));
  entry1->Close();
  ASSERT_TRUE(cache_->OpenEntry(key, &entry1));
  EXPECT_EQ(kSize, entry1->ReadData(0, 0, buffer1, kSize, NULL));

  SimulateCrash();

  EXPECT_FALSE(cache_->OpenEntry(key, &entry1));
  EXPECT_EQ(0, cache_->GetEntryCount());
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, InvalidEntryRead) {
  BackendInvalidEntryRead();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, NewEvictionInvalidEntryRead) {
  SetNewEviction();
  BackendInvalidEntryRead();
}

// We'll be leaking memory from this test.
void DiskCacheBackendTest::BackendInvalidEntryWithLoad() {
  // Work with a tiny index table (16 entries)
  SetMask(0xf);
  SetMaxSize(0x100000);
  InitCache();

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);

  const int kNumEntries = 100;
  disk_cache::Entry* entries[kNumEntries];
  for (int i = 0; i < kNumEntries; i++) {
    std::string key = GenerateKey(true);
    ASSERT_TRUE(cache_->CreateEntry(key, &entries[i]));
  }
  EXPECT_EQ(kNumEntries, cache_->GetEntryCount());

  for (int i = 0; i < kNumEntries; i++) {
    int source1 = rand() % kNumEntries;
    int source2 = rand() % kNumEntries;
    disk_cache::Entry* temp = entries[source1];
    entries[source1] = entries[source2];
    entries[source2] = temp;
  }

  std::string keys[kNumEntries];
  for (int i = 0; i < kNumEntries; i++) {
    keys[i] = entries[i]->GetKey();
    if (i < kNumEntries / 2)
      entries[i]->Close();
  }

  SimulateCrash();

  for (int i = kNumEntries / 2; i < kNumEntries; i++) {
    disk_cache::Entry* entry;
    EXPECT_FALSE(cache_->OpenEntry(keys[i], &entry));
  }

  for (int i = 0; i < kNumEntries / 2; i++) {
    disk_cache::Entry* entry;
    EXPECT_TRUE(cache_->OpenEntry(keys[i], &entry));
    entry->Close();
  }

  EXPECT_EQ(kNumEntries / 2, cache_->GetEntryCount());
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, InvalidEntryWithLoad) {
  BackendInvalidEntryWithLoad();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, NewEvictionInvalidEntryWithLoad) {
  SetNewEviction();
  BackendInvalidEntryWithLoad();
}

// We'll be leaking memory from this test.
void DiskCacheBackendTest::BackendTrimInvalidEntry() {
  // Use the implementation directly... we need to simulate a crash.
  SetDirectMode();

  const int cache_size = 0x4000;  // 16 kB
  SetMaxSize(cache_size * 10);
  InitCache();

  std::string first("some key");
  std::string second("something else");
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache_->CreateEntry(first, &entry));

  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(cache_size);
  memset(buffer->data(), 0, cache_size);
  EXPECT_EQ(cache_size * 19 / 20, entry->WriteData(0, 0, buffer,
                cache_size * 19 / 20, NULL, false));

  // Simulate a crash.
  SimulateCrash();

  ASSERT_TRUE(cache_->CreateEntry(second, &entry));
  EXPECT_EQ(cache_size / 10, entry->WriteData(0, 0, buffer, cache_size / 10,
                                              NULL, false)) << "trim the cache";
  entry->Close();

  EXPECT_FALSE(cache_->OpenEntry(first, &entry));
  EXPECT_EQ(1, cache_->GetEntryCount());
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, TrimInvalidEntry) {
  BackendTrimInvalidEntry();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, NewEvictionTrimInvalidEntry) {
  SetNewEviction();
  BackendTrimInvalidEntry();
}

void DiskCacheBackendTest::BackendEnumerations() {
  InitCache();
  Time initial = Time::Now();
  int seed = static_cast<int>(initial.ToInternalValue());
  srand(seed);

  const int kNumEntries = 100;
  for (int i = 0; i < kNumEntries; i++) {
    std::string key = GenerateKey(true);
    disk_cache::Entry* entry;
    ASSERT_TRUE(cache_->CreateEntry(key, &entry));
    entry->Close();
  }
  EXPECT_EQ(kNumEntries, cache_->GetEntryCount());
  Time final = Time::Now();

  disk_cache::Entry* entry;
  void* iter = NULL;
  int count = 0;
  Time last_modified[kNumEntries];
  Time last_used[kNumEntries];
  while (cache_->OpenNextEntry(&iter, &entry)) {
    ASSERT_TRUE(NULL != entry);
    if (count < kNumEntries) {
      last_modified[count] = entry->GetLastModified();
      last_used[count] = entry->GetLastUsed();
      EXPECT_TRUE(initial <= last_modified[count]);
      EXPECT_TRUE(final >= last_modified[count]);
    }

    entry->Close();
    count++;
  };
  EXPECT_EQ(kNumEntries, count);

  iter = NULL;
  count = 0;
  // The previous enumeration should not have changed the timestamps.
  while (cache_->OpenNextEntry(&iter, &entry)) {
    ASSERT_TRUE(NULL != entry);
    if (count < kNumEntries) {
      EXPECT_TRUE(last_modified[count] == entry->GetLastModified());
      EXPECT_TRUE(last_used[count] == entry->GetLastUsed());
    }
    entry->Close();
    count++;
  };
  EXPECT_EQ(kNumEntries, count);
}

TEST_F(DiskCacheBackendTest, Enumerations) {
  BackendEnumerations();
}

TEST_F(DiskCacheBackendTest, NewEvictionEnumerations) {
  SetNewEviction();
  BackendEnumerations();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyEnumerations) {
  SetMemoryOnlyMode();
  BackendEnumerations();
}

// Verify handling of invalid entries while doing enumerations.
// We'll be leaking memory from this test.
void DiskCacheBackendTest::BackendInvalidEntryEnumeration() {
  // Use the implementation directly... we need to simulate a crash.
  SetDirectMode();
  InitCache();

  std::string key("Some key");
  disk_cache::Entry *entry, *entry1, *entry2;
  ASSERT_TRUE(cache_->CreateEntry(key, &entry1));

  const int kSize = 50;
  scoped_refptr<net::IOBuffer> buffer1 = new net::IOBuffer(kSize);
  memset(buffer1->data(), 0, kSize);
  base::strlcpy(buffer1->data(), "And the data to save", kSize);
  EXPECT_EQ(kSize, entry1->WriteData(0, 0, buffer1, kSize, NULL, false));
  entry1->Close();
  ASSERT_TRUE(cache_->OpenEntry(key, &entry1));
  EXPECT_EQ(kSize, entry1->ReadData(0, 0, buffer1, kSize, NULL));

  std::string key2("Another key");
  ASSERT_TRUE(cache_->CreateEntry(key2, &entry2));
  entry2->Close();
  ASSERT_EQ(2, cache_->GetEntryCount());

  SimulateCrash();

  void* iter = NULL;
  int count = 0;
  while (cache_->OpenNextEntry(&iter, &entry)) {
    ASSERT_TRUE(NULL != entry);
    EXPECT_EQ(key2, entry->GetKey());
    entry->Close();
    count++;
  };
  EXPECT_EQ(1, count);
  EXPECT_EQ(1, cache_->GetEntryCount());
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, InvalidEntryEnumeration) {
  BackendInvalidEntryEnumeration();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, NewEvictionInvalidEntryEnumeration) {
  SetNewEviction();
  BackendInvalidEntryEnumeration();
}

// Tests that if for some reason entries are modified close to existing cache
// iterators, we don't generate fatal errors or reset the cache.
void DiskCacheBackendTest::BackendFixEnumerators() {
  InitCache();

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);

  const int kNumEntries = 10;
  for (int i = 0; i < kNumEntries; i++) {
    std::string key = GenerateKey(true);
    disk_cache::Entry* entry;
    ASSERT_TRUE(cache_->CreateEntry(key, &entry));
    entry->Close();
  }
  EXPECT_EQ(kNumEntries, cache_->GetEntryCount());

  disk_cache::Entry *entry1, *entry2;
  void* iter1 = NULL;
  void* iter2 = NULL;
  ASSERT_TRUE(cache_->OpenNextEntry(&iter1, &entry1));
  ASSERT_TRUE(NULL != entry1);
  entry1->Close();
  entry1 = NULL;

  // Let's go to the middle of the list.
  for (int i = 0; i < kNumEntries / 2; i++) {
    if (entry1)
      entry1->Close();
    ASSERT_TRUE(cache_->OpenNextEntry(&iter1, &entry1));
    ASSERT_TRUE(NULL != entry1);

    ASSERT_TRUE(cache_->OpenNextEntry(&iter2, &entry2));
    ASSERT_TRUE(NULL != entry2);
    entry2->Close();
  }

  // Messing up with entry1 will modify entry2->next.
  entry1->Doom();
  ASSERT_TRUE(cache_->OpenNextEntry(&iter2, &entry2));
  ASSERT_TRUE(NULL != entry2);

  // The link entry2->entry1 should be broken.
  EXPECT_NE(entry2->GetKey(), entry1->GetKey());
  entry1->Close();
  entry2->Close();

  // And the second iterator should keep working.
  ASSERT_TRUE(cache_->OpenNextEntry(&iter2, &entry2));
  ASSERT_TRUE(NULL != entry2);
  entry2->Close();

  cache_->EndEnumeration(&iter1);
  cache_->EndEnumeration(&iter2);
}

TEST_F(DiskCacheBackendTest, FixEnumerators) {
  BackendFixEnumerators();
}

TEST_F(DiskCacheBackendTest, NewEvictionFixEnumerators) {
  SetNewEviction();
  BackendFixEnumerators();
}

void DiskCacheBackendTest::BackendDoomRecent() {
  InitCache();
  Time initial = Time::Now();

  disk_cache::Entry *entry;
  ASSERT_TRUE(cache_->CreateEntry("first", &entry));
  entry->Close();
  ASSERT_TRUE(cache_->CreateEntry("second", &entry));
  entry->Close();

  PlatformThread::Sleep(20);
  Time middle = Time::Now();

  ASSERT_TRUE(cache_->CreateEntry("third", &entry));
  entry->Close();
  ASSERT_TRUE(cache_->CreateEntry("fourth", &entry));
  entry->Close();

  PlatformThread::Sleep(20);
  Time final = Time::Now();

  ASSERT_EQ(4, cache_->GetEntryCount());
  EXPECT_TRUE(cache_->DoomEntriesSince(final));
  ASSERT_EQ(4, cache_->GetEntryCount());

  EXPECT_TRUE(cache_->DoomEntriesSince(middle));
  ASSERT_EQ(2, cache_->GetEntryCount());

  ASSERT_TRUE(cache_->OpenEntry("second", &entry));
  entry->Close();
}

TEST_F(DiskCacheBackendTest, DoomRecent) {
  BackendDoomRecent();
}

TEST_F(DiskCacheBackendTest, NewEvictionDoomRecent) {
  SetNewEviction();
  BackendDoomRecent();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyDoomRecent) {
  SetMemoryOnlyMode();
  BackendDoomRecent();
}

void DiskCacheBackendTest::BackendDoomBetween() {
  InitCache();
  Time initial = Time::Now();

  disk_cache::Entry *entry;
  ASSERT_TRUE(cache_->CreateEntry("first", &entry));
  entry->Close();

  PlatformThread::Sleep(20);
  Time middle_start = Time::Now();

  ASSERT_TRUE(cache_->CreateEntry("second", &entry));
  entry->Close();
  ASSERT_TRUE(cache_->CreateEntry("third", &entry));
  entry->Close();

  PlatformThread::Sleep(20);
  Time middle_end = Time::Now();

  ASSERT_TRUE(cache_->CreateEntry("fourth", &entry));
  entry->Close();
  ASSERT_TRUE(cache_->OpenEntry("fourth", &entry));
  entry->Close();

  PlatformThread::Sleep(20);
  Time final = Time::Now();

  ASSERT_EQ(4, cache_->GetEntryCount());
  EXPECT_TRUE(cache_->DoomEntriesBetween(middle_start, middle_end));
  ASSERT_EQ(2, cache_->GetEntryCount());

  ASSERT_TRUE(cache_->OpenEntry("fourth", &entry));
  entry->Close();

  EXPECT_TRUE(cache_->DoomEntriesBetween(middle_start, final));
  ASSERT_EQ(1, cache_->GetEntryCount());

  ASSERT_TRUE(cache_->OpenEntry("first", &entry));
  entry->Close();
}

TEST_F(DiskCacheBackendTest, DoomBetween) {
  BackendDoomBetween();
}

TEST_F(DiskCacheBackendTest, NewEvictionDoomBetween) {
  SetNewEviction();
  BackendDoomBetween();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyDoomBetween) {
  SetMemoryOnlyMode();
  BackendDoomBetween();
}

void DiskCacheBackendTest::BackendTransaction(const std::wstring& name,
                                              int num_entries, bool load) {
  success_ = false;
  ASSERT_TRUE(CopyTestCache(name));
  DisableFirstCleanup();

  if (load) {
    SetMask(0xf);
    SetMaxSize(0x100000);
  } else {
    // Clear the settings from the previous run.
    SetMask(0);
    SetMaxSize(0);
  }

  InitCache();
  ASSERT_EQ(num_entries + 1, cache_->GetEntryCount());

  std::string key("the first key");
  disk_cache::Entry* entry1;
  ASSERT_FALSE(cache_->OpenEntry(key, &entry1));

  int actual = cache_->GetEntryCount();
  if (num_entries != actual) {
    ASSERT_TRUE(load);
    // If there is a heavy load, inserting an entry will make another entry
    // dirty (on the hash bucket) so two entries are removed.
    ASSERT_EQ(num_entries - 1, actual);
  }

  delete cache_;
  cache_ = NULL;
  cache_impl_ = NULL;

  ASSERT_TRUE(CheckCacheIntegrity(GetCachePath(), new_eviction_));
  success_ = true;
}

void DiskCacheBackendTest::BackendRecoverInsert() {
  // Tests with an empty cache.
  BackendTransaction(L"insert_empty1", 0, false);
  ASSERT_TRUE(success_) << "insert_empty1";
  BackendTransaction(L"insert_empty2", 0, false);
  ASSERT_TRUE(success_) << "insert_empty2";
  BackendTransaction(L"insert_empty3", 0, false);
  ASSERT_TRUE(success_) << "insert_empty3";

  // Tests with one entry on the cache.
  BackendTransaction(L"insert_one1", 1, false);
  ASSERT_TRUE(success_) << "insert_one1";
  BackendTransaction(L"insert_one2", 1, false);
  ASSERT_TRUE(success_) << "insert_one2";
  BackendTransaction(L"insert_one3", 1, false);
  ASSERT_TRUE(success_) << "insert_one3";

  // Tests with one hundred entries on the cache, tiny index.
  BackendTransaction(L"insert_load1", 100, true);
  ASSERT_TRUE(success_) << "insert_load1";
  BackendTransaction(L"insert_load2", 100, true);
  ASSERT_TRUE(success_) << "insert_load2";
}

TEST_F(DiskCacheBackendTest, RecoverInsert) {
  BackendRecoverInsert();
}

TEST_F(DiskCacheBackendTest, NewEvictionRecoverInsert) {
  SetNewEviction();
  BackendRecoverInsert();
}

void DiskCacheBackendTest::BackendRecoverRemove() {
  // Removing the only element.
  BackendTransaction(L"remove_one1", 0, false);
  ASSERT_TRUE(success_) << "remove_one1";
  BackendTransaction(L"remove_one2", 0, false);
  ASSERT_TRUE(success_) << "remove_one2";
  BackendTransaction(L"remove_one3", 0, false);
  ASSERT_TRUE(success_) << "remove_one3";

  // Removing the head.
  BackendTransaction(L"remove_head1", 1, false);
  ASSERT_TRUE(success_) << "remove_head1";
  BackendTransaction(L"remove_head2", 1, false);
  ASSERT_TRUE(success_) << "remove_head2";
  BackendTransaction(L"remove_head3", 1, false);
  ASSERT_TRUE(success_) << "remove_head3";

  // Removing the tail.
  BackendTransaction(L"remove_tail1", 1, false);
  ASSERT_TRUE(success_) << "remove_tail1";
  BackendTransaction(L"remove_tail2", 1, false);
  ASSERT_TRUE(success_) << "remove_tail2";
  BackendTransaction(L"remove_tail3", 1, false);
  ASSERT_TRUE(success_) << "remove_tail3";

  // Removing with one hundred entries on the cache, tiny index.
  BackendTransaction(L"remove_load1", 100, true);
  ASSERT_TRUE(success_) << "remove_load1";
  BackendTransaction(L"remove_load2", 100, true);
  ASSERT_TRUE(success_) << "remove_load2";
  BackendTransaction(L"remove_load3", 100, true);
  ASSERT_TRUE(success_) << "remove_load3";

#ifdef NDEBUG
  // This case cannot be reverted, so it will assert on debug builds.
  BackendTransaction(L"remove_one4", 0, false);
  ASSERT_TRUE(success_) << "remove_one4";
  BackendTransaction(L"remove_head4", 1, false);
  ASSERT_TRUE(success_) << "remove_head4";
#endif
}

TEST_F(DiskCacheBackendTest, RecoverRemove) {
  BackendRecoverRemove();
}

TEST_F(DiskCacheBackendTest, NewEvictionRecoverRemove) {
  SetNewEviction();
  BackendRecoverRemove();
}

// Tests dealing with cache files that cannot be recovered.
TEST_F(DiskCacheTest, Backend_DeleteOld) {
  ASSERT_TRUE(CopyTestCache(L"wrong_version"));
  std::wstring path = GetCachePath();
  scoped_ptr<disk_cache::Backend> cache;
  cache.reset(disk_cache::CreateCacheBackend(path, true, 0, net::DISK_CACHE));

  MessageLoopHelper helper;

  ASSERT_TRUE(NULL != cache.get());
  ASSERT_EQ(0, cache->GetEntryCount());

  // Wait for a callback that never comes... about 2 secs :). The message loop
  // has to run to allow destruction of the cleaner thread.
  helper.WaitUntilCacheIoFinished(1);
}

// We want to be able to deal with messed up entries on disk.
void DiskCacheBackendTest::BackendInvalidEntry2() {
  ASSERT_TRUE(CopyTestCache(L"bad_entry"));
  DisableFirstCleanup();
  InitCache();

  disk_cache::Entry *entry1, *entry2;
  ASSERT_TRUE(cache_->OpenEntry("the first key", &entry1));
  EXPECT_FALSE(cache_->OpenEntry("some other key", &entry2));
  entry1->Close();

  // CheckCacheIntegrity will fail at this point.
  DisableIntegrityCheck();
}

TEST_F(DiskCacheBackendTest, InvalidEntry2) {
  BackendInvalidEntry2();
}

TEST_F(DiskCacheBackendTest, NewEvictionInvalidEntry2) {
  SetNewEviction();
  BackendInvalidEntry2();
}

// We want to be able to deal with abnormal dirty entries.
void DiskCacheBackendTest::BackendNotMarkedButDirty(const std::wstring& name) {
  ASSERT_TRUE(CopyTestCache(name));
  DisableFirstCleanup();
  InitCache();

  disk_cache::Entry *entry1, *entry2;
  ASSERT_TRUE(cache_->OpenEntry("the first key", &entry1));
  EXPECT_FALSE(cache_->OpenEntry("some other key", &entry2));
  entry1->Close();
}

TEST_F(DiskCacheBackendTest, NotMarkedButDirty) {
  BackendNotMarkedButDirty(L"dirty_entry");
}

TEST_F(DiskCacheBackendTest, NewEvictionNotMarkedButDirty) {
  SetNewEviction();
  BackendNotMarkedButDirty(L"dirty_entry");
}

TEST_F(DiskCacheBackendTest, NotMarkedButDirty2) {
  BackendNotMarkedButDirty(L"dirty_entry2");
}

TEST_F(DiskCacheBackendTest, NewEvictionNotMarkedButDirty2) {
  SetNewEviction();
  BackendNotMarkedButDirty(L"dirty_entry2");
}

// We want to be able to deal with messed up entries on disk.
void DiskCacheBackendTest::BackendInvalidRankings2() {
  ASSERT_TRUE(CopyTestCache(L"bad_rankings"));
  std::wstring path = GetCachePath();
  DisableFirstCleanup();
  InitCache();

  disk_cache::Entry *entry1, *entry2;
  EXPECT_FALSE(cache_->OpenEntry("the first key", &entry1));
  ASSERT_TRUE(cache_->OpenEntry("some other key", &entry2));
  entry2->Close();

  // CheckCacheIntegrity will fail at this point.
  DisableIntegrityCheck();
}

TEST_F(DiskCacheBackendTest, InvalidRankings2) {
  BackendInvalidRankings2();
}

TEST_F(DiskCacheBackendTest, NewEvictionInvalidRankings2) {
  SetNewEviction();
  BackendInvalidRankings2();
}

// If the LRU is corrupt, we delete the cache.
void DiskCacheBackendTest::BackendInvalidRankings() {
  disk_cache::Entry* entry;
  void* iter = NULL;
  ASSERT_TRUE(cache_->OpenNextEntry(&iter, &entry));
  entry->Close();
  EXPECT_EQ(2, cache_->GetEntryCount());

  EXPECT_FALSE(cache_->OpenNextEntry(&iter, &entry));
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, InvalidRankingsSuccess) {
  ASSERT_TRUE(CopyTestCache(L"bad_rankings"));
  DisableFirstCleanup();
  SetDirectMode();
  InitCache();
  BackendInvalidRankings();
}

TEST_F(DiskCacheBackendTest, NewEvictionInvalidRankingsSuccess) {
  ASSERT_TRUE(CopyTestCache(L"bad_rankings"));
  DisableFirstCleanup();
  SetDirectMode();
  SetNewEviction();
  InitCache();
  BackendInvalidRankings();
}

TEST_F(DiskCacheBackendTest, InvalidRankingsFailure) {
  ASSERT_TRUE(CopyTestCache(L"bad_rankings"));
  DisableFirstCleanup();
  SetDirectMode();
  InitCache();
  SetTestMode();  // Fail cache reinitialization.
  BackendInvalidRankings();
}

TEST_F(DiskCacheBackendTest, NewEvictionInvalidRankingsFailure) {
  ASSERT_TRUE(CopyTestCache(L"bad_rankings"));
  DisableFirstCleanup();
  SetDirectMode();
  SetNewEviction();
  InitCache();
  SetTestMode();  // Fail cache reinitialization.
  BackendInvalidRankings();
}

// If the LRU is corrupt and we have open entries, we disable the cache.
void DiskCacheBackendTest::BackendDisable() {
  disk_cache::Entry *entry1, *entry2;
  void* iter = NULL;
  ASSERT_TRUE(cache_->OpenNextEntry(&iter, &entry1));

  EXPECT_FALSE(cache_->OpenNextEntry(&iter, &entry2));
  EXPECT_EQ(2, cache_->GetEntryCount());
  EXPECT_FALSE(cache_->CreateEntry("Something new", &entry2));

  entry1->Close();

  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, DisableSuccess) {
  ASSERT_TRUE(CopyTestCache(L"bad_rankings"));
  DisableFirstCleanup();
  SetDirectMode();
  InitCache();
  BackendDisable();
}

TEST_F(DiskCacheBackendTest, NewEvictionDisableSuccess) {
  ASSERT_TRUE(CopyTestCache(L"bad_rankings"));
  DisableFirstCleanup();
  SetDirectMode();
  SetNewEviction();
  InitCache();
  BackendDisable();
}

TEST_F(DiskCacheBackendTest, DisableFailure) {
  ASSERT_TRUE(CopyTestCache(L"bad_rankings"));
  DisableFirstCleanup();
  SetDirectMode();
  InitCache();
  SetTestMode();  // Fail cache reinitialization.
  BackendDisable();
}

TEST_F(DiskCacheBackendTest, NewEvictionDisableFailure) {
  ASSERT_TRUE(CopyTestCache(L"bad_rankings"));
  DisableFirstCleanup();
  SetDirectMode();
  SetNewEviction();
  InitCache();
  SetTestMode();  // Fail cache reinitialization.
  BackendDisable();
}

// This is another type of corruption on the LRU; disable the cache.
void DiskCacheBackendTest::BackendDisable2() {
  EXPECT_EQ(8, cache_->GetEntryCount());

  disk_cache::Entry* entry;
  void* iter = NULL;
  int count = 0;
  while (cache_->OpenNextEntry(&iter, &entry)) {
    ASSERT_TRUE(NULL != entry);
    entry->Close();
    count++;
    ASSERT_LT(count, 9);
  };

  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, DisableSuccess2) {
  ASSERT_TRUE(CopyTestCache(L"list_loop"));
  DisableFirstCleanup();
  SetDirectMode();
  InitCache();
  BackendDisable2();
}

TEST_F(DiskCacheBackendTest, NewEvictionDisableSuccess2) {
  ASSERT_TRUE(CopyTestCache(L"list_loop"));
  DisableFirstCleanup();
  SetNewEviction();
  SetDirectMode();
  InitCache();
  BackendDisable2();
}

TEST_F(DiskCacheBackendTest, DisableFailure2) {
  ASSERT_TRUE(CopyTestCache(L"list_loop"));
  DisableFirstCleanup();
  SetDirectMode();
  InitCache();
  SetTestMode();  // Fail cache reinitialization.
  BackendDisable2();
}

TEST_F(DiskCacheBackendTest, NewEvictionDisableFailure2) {
  ASSERT_TRUE(CopyTestCache(L"list_loop"));
  DisableFirstCleanup();
  SetDirectMode();
  SetNewEviction();
  InitCache();
  SetTestMode();  // Fail cache reinitialization.
  BackendDisable2();
}

// If the index size changes when we disable the cache, we should not crash.
void DiskCacheBackendTest::BackendDisable3() {
  disk_cache::Entry *entry1, *entry2;
  void* iter = NULL;
  EXPECT_EQ(2, cache_->GetEntryCount());
  ASSERT_TRUE(cache_->OpenNextEntry(&iter, &entry1));
  entry1->Close();

  EXPECT_FALSE(cache_->OpenNextEntry(&iter, &entry2));
  ASSERT_TRUE(cache_->CreateEntry("Something new", &entry2));
  entry2->Close();

  EXPECT_EQ(1, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, DisableSuccess3) {
  ASSERT_TRUE(CopyTestCache(L"bad_rankings2"));
  DisableFirstCleanup();
  SetMaxSize(20 * 1024 * 1024);
  InitCache();
  BackendDisable3();
}

TEST_F(DiskCacheBackendTest, NewEvictionDisableSuccess3) {
  ASSERT_TRUE(CopyTestCache(L"bad_rankings2"));
  DisableFirstCleanup();
  SetMaxSize(20 * 1024 * 1024);
  SetNewEviction();
  InitCache();
  BackendDisable3();
}

TEST_F(DiskCacheTest, Backend_UsageStats) {
  MessageLoopHelper helper;

  std::wstring path = GetCachePath();
  ASSERT_TRUE(DeleteCache(path.c_str()));
  scoped_ptr<disk_cache::BackendImpl> cache;
  cache.reset(new disk_cache::BackendImpl(path));
  ASSERT_TRUE(NULL != cache.get());
  cache->SetUnitTestMode();
  ASSERT_TRUE(cache->Init());

  // Wait for a callback that never comes... about 2 secs :). The message loop
  // has to run to allow invocation of the usage timer.
  helper.WaitUntilCacheIoFinished(1);
}

void DiskCacheBackendTest::BackendDoomAll() {
  InitCache();
  Time initial = Time::Now();

  disk_cache::Entry *entry1, *entry2;
  ASSERT_TRUE(cache_->CreateEntry("first", &entry1));
  ASSERT_TRUE(cache_->CreateEntry("second", &entry2));
  entry1->Close();
  entry2->Close();

  ASSERT_TRUE(cache_->CreateEntry("third", &entry1));
  ASSERT_TRUE(cache_->CreateEntry("fourth", &entry2));

  ASSERT_EQ(4, cache_->GetEntryCount());
  EXPECT_TRUE(cache_->DoomAllEntries());
  ASSERT_EQ(0, cache_->GetEntryCount());

  disk_cache::Entry *entry3, *entry4;
  ASSERT_TRUE(cache_->CreateEntry("third", &entry3));
  ASSERT_TRUE(cache_->CreateEntry("fourth", &entry4));

  EXPECT_TRUE(cache_->DoomAllEntries());
  ASSERT_EQ(0, cache_->GetEntryCount());

  entry1->Close();
  entry2->Close();
  entry3->Doom();  // The entry should be already doomed, but this must work.
  entry3->Close();
  entry4->Close();

  // Now try with all references released.
  ASSERT_TRUE(cache_->CreateEntry("third", &entry1));
  ASSERT_TRUE(cache_->CreateEntry("fourth", &entry2));
  entry1->Close();
  entry2->Close();

  ASSERT_EQ(2, cache_->GetEntryCount());
  EXPECT_TRUE(cache_->DoomAllEntries());
  ASSERT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, DoomAll) {
  BackendDoomAll();
}

TEST_F(DiskCacheBackendTest, NewEvictionDoomAll) {
  SetNewEviction();
  BackendDoomAll();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyDoomAll) {
  SetMemoryOnlyMode();
  BackendDoomAll();
}

// If the index size changes when we doom the cache, we should not crash.
void DiskCacheBackendTest::BackendDoomAll2() {
  EXPECT_EQ(2, cache_->GetEntryCount());
  EXPECT_TRUE(cache_->DoomAllEntries());

  disk_cache::Entry* entry;
  ASSERT_TRUE(cache_->CreateEntry("Something new", &entry));
  entry->Close();

  EXPECT_EQ(1, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, DoomAll2) {
  ASSERT_TRUE(CopyTestCache(L"bad_rankings2"));
  DisableFirstCleanup();
  SetMaxSize(20 * 1024 * 1024);
  InitCache();
  BackendDoomAll2();
}

TEST_F(DiskCacheBackendTest, NewEvictionDoomAll2) {
  ASSERT_TRUE(CopyTestCache(L"bad_rankings2"));
  DisableFirstCleanup();
  SetMaxSize(20 * 1024 * 1024);
  SetNewEviction();
  InitCache();
  BackendDoomAll2();
}

// We should be able to create the same entry on multiple simultaneous instances
// of the cache.
TEST_F(DiskCacheTest, MultipleInstances) {
  ScopedTestCache store1;
  ScopedTestCache store2(L"cache_test2");
  ScopedTestCache store3(L"cache_test3");

  const int kNumberOfCaches = 2;
  scoped_ptr<disk_cache::Backend> cache[kNumberOfCaches];

  cache[0].reset(disk_cache::CreateCacheBackend(store1.path_wstring(), false, 0,
                                                net::DISK_CACHE));
  cache[1].reset(disk_cache::CreateCacheBackend(store2.path_wstring(), false, 0,
                                                net::MEDIA_CACHE));

  ASSERT_TRUE(cache[0].get() != NULL && cache[1].get() != NULL);

  std::string key("the first key");
  disk_cache::Entry* entry;
  for (int i = 0; i < kNumberOfCaches; i++) {
    ASSERT_TRUE(cache[i]->CreateEntry(key, &entry));
    entry->Close();
  }
}

// Test the four regions of the curve that determines the max cache size.
TEST_F(DiskCacheTest, AutomaticMaxSize) {
  const int kDefaultSize = 80 * 1024 * 1024;
  int64 large_size = kDefaultSize;

  EXPECT_EQ(kDefaultSize, disk_cache::PreferedCacheSize(large_size));
  EXPECT_EQ((kDefaultSize / 2) * 8 / 10,
            disk_cache::PreferedCacheSize(large_size / 2));

  EXPECT_EQ(kDefaultSize, disk_cache::PreferedCacheSize(large_size * 2));
  EXPECT_EQ(kDefaultSize, disk_cache::PreferedCacheSize(large_size * 4));
  EXPECT_EQ(kDefaultSize, disk_cache::PreferedCacheSize(large_size * 10));

  EXPECT_EQ(kDefaultSize * 2, disk_cache::PreferedCacheSize(large_size * 20));
  EXPECT_EQ(kDefaultSize * 5 / 2,
            disk_cache::PreferedCacheSize(large_size * 50 / 2));

  EXPECT_EQ(kDefaultSize * 5 / 2,
            disk_cache::PreferedCacheSize(large_size * 51 / 2));
  EXPECT_EQ(kDefaultSize * 5 / 2,
            disk_cache::PreferedCacheSize(large_size * 100 / 2));
  EXPECT_EQ(kDefaultSize * 5 / 2,
            disk_cache::PreferedCacheSize(large_size * 500 / 2));

  EXPECT_EQ(kDefaultSize * 6 / 2,
            disk_cache::PreferedCacheSize(large_size * 600 / 2));
  EXPECT_EQ(kDefaultSize * 7 / 2,
            disk_cache::PreferedCacheSize(large_size * 700 / 2));
}
