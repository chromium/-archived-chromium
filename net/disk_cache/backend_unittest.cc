// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
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
  std::wstring path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  file_util::AppendToPath(&path, L"net");
  file_util::AppendToPath(&path, L"data");
  file_util::AppendToPath(&path, L"cache_tests");
  file_util::AppendToPath(&path, name);

  std::wstring dest = GetCachePath();
  if (!DeleteCache(dest.c_str()))
    return false;
  return file_util::CopyDirectory(path, dest, false);
}

// Verifies that we can recover a transaction (insert or remove on the rankings
// list) that is interrupted.
int TestTransaction(const std::wstring& name, int num_entries, bool load) {
  if (!CopyTestCache(name))
    return 1;
  std::wstring path = GetCachePath();
  scoped_ptr<disk_cache::Backend> cache;

  if (!load) {
    cache.reset(disk_cache::CreateCacheBackend(path, false, 0));
  } else {
    disk_cache::BackendImpl* cache2 = new disk_cache::BackendImpl(path, 0xf);
    if (!cache2 || !cache2->SetMaxSize(0x100000) || !cache2->Init())
      return 2;
    cache.reset(cache2);
  }
  if (!cache.get())
    return 2;

  if (num_entries + 1 != cache->GetEntryCount())
    return 3;

  std::string key("the first key");
  disk_cache::Entry* entry1;
  if (cache->OpenEntry(key, &entry1))
    return 4;

  int actual = cache->GetEntryCount();
  if (num_entries != actual) {
    if (!load)
      return 5;
    // If there is a heavy load, inserting an entry will make another entry
    // dirty (on the hash bucket) so two entries are removed.
    if (actual != num_entries - 1)
      return 5;
  }

  cache.reset();

  if (!CheckCacheIntegrity(path))
    return 6;

  return 0;
}

}  // namespace

// Tests that can run with different types of caches.
class DiskCacheBackendTest : public DiskCacheTestWithCache {
 protected:
  void BackendBasics();
  void BackendSetSize();
  void BackendLoad();
  void BackendKeying();
  void BackendEnumerations();
  void BackendDoomRecent();
  void BackendDoomBetween();
  void BackendDoomAll();
  void BackendInvalidRankings();
  void BackendDisable();
  void BackendDisable2();
};

void DiskCacheBackendTest::BackendBasics() {
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
  InitCache();
  BackendBasics();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyBasics) {
  SetMemoryOnlyMode();
  InitCache();
  BackendBasics();
}

void DiskCacheBackendTest::BackendKeying() {
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
  InitCache();
  BackendKeying();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyKeying) {
  SetMemoryOnlyMode();
  InitCache();
  BackendKeying();
}

TEST_F(DiskCacheBackendTest, ExternalFiles) {
  InitCache();
  // First, lets create a file on the folder.
  std::wstring filename = GetCachePath();
  file_util::AppendToPath(&filename, L"f_000001");

  const int kDataSize = 50;
  char data[kDataSize];
  CacheTestFillBuffer(data, kDataSize, false);
  ASSERT_EQ(kDataSize, file_util::WriteFile(filename, data, kDataSize));

  // Now let's create a file with the cache.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache_->CreateEntry("key", &entry));
  ASSERT_EQ(0, entry->WriteData(0, 20000, data, 0, NULL, false));
  entry->Close();

  // And verify that the first file is still there.
  char buffer[kDataSize];
  ASSERT_EQ(kDataSize, file_util::ReadFile(filename, buffer, kDataSize));
  EXPECT_EQ(0, memcmp(data, buffer, kDataSize));
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

  char buffer[cache_size] = {0};
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

  // Verify that the cache is 95% full.
  ASSERT_TRUE(cache_->OpenEntry(first, &entry));
  EXPECT_EQ(cache_size * 3 / 4, entry->GetDataSize(0));
  EXPECT_EQ(cache_size / 5, entry->GetDataSize(1));
  entry->Close();

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

TEST_F(DiskCacheBackendTest, MemoryOnlySetSize) {
  SetMemoryOnlyMode();
  BackendSetSize();
}

void DiskCacheBackendTest::BackendLoad() {
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
  InitCache();
  BackendLoad();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyLoad) {
  // Work with a tiny index table (16 entries)
  SetMaxSize(0x100000);
  SetMemoryOnlyMode();
  InitCache();
  BackendLoad();
}

// Before looking for invalid entries, let's check a valid entry.
TEST_F(DiskCacheBackendTest, ValidEntry) {
  SetDirectMode();
  InitCache();

  std::string key("Some key");
  disk_cache::Entry* entry1;
  ASSERT_TRUE(cache_->CreateEntry(key, &entry1));

  char data[] = "And the data to save";
  EXPECT_TRUE(sizeof(data) == entry1->WriteData(0, 0, data, sizeof(data), NULL,
                                                false));
  entry1->Close();
  SimulateCrash();

  ASSERT_TRUE(cache_->OpenEntry(key, &entry1));

  char buffer[40];
  memset(buffer, 0, sizeof(buffer));
  EXPECT_TRUE(sizeof(data) == entry1->ReadData(0, 0, buffer, sizeof(data),
                                               NULL));
  entry1->Close();
  EXPECT_STREQ(data, buffer);
}

// The same logic of the previous test (ValidEntry), but this time force the
// entry to be invalid, simulating a crash in the middle.
// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, InvalidEntry) {
  // Use the implementation directly... we need to simulate a crash.
  SetDirectMode();
  InitCache();

  std::string key("Some key");
  disk_cache::Entry* entry1;
  ASSERT_TRUE(cache_->CreateEntry(key, &entry1));

  char data[] = "And the data to save";
  EXPECT_TRUE(sizeof(data) == entry1->WriteData(0, 0, data, sizeof(data), NULL,
                                                false));
  SimulateCrash();

  EXPECT_FALSE(cache_->OpenEntry(key, &entry1));
  EXPECT_EQ(0, cache_->GetEntryCount());
}

// Almost the same test, but this time crash the cache after reading an entry.
// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, InvalidEntryRead) {
  // Use the implementation directly... we need to simulate a crash.
  SetDirectMode();
  InitCache();

  std::string key("Some key");
  disk_cache::Entry* entry1;
  ASSERT_TRUE(cache_->CreateEntry(key, &entry1));

  char data[] = "And the data to save";
  EXPECT_TRUE(sizeof(data) == entry1->WriteData(0, 0, data, sizeof(data), NULL,
                                                false));
  entry1->Close();
  ASSERT_TRUE(cache_->OpenEntry(key, &entry1));
  EXPECT_TRUE(sizeof(data) == entry1->ReadData(0, 0, data, sizeof(data), NULL));

  SimulateCrash();

  EXPECT_FALSE(cache_->OpenEntry(key, &entry1));
  EXPECT_EQ(0, cache_->GetEntryCount());
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, InvalidEntryWithLoad) {
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
TEST_F(DiskCacheBackendTest, TrimInvalidEntry) {
  // Use the implementation directly... we need to simulate a crash.
  SetDirectMode();

  const int cache_size = 0x4000;  // 16 kB
  SetMaxSize(cache_size * 10);
  InitCache();

  std::string first("some key");
  std::string second("something else");
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache_->CreateEntry(first, &entry));

  char buffer[cache_size] = {0};
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

void DiskCacheBackendTest::BackendEnumerations() {
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
    }

    EXPECT_TRUE(initial <= last_modified[count]);
    EXPECT_TRUE(final >= last_modified[count]);
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
  InitCache();
  BackendEnumerations();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyEnumerations) {
  SetMemoryOnlyMode();
  InitCache();
  BackendEnumerations();
}

// Verify handling of invalid entries while doing enumerations.
// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, InvalidEntryEnumeration) {
  // Use the implementation directly... we need to simulate a crash.
  SetDirectMode();
  InitCache();

  std::string key("Some key");
  disk_cache::Entry *entry, *entry1, *entry2;
  ASSERT_TRUE(cache_->CreateEntry(key, &entry1));

  char data[] = "And the data to save";
  EXPECT_TRUE(sizeof(data) == entry1->WriteData(0, 0, data, sizeof(data), NULL,
                                                false));
  entry1->Close();
  ASSERT_TRUE(cache_->OpenEntry(key, &entry1));
  EXPECT_TRUE(sizeof(data) == entry1->ReadData(0, 0, data, sizeof(data), NULL));

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

// Tests that if for some reason entries are modified close to existing cache
// iterators, we don't generate fatal errors or reset the cache.
TEST_F(DiskCacheBackendTest, FixEnumerators) {
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

void DiskCacheBackendTest::BackendDoomRecent() {
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

void DiskCacheBackendTest::BackendDoomBetween() {
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

TEST_F(DiskCacheBackendTest, DoomRecent) {
  InitCache();
  BackendDoomRecent();
}

TEST_F(DiskCacheBackendTest, DoomBetween) {
  InitCache();
  BackendDoomBetween();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyDoomRecent) {
  SetMemoryOnlyMode();
  InitCache();
  BackendDoomRecent();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyDoomBetween) {
  SetMemoryOnlyMode();
  InitCache();
  BackendDoomBetween();
}

TEST_F(DiskCacheTest, Backend_RecoverInsert) {
  // Tests with an empty cache.
  EXPECT_EQ(0, TestTransaction(L"insert_empty1", 0, false));
  EXPECT_EQ(0, TestTransaction(L"insert_empty2", 0, false));
  EXPECT_EQ(0, TestTransaction(L"insert_empty3", 0, false));

  // Tests with one entry on the cache.
  EXPECT_EQ(0, TestTransaction(L"insert_one1", 1, false));
  EXPECT_EQ(0, TestTransaction(L"insert_one2", 1, false));
  EXPECT_EQ(0, TestTransaction(L"insert_one3", 1, false));

  // Tests with one hundred entries on the cache, tiny index.
  EXPECT_EQ(0, TestTransaction(L"insert_load1", 100, true));
  EXPECT_EQ(0, TestTransaction(L"insert_load2", 100, true));
}

TEST_F(DiskCacheTest, Backend_RecoverRemove) {
  // Removing the only element.
  EXPECT_EQ(0, TestTransaction(L"remove_one1", 0, false));
  EXPECT_EQ(0, TestTransaction(L"remove_one2", 0, false));
  EXPECT_EQ(0, TestTransaction(L"remove_one3", 0, false));

  // Removing the head.
  EXPECT_EQ(0, TestTransaction(L"remove_head1", 1, false));
  EXPECT_EQ(0, TestTransaction(L"remove_head2", 1, false));
  EXPECT_EQ(0, TestTransaction(L"remove_head3", 1, false));

  // Removing the tail.
  EXPECT_EQ(0, TestTransaction(L"remove_tail1", 1, false));
  EXPECT_EQ(0, TestTransaction(L"remove_tail2", 1, false));
  EXPECT_EQ(0, TestTransaction(L"remove_tail3", 1, false));

  // Removing with one hundred entries on the cache, tiny index.
  EXPECT_EQ(0, TestTransaction(L"remove_load1", 100, true));
  EXPECT_EQ(0, TestTransaction(L"remove_load2", 100, true));
  EXPECT_EQ(0, TestTransaction(L"remove_load3", 100, true));

#ifdef NDEBUG
  // This case cannot be reverted, so it will assert on debug builds.
  EXPECT_EQ(0, TestTransaction(L"remove_one4", 0, false));
  EXPECT_EQ(0, TestTransaction(L"remove_head4", 1, false));
#endif
}

// Tests dealing with cache files that cannot be recovered.
TEST_F(DiskCacheTest, Backend_DeleteOld) {
  ASSERT_TRUE(CopyTestCache(L"wrong_version"));
  std::wstring path = GetCachePath();
  scoped_ptr<disk_cache::Backend> cache;
  cache.reset(disk_cache::CreateCacheBackend(path, true, 0));

  MessageLoopHelper helper;

  ASSERT_TRUE(NULL != cache.get());
  ASSERT_EQ(0, cache->GetEntryCount());

  // Wait for a callback that never comes... about 2 secs :). The message loop
  // has to run to allow destruction of the cleaner thread.
  helper.WaitUntilCacheIoFinished(1);
}

// We want to be able to deal with messed up entries on disk.
TEST_F(DiskCacheTest, Backend_InvalidEntry) {
  ASSERT_TRUE(CopyTestCache(L"bad_entry"));
  std::wstring path = GetCachePath();
  disk_cache::Backend* cache = disk_cache::CreateCacheBackend(path, false, 0);
  ASSERT_TRUE(NULL != cache);

  disk_cache::Entry *entry1, *entry2;
  ASSERT_TRUE(cache->OpenEntry("the first key", &entry1));
  EXPECT_FALSE(cache->OpenEntry("some other key", &entry2));
  entry1->Close();

  // CheckCacheIntegrity will fail at this point.
  delete cache;
}

// We want to be able to deal with messed up entries on disk.
TEST_F(DiskCacheTest, Backend_InvalidRankings) {
  ASSERT_TRUE(CopyTestCache(L"bad_rankings"));
  std::wstring path = GetCachePath();
  disk_cache::Backend* cache = disk_cache::CreateCacheBackend(path, false, 0);
  ASSERT_TRUE(NULL != cache);

  disk_cache::Entry *entry1, *entry2;
  EXPECT_FALSE(cache->OpenEntry("the first key", &entry1));
  ASSERT_TRUE(cache->OpenEntry("some other key", &entry2));
  entry2->Close();

  // CheckCacheIntegrity will fail at this point.
  delete cache;
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

TEST_F(DiskCacheBackendTest, InvalidRankingsFailure) {
  ASSERT_TRUE(CopyTestCache(L"bad_rankings"));
  DisableFirstCleanup();
  SetDirectMode();
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

TEST_F(DiskCacheBackendTest, DisableFailure) {
  ASSERT_TRUE(CopyTestCache(L"bad_rankings"));
  DisableFirstCleanup();
  SetDirectMode();
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

TEST_F(DiskCacheBackendTest, DisableFailure2) {
  ASSERT_TRUE(CopyTestCache(L"list_loop"));
  DisableFirstCleanup();
  SetDirectMode();
  InitCache();
  SetTestMode();  // Fail cache reinitialization.
  BackendDisable2();
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
  InitCache();
  BackendDoomAll();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyDoomAll) {
  SetMemoryOnlyMode();
  InitCache();
  BackendDoomAll();
}

