// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/platform_thread.h"
#include "base/timer.h"
#include "base/string_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache_test_base.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/entry_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;

extern int g_cache_tests_max_id;
extern volatile int g_cache_tests_received;
extern volatile bool g_cache_tests_error;

// Tests that can run with different types of caches.
class DiskCacheEntryTest : public DiskCacheTestWithCache {
 protected:
  void InternalSyncIO();
  void InternalAsyncIO();
  void ExternalSyncIO();
  void ExternalAsyncIO();
  void StreamAccess();
  void GetKey();
  void GrowData();
  void TruncateData();
  void ZeroLengthIO();
  void ReuseEntry(int size);
  void InvalidData();
  void DoomEntry();
  void DoomedEntry();
};

void DiskCacheEntryTest::InternalSyncIO() {
  disk_cache::Entry *entry1 = NULL;
  ASSERT_TRUE(cache_->CreateEntry("the first key", &entry1));
  ASSERT_TRUE(NULL != entry1);

  const int kSize1 = 10;
  scoped_refptr<net::IOBuffer> buffer1 = new net::IOBuffer(kSize1);
  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  EXPECT_EQ(0, entry1->ReadData(0, 0, buffer1, kSize1, NULL));
  base::strlcpy(buffer1->data(), "the data", kSize1);
  EXPECT_EQ(10, entry1->WriteData(0, 0, buffer1, kSize1, NULL, false));
  memset(buffer1->data(), 0, kSize1);
  EXPECT_EQ(10, entry1->ReadData(0, 0, buffer1, kSize1, NULL));
  EXPECT_STREQ("the data", buffer1->data());

  const int kSize2 = 5000;
  const int kSize3 = 10000;
  scoped_refptr<net::IOBuffer> buffer2 = new net::IOBuffer(kSize2);
  scoped_refptr<net::IOBuffer> buffer3 = new net::IOBuffer(kSize3);
  memset(buffer3->data(), 0, kSize3);
  CacheTestFillBuffer(buffer2->data(), kSize2, false);
  base::strlcpy(buffer2->data(), "The really big data goes here", kSize2);
  EXPECT_EQ(5000, entry1->WriteData(1, 1500, buffer2, kSize2, NULL, false));
  memset(buffer2->data(), 0, kSize2);
  EXPECT_EQ(4989, entry1->ReadData(1, 1511, buffer2, kSize2, NULL));
  EXPECT_STREQ("big data goes here", buffer2->data());
  EXPECT_EQ(5000, entry1->ReadData(1, 0, buffer2, kSize2, NULL));
  EXPECT_EQ(0, memcmp(buffer2->data(), buffer3->data(), 1500));
  EXPECT_EQ(1500, entry1->ReadData(1, 5000, buffer2, kSize2, NULL));

  EXPECT_EQ(0, entry1->ReadData(1, 6500, buffer2, kSize2, NULL));
  EXPECT_EQ(6500, entry1->ReadData(1, 0, buffer3, kSize3, NULL));
  EXPECT_EQ(8192, entry1->WriteData(1, 0, buffer3, 8192, NULL, false));
  EXPECT_EQ(8192, entry1->ReadData(1, 0, buffer3, kSize3, NULL));
  EXPECT_EQ(8192, entry1->GetDataSize(1));

  entry1->Doom();
  entry1->Close();
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheEntryTest, InternalSyncIO) {
  InitCache();
  InternalSyncIO();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyInternalSyncIO) {
  SetMemoryOnlyMode();
  InitCache();
  InternalSyncIO();
}

void DiskCacheEntryTest::InternalAsyncIO() {
  disk_cache::Entry *entry1 = NULL;
  ASSERT_TRUE(cache_->CreateEntry("the first key", &entry1));
  ASSERT_TRUE(NULL != entry1);

  // Let's verify that each IO goes to the right callback object.
  CallbackTest callback1(1, false);
  CallbackTest callback2(2, false);
  CallbackTest callback3(3, false);
  CallbackTest callback4(4, false);
  CallbackTest callback5(5, false);
  CallbackTest callback6(6, false);
  CallbackTest callback7(7, false);
  CallbackTest callback8(8, false);
  CallbackTest callback9(9, false);
  CallbackTest callback10(10, false);
  CallbackTest callback11(11, false);
  CallbackTest callback12(12, false);
  CallbackTest callback13(13, false);

  g_cache_tests_error = false;
  g_cache_tests_max_id = 0;
  g_cache_tests_received = 0;

  MessageLoopHelper helper;

  const int kSize1 = 10;
  const int kSize2 = 5000;
  const int kSize3 = 10000;
  scoped_refptr<net::IOBuffer> buffer1 = new net::IOBuffer(kSize1);
  scoped_refptr<net::IOBuffer> buffer2 = new net::IOBuffer(kSize2);
  scoped_refptr<net::IOBuffer> buffer3 = new net::IOBuffer(kSize3);
  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  CacheTestFillBuffer(buffer2->data(), kSize2, false);
  CacheTestFillBuffer(buffer3->data(), kSize3, false);

  EXPECT_EQ(0, entry1->ReadData(0, 0, buffer1, kSize1, &callback1));
  base::strlcpy(buffer1->data(), "the data", kSize1);
  int expected = 0;
  int ret = entry1->WriteData(0, 0, buffer1, kSize1, &callback2, false);
  EXPECT_TRUE(10 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  memset(buffer2->data(), 0, kSize1);
  ret = entry1->ReadData(0, 0, buffer2, kSize1, &callback3);
  EXPECT_TRUE(10 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  g_cache_tests_max_id = 3;
  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_STREQ("the data", buffer2->data());

  base::strlcpy(buffer2->data(), "The really big data goes here", kSize2);
  ret = entry1->WriteData(1, 1500, buffer2, kSize2, &callback4, false);
  EXPECT_TRUE(5000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  memset(buffer3->data(), 0, kSize2);
  ret = entry1->ReadData(1, 1511, buffer3, kSize2, &callback5);
  EXPECT_TRUE(4989 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  g_cache_tests_max_id = 5;
  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_STREQ("big data goes here", buffer3->data());
  ret = entry1->ReadData(1, 0, buffer2, kSize2, &callback6);
  EXPECT_TRUE(5000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  memset(buffer3->data(), 0, kSize3);

  g_cache_tests_max_id = 6;
  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_EQ(0, memcmp(buffer2->data(), buffer3->data(), 1500));
  ret = entry1->ReadData(1, 5000, buffer2, kSize2, &callback7);
  EXPECT_TRUE(1500 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_EQ(0, entry1->ReadData(1, 6500, buffer2, kSize2, &callback8));
  ret = entry1->ReadData(1, 0, buffer3, kSize3, &callback9);
  EXPECT_TRUE(6500 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  ret = entry1->WriteData(1, 0, buffer3, 8192, &callback10, false);
  EXPECT_TRUE(8192 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  ret = entry1->ReadData(1, 0, buffer3, kSize3, &callback11);
  EXPECT_TRUE(8192 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_EQ(8192, entry1->GetDataSize(1));

  ret = entry1->ReadData(0, 0, buffer1, kSize1, &callback12);
  EXPECT_TRUE(10 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  ret = entry1->ReadData(1, 0, buffer2, kSize2, &callback13);
  EXPECT_TRUE(5000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  g_cache_tests_max_id = 13;
  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));

  EXPECT_FALSE(g_cache_tests_error);
  EXPECT_EQ(expected, g_cache_tests_received);

  entry1->Doom();
  entry1->Close();
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheEntryTest, InternalAsyncIO) {
  InitCache();
  InternalAsyncIO();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyInternalAsyncIO) {
  SetMemoryOnlyMode();
  InitCache();
  InternalAsyncIO();
}

void DiskCacheEntryTest::ExternalSyncIO() {
  disk_cache::Entry *entry1;
  ASSERT_TRUE(cache_->CreateEntry("the first key", &entry1));

  const int kSize1 = 17000;
  const int kSize2 = 25000;
  scoped_refptr<net::IOBuffer> buffer1 = new net::IOBuffer(kSize1);
  scoped_refptr<net::IOBuffer> buffer2 = new net::IOBuffer(kSize2);
  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  CacheTestFillBuffer(buffer2->data(), kSize2, false);
  base::strlcpy(buffer1->data(), "the data", kSize1);
  EXPECT_EQ(17000, entry1->WriteData(0, 0, buffer1, kSize1, NULL, false));
  memset(buffer1->data(), 0, kSize1);
  EXPECT_EQ(17000, entry1->ReadData(0, 0, buffer1, kSize1, NULL));
  EXPECT_STREQ("the data", buffer1->data());

  base::strlcpy(buffer2->data(), "The really big data goes here", kSize2);
  EXPECT_EQ(25000, entry1->WriteData(1, 10000, buffer2, kSize2, NULL, false));
  memset(buffer2->data(), 0, kSize2);
  EXPECT_EQ(24989, entry1->ReadData(1, 10011, buffer2, kSize2, NULL));
  EXPECT_STREQ("big data goes here", buffer2->data());
  EXPECT_EQ(25000, entry1->ReadData(1, 0, buffer2, kSize2, NULL));
  EXPECT_EQ(0, memcmp(buffer2->data(), buffer2->data(), 10000));
  EXPECT_EQ(5000, entry1->ReadData(1, 30000, buffer2, kSize2, NULL));

  EXPECT_EQ(0, entry1->ReadData(1, 35000, buffer2, kSize2, NULL));
  EXPECT_EQ(17000, entry1->ReadData(1, 0, buffer1, kSize1, NULL));
  EXPECT_EQ(17000, entry1->WriteData(1, 20000, buffer1, kSize1, NULL, false));
  EXPECT_EQ(37000, entry1->GetDataSize(1));

  entry1->Doom();
  entry1->Close();
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheEntryTest, ExternalSyncIO) {
  InitCache();
  ExternalSyncIO();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyExternalSyncIO) {
  SetMemoryOnlyMode();
  InitCache();
  ExternalSyncIO();
}

void DiskCacheEntryTest::ExternalAsyncIO() {
  disk_cache::Entry *entry1;
  ASSERT_TRUE(cache_->CreateEntry("the first key", &entry1));

  // Let's verify that each IO goes to the right callback object.
  CallbackTest callback1(1, false);
  CallbackTest callback2(2, false);
  CallbackTest callback3(3, false);
  CallbackTest callback4(4, false);
  CallbackTest callback5(5, false);
  CallbackTest callback6(6, false);
  CallbackTest callback7(7, false);
  CallbackTest callback8(8, false);
  CallbackTest callback9(9, false);

  g_cache_tests_error = false;
  g_cache_tests_max_id = 0;
  g_cache_tests_received = 0;
  int expected = 0;

  MessageLoopHelper helper;

  const int kSize1 = 17000;
  const int kSize2 = 25000;
  const int kSize3 = 25000;
  scoped_refptr<net::IOBuffer> buffer1 = new net::IOBuffer(kSize1);
  scoped_refptr<net::IOBuffer> buffer2 = new net::IOBuffer(kSize2);
  scoped_refptr<net::IOBuffer> buffer3 = new net::IOBuffer(kSize3);
  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  CacheTestFillBuffer(buffer2->data(), kSize2, false);
  CacheTestFillBuffer(buffer3->data(), kSize3, false);
  base::strlcpy(buffer1->data(), "the data", kSize1);
  int ret = entry1->WriteData(0, 0, buffer1, kSize1, &callback1, false);
  EXPECT_TRUE(17000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  g_cache_tests_max_id = 1;
  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));

  memset(buffer2->data(), 0, kSize1);
  ret = entry1->ReadData(0, 0, buffer2, kSize1, &callback2);
  EXPECT_TRUE(17000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  g_cache_tests_max_id = 2;
  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_STREQ("the data", buffer1->data());

  base::strlcpy(buffer2->data(), "The really big data goes here", kSize2);
  ret = entry1->WriteData(1, 10000, buffer2, kSize2, &callback3, false);
  EXPECT_TRUE(25000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  g_cache_tests_max_id = 3;
  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));

  memset(buffer3->data(), 0, kSize3);
  ret = entry1->ReadData(1, 10011, buffer3, kSize3, &callback4);
  EXPECT_TRUE(24989 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  g_cache_tests_max_id = 4;
  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_STREQ("big data goes here", buffer3->data());
  ret = entry1->ReadData(1, 0, buffer2, kSize2, &callback5);
  EXPECT_TRUE(25000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  g_cache_tests_max_id = 5;
  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));
  EXPECT_EQ(0, memcmp(buffer2->data(), buffer2->data(), 10000));
  ret = entry1->ReadData(1, 30000, buffer2, kSize2, &callback6);
  EXPECT_TRUE(5000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;

  EXPECT_EQ(0, entry1->ReadData(1, 35000, buffer2, kSize2, &callback7));
  ret = entry1->ReadData(1, 0, buffer1, kSize1, &callback8);
  EXPECT_TRUE(17000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;
  ret = entry1->WriteData(1, 20000, buffer1, kSize1, &callback9, false);
  EXPECT_TRUE(17000 == ret || net::ERR_IO_PENDING == ret);
  if (net::ERR_IO_PENDING == ret)
    expected++;
  EXPECT_EQ(37000, entry1->GetDataSize(1));

  g_cache_tests_max_id = 9;
  EXPECT_TRUE(helper.WaitUntilCacheIoFinished(expected));

  EXPECT_FALSE(g_cache_tests_error);
  EXPECT_EQ(expected, g_cache_tests_received);

  entry1->Doom();
  entry1->Close();
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheEntryTest, ExternalAsyncIO) {
  InitCache();
  ExternalAsyncIO();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyExternalAsyncIO) {
  SetMemoryOnlyMode();
  InitCache();
  ExternalAsyncIO();
}

void DiskCacheEntryTest::StreamAccess() {
  disk_cache::Entry *entry = NULL;
  ASSERT_TRUE(cache_->CreateEntry("the first key", &entry));
  ASSERT_TRUE(NULL != entry);

  const int kBufferSize = 1024;
  scoped_refptr<net::IOBuffer> buffer1 = new net::IOBuffer(kBufferSize);
  scoped_refptr<net::IOBuffer> buffer2 = new net::IOBuffer(kBufferSize);

  const int kNumStreams = 3;
  for (int i = 0; i < kNumStreams; i++) {
    CacheTestFillBuffer(buffer1->data(), kBufferSize, false);
    EXPECT_EQ(kBufferSize, entry->WriteData(i, 0, buffer1, kBufferSize, NULL,
                                            false));
    memset(buffer2->data(), 0, kBufferSize);
    EXPECT_EQ(kBufferSize, entry->ReadData(i, 0, buffer2, kBufferSize, NULL));
    EXPECT_EQ(0, memcmp(buffer1->data(), buffer2->data(), kBufferSize));
  }

  EXPECT_EQ(net::ERR_INVALID_ARGUMENT,
            entry->ReadData(kNumStreams, 0, buffer1, kBufferSize, NULL));
  entry->Close();
}

TEST_F(DiskCacheEntryTest, StreamAccess) {
  InitCache();
  StreamAccess();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyStreamAccess) {
  SetMemoryOnlyMode();
  InitCache();
  StreamAccess();
}

void DiskCacheEntryTest::GetKey() {
  std::string key1("the first key");
  disk_cache::Entry *entry1;
  ASSERT_TRUE(cache_->CreateEntry(key1, &entry1));
  EXPECT_EQ(key1, entry1->GetKey()) << "short key";
  entry1->Close();

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);
  char key_buffer[20000];

  CacheTestFillBuffer(key_buffer, 3000, true);
  key_buffer[1000] = '\0';

  key1 = key_buffer;
  ASSERT_TRUE(cache_->CreateEntry(key1, &entry1));
  EXPECT_TRUE(key1 == entry1->GetKey()) << "1000 bytes key";
  entry1->Close();

  key_buffer[1000] = 'p';
  key_buffer[3000] = '\0';
  key1 = key_buffer;
  ASSERT_TRUE(cache_->CreateEntry(key1, &entry1));
  EXPECT_TRUE(key1 == entry1->GetKey()) << "medium size key";
  entry1->Close();

  CacheTestFillBuffer(key_buffer, sizeof(key_buffer), true);
  key_buffer[19999] = '\0';

  key1 = key_buffer;
  ASSERT_TRUE(cache_->CreateEntry(key1, &entry1));
  EXPECT_TRUE(key1 == entry1->GetKey()) << "long key";
  entry1->Close();
}

TEST_F(DiskCacheEntryTest, GetKey) {
  InitCache();
  GetKey();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyGetKey) {
  SetMemoryOnlyMode();
  InitCache();
  GetKey();
}

void DiskCacheEntryTest::GrowData() {
  std::string key1("the first key");
  disk_cache::Entry *entry1, *entry2;
  ASSERT_TRUE(cache_->CreateEntry(key1, &entry1));

  const int kSize = 20000;
  scoped_refptr<net::IOBuffer> buffer1 = new net::IOBuffer(kSize);
  scoped_refptr<net::IOBuffer> buffer2 = new net::IOBuffer(kSize);
  CacheTestFillBuffer(buffer1->data(), kSize, false);
  memset(buffer2->data(), 0, kSize);

  base::strlcpy(buffer1->data(), "the data", kSize);
  EXPECT_EQ(10, entry1->WriteData(0, 0, buffer1, 10, NULL, false));
  EXPECT_EQ(10, entry1->ReadData(0, 0, buffer2, 10, NULL));
  EXPECT_STREQ("the data", buffer2->data());
  EXPECT_EQ(10, entry1->GetDataSize(0));

  EXPECT_EQ(2000, entry1->WriteData(0, 0, buffer1, 2000, NULL, false));
  EXPECT_EQ(2000, entry1->GetDataSize(0));
  EXPECT_EQ(2000, entry1->ReadData(0, 0, buffer2, 2000, NULL));
  EXPECT_TRUE(!memcmp(buffer1->data(), buffer2->data(), 2000));

  EXPECT_EQ(20000, entry1->WriteData(0, 0, buffer1, kSize, NULL, false));
  EXPECT_EQ(20000, entry1->GetDataSize(0));
  EXPECT_EQ(20000, entry1->ReadData(0, 0, buffer2, kSize, NULL));
  EXPECT_TRUE(!memcmp(buffer1->data(), buffer2->data(), kSize));
  entry1->Close();

  memset(buffer2->data(), 0, kSize);
  ASSERT_TRUE(cache_->CreateEntry("Second key", &entry2));
  EXPECT_EQ(10, entry2->WriteData(0, 0, buffer1, 10, NULL, false));
  EXPECT_EQ(10, entry2->GetDataSize(0));
  entry2->Close();

  // Go from an internal address to a bigger block size.
  ASSERT_TRUE(cache_->OpenEntry("Second key", &entry2));
  EXPECT_EQ(2000, entry2->WriteData(0, 0, buffer1, 2000, NULL, false));
  EXPECT_EQ(2000, entry2->GetDataSize(0));
  EXPECT_EQ(2000, entry2->ReadData(0, 0, buffer2, 2000, NULL));
  EXPECT_TRUE(!memcmp(buffer1->data(), buffer2->data(), 2000));
  entry2->Close();
  memset(buffer2->data(), 0, kSize);

  // Go from an internal address to an external one.
  ASSERT_TRUE(cache_->OpenEntry("Second key", &entry2));
  EXPECT_EQ(20000, entry2->WriteData(0, 0, buffer1, kSize, NULL, false));
  EXPECT_EQ(20000, entry2->GetDataSize(0));
  EXPECT_EQ(20000, entry2->ReadData(0, 0, buffer2, kSize, NULL));
  EXPECT_TRUE(!memcmp(buffer1->data(), buffer2->data(), kSize));
  entry2->Close();
}

TEST_F(DiskCacheEntryTest, GrowData) {
  InitCache();
  GrowData();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyGrowData) {
  SetMemoryOnlyMode();
  InitCache();
  GrowData();
}

void DiskCacheEntryTest::TruncateData() {
  std::string key1("the first key");
  disk_cache::Entry *entry1;
  ASSERT_TRUE(cache_->CreateEntry(key1, &entry1));

  const int kSize1 = 20000;
  const int kSize2 = 20000;
  scoped_refptr<net::IOBuffer> buffer1 = new net::IOBuffer(kSize1);
  scoped_refptr<net::IOBuffer> buffer2 = new net::IOBuffer(kSize2);

  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  memset(buffer2->data(), 0, kSize2);

  // Simple truncation:
  EXPECT_EQ(200, entry1->WriteData(0, 0, buffer1, 200, NULL, false));
  EXPECT_EQ(200, entry1->GetDataSize(0));
  EXPECT_EQ(100, entry1->WriteData(0, 0, buffer1, 100, NULL, false));
  EXPECT_EQ(200, entry1->GetDataSize(0));
  EXPECT_EQ(100, entry1->WriteData(0, 0, buffer1, 100, NULL, true));
  EXPECT_EQ(100, entry1->GetDataSize(0));
  EXPECT_EQ(0, entry1->WriteData(0, 50, buffer1, 0, NULL, true));
  EXPECT_EQ(50, entry1->GetDataSize(0));
  EXPECT_EQ(0, entry1->WriteData(0, 0, buffer1, 0, NULL, true));
  EXPECT_EQ(0, entry1->GetDataSize(0));
  entry1->Close();
  ASSERT_TRUE(cache_->OpenEntry(key1, &entry1));

  // Go to an external file.
  EXPECT_EQ(20000, entry1->WriteData(0, 0, buffer1, 20000, NULL, true));
  EXPECT_EQ(20000, entry1->GetDataSize(0));
  EXPECT_EQ(20000, entry1->ReadData(0, 0, buffer2, 20000, NULL));
  EXPECT_TRUE(!memcmp(buffer1->data(), buffer2->data(), 20000));
  memset(buffer2->data(), 0, kSize2);

  // External file truncation
  EXPECT_EQ(18000, entry1->WriteData(0, 0, buffer1, 18000, NULL, false));
  EXPECT_EQ(20000, entry1->GetDataSize(0));
  EXPECT_EQ(18000, entry1->WriteData(0, 0, buffer1, 18000, NULL, true));
  EXPECT_EQ(18000, entry1->GetDataSize(0));
  EXPECT_EQ(0, entry1->WriteData(0, 17500, buffer1, 0, NULL, true));
  EXPECT_EQ(17500, entry1->GetDataSize(0));

  // And back to an internal block.
  EXPECT_EQ(600, entry1->WriteData(0, 1000, buffer1, 600, NULL, true));
  EXPECT_EQ(1600, entry1->GetDataSize(0));
  EXPECT_EQ(600, entry1->ReadData(0, 1000, buffer2, 600, NULL));
  EXPECT_TRUE(!memcmp(buffer1->data(), buffer2->data(), 600));
  EXPECT_EQ(1000, entry1->ReadData(0, 0, buffer2, 1000, NULL));
  EXPECT_TRUE(!memcmp(buffer1->data(), buffer2->data(), 1000)) <<
      "Preserves previous data";

  // Go from external file to zero length.
  EXPECT_EQ(20000, entry1->WriteData(0, 0, buffer1, 20000, NULL, true));
  EXPECT_EQ(20000, entry1->GetDataSize(0));
  EXPECT_EQ(0, entry1->WriteData(0, 0, buffer1, 0, NULL, true));
  EXPECT_EQ(0, entry1->GetDataSize(0));

  entry1->Close();
}

TEST_F(DiskCacheEntryTest, TruncateData) {
  InitCache();
  TruncateData();

  // We generate asynchronous IO that is not really tracked until completion
  // so we just wait here before running the next test.
  MessageLoopHelper helper;
  helper.WaitUntilCacheIoFinished(1);
}

TEST_F(DiskCacheEntryTest, MemoryOnlyTruncateData) {
  SetMemoryOnlyMode();
  InitCache();
  TruncateData();
}

void DiskCacheEntryTest::ZeroLengthIO() {
  std::string key1("the first key");
  disk_cache::Entry *entry1;
  ASSERT_TRUE(cache_->CreateEntry(key1, &entry1));
  
  EXPECT_EQ(0, entry1->ReadData(0, 0, NULL, 0, NULL));
  EXPECT_EQ(0, entry1->WriteData(0, 0, NULL, 0, NULL, false));

  // This write should extend the entry.
  EXPECT_EQ(0, entry1->WriteData(0, 1000, NULL, 0, NULL, false));
  EXPECT_EQ(0, entry1->ReadData(0, 500, NULL, 0, NULL));
  EXPECT_EQ(0, entry1->ReadData(0, 2000, NULL, 0, NULL));
  EXPECT_EQ(1000, entry1->GetDataSize(0));
  entry1->Close();
}

TEST_F(DiskCacheEntryTest, ZeroLengthIO) {
  InitCache();
  ZeroLengthIO();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyZeroLengthIO) {
  SetMemoryOnlyMode();
  InitCache();
  ZeroLengthIO();
}

// Write more than the total cache capacity but to a single entry. |size| is the
// amount of bytes to write each time.
void DiskCacheEntryTest::ReuseEntry(int size) {
  std::string key1("the first key");
  disk_cache::Entry *entry;
  ASSERT_TRUE(cache_->CreateEntry(key1, &entry));

  entry->Close();
  std::string key2("the second key");
  ASSERT_TRUE(cache_->CreateEntry(key2, &entry));

  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(size);
  CacheTestFillBuffer(buffer->data(), size, false);

  for (int i = 0; i < 15; i++) {
    EXPECT_EQ(0, entry->WriteData(0, 0, buffer, 0, NULL, true));
    EXPECT_EQ(size, entry->WriteData(0, 0, buffer, size, NULL, false));
    entry->Close();
    ASSERT_TRUE(cache_->OpenEntry(key2, &entry));
  }

  entry->Close();
  ASSERT_TRUE(cache_->OpenEntry(key1, &entry)) << "have not evicted this entry";
  entry->Close();
}

TEST_F(DiskCacheEntryTest, ReuseExternalEntry) {
  SetDirectMode();
  SetMaxSize(200 * 1024);
  InitCache();
  ReuseEntry(20 * 1024);
}

TEST_F(DiskCacheEntryTest, MemoryOnlyReuseExternalEntry) {
  SetDirectMode();
  SetMemoryOnlyMode();
  SetMaxSize(200 * 1024);
  InitCache();
  ReuseEntry(20 * 1024);
}

TEST_F(DiskCacheEntryTest, ReuseInternalEntry) {
  SetDirectMode();
  SetMaxSize(100 * 1024);
  InitCache();
  ReuseEntry(10 * 1024);
}

TEST_F(DiskCacheEntryTest, MemoryOnlyReuseInternalEntry) {
  SetDirectMode();
  SetMemoryOnlyMode();
  SetMaxSize(100 * 1024);
  InitCache();
  ReuseEntry(10 * 1024);
}

// Reading somewhere that was not written should return zeros.
void DiskCacheEntryTest::InvalidData() {
  std::string key1("the first key");
  disk_cache::Entry *entry1;
  ASSERT_TRUE(cache_->CreateEntry(key1, &entry1));

  const int kSize1 = 20000;
  const int kSize2 = 20000;
  const int kSize3 = 20000;
  scoped_refptr<net::IOBuffer> buffer1 = new net::IOBuffer(kSize1);
  scoped_refptr<net::IOBuffer> buffer2 = new net::IOBuffer(kSize2);
  scoped_refptr<net::IOBuffer> buffer3 = new net::IOBuffer(kSize3);

  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  memset(buffer2->data(), 0, kSize2);

  // Simple data grow:
  EXPECT_EQ(200, entry1->WriteData(0, 400, buffer1, 200, NULL, false));
  EXPECT_EQ(600, entry1->GetDataSize(0));
  EXPECT_EQ(100, entry1->ReadData(0, 300, buffer3, 100, NULL));
  EXPECT_TRUE(!memcmp(buffer3->data(), buffer2->data(), 100));
  entry1->Close();
  ASSERT_TRUE(cache_->OpenEntry(key1, &entry1));

  // The entry is now on disk. Load it and extend it.
  EXPECT_EQ(200, entry1->WriteData(0, 800, buffer1, 200, NULL, false));
  EXPECT_EQ(1000, entry1->GetDataSize(0));
  EXPECT_EQ(100, entry1->ReadData(0, 700, buffer3, 100, NULL));
  EXPECT_TRUE(!memcmp(buffer3->data(), buffer2->data(), 100));
  entry1->Close();
  ASSERT_TRUE(cache_->OpenEntry(key1, &entry1));

  // This time using truncate.
  EXPECT_EQ(200, entry1->WriteData(0, 1800, buffer1, 200, NULL, true));
  EXPECT_EQ(2000, entry1->GetDataSize(0));
  EXPECT_EQ(100, entry1->ReadData(0, 1500, buffer3, 100, NULL));
  EXPECT_TRUE(!memcmp(buffer3->data(), buffer2->data(), 100));

  // Go to an external file.
  EXPECT_EQ(200, entry1->WriteData(0, 19800, buffer1, 200, NULL, false));
  EXPECT_EQ(20000, entry1->GetDataSize(0));
  EXPECT_EQ(4000, entry1->ReadData(0, 14000, buffer3, 4000, NULL));
  EXPECT_TRUE(!memcmp(buffer3->data(), buffer2->data(), 4000));

  // And back to an internal block.
  EXPECT_EQ(600, entry1->WriteData(0, 1000, buffer1, 600, NULL, true));
  EXPECT_EQ(1600, entry1->GetDataSize(0));
  EXPECT_EQ(600, entry1->ReadData(0, 1000, buffer3, 600, NULL));
  EXPECT_TRUE(!memcmp(buffer3->data(), buffer1->data(), 600));

  // Extend it again.
  EXPECT_EQ(600, entry1->WriteData(0, 2000, buffer1, 600, NULL, false));
  EXPECT_EQ(2600, entry1->GetDataSize(0));
  EXPECT_EQ(200, entry1->ReadData(0, 1800, buffer3, 200, NULL));
  EXPECT_TRUE(!memcmp(buffer3->data(), buffer2->data(), 200));

  // And again (with truncation flag).
  EXPECT_EQ(600, entry1->WriteData(0, 3000, buffer1, 600, NULL, true));
  EXPECT_EQ(3600, entry1->GetDataSize(0));
  EXPECT_EQ(200, entry1->ReadData(0, 2800, buffer3, 200, NULL));
  EXPECT_TRUE(!memcmp(buffer3->data(), buffer2->data(), 200));

  entry1->Close();
}

TEST_F(DiskCacheEntryTest, InvalidData) {
  InitCache();
  InvalidData();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyInvalidData) {
  SetMemoryOnlyMode();
  InitCache();
  InvalidData();
}

void DiskCacheEntryTest::DoomEntry() {
  std::string key1("the first key");
  disk_cache::Entry *entry1;
  ASSERT_TRUE(cache_->CreateEntry(key1, &entry1));
  entry1->Doom();
  entry1->Close();

  const int kSize = 20000;
  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(kSize);
  CacheTestFillBuffer(buffer->data(), kSize, true);
  buffer->data()[19999] = '\0';

  key1 = buffer->data();
  ASSERT_TRUE(cache_->CreateEntry(key1, &entry1));
  EXPECT_EQ(20000, entry1->WriteData(0, 0, buffer, kSize, NULL, false));
  EXPECT_EQ(20000, entry1->WriteData(1, 0, buffer, kSize, NULL, false));
  entry1->Doom();
  entry1->Close();

  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheEntryTest, DoomEntry) {
  InitCache();
  DoomEntry();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyDoomEntry) {
  SetMemoryOnlyMode();
  InitCache();
  DoomEntry();
}

// Verify that basic operations work as expected with doomed entries.
void DiskCacheEntryTest::DoomedEntry() {
  std::string key("the first key");
  disk_cache::Entry *entry;
  ASSERT_TRUE(cache_->CreateEntry(key, &entry));
  entry->Doom();

  EXPECT_EQ(0, cache_->GetEntryCount());
  Time initial = Time::Now();
  PlatformThread::Sleep(20);

  const int kSize1 = 2000;
  const int kSize2 = 2000;
  scoped_refptr<net::IOBuffer> buffer1 = new net::IOBuffer(kSize1);
  scoped_refptr<net::IOBuffer> buffer2 = new net::IOBuffer(kSize2);
  CacheTestFillBuffer(buffer1->data(), kSize1, false);
  memset(buffer2->data(), 0, kSize2);

  EXPECT_EQ(2000, entry->WriteData(0, 0, buffer1, 2000, NULL, false));
  EXPECT_EQ(2000, entry->ReadData(0, 0, buffer2, 2000, NULL));
  EXPECT_EQ(0, memcmp(buffer1->data(), buffer2->data(), kSize1));
  EXPECT_TRUE(initial < entry->GetLastModified());
  EXPECT_TRUE(initial < entry->GetLastUsed());

  entry->Close();
}

TEST_F(DiskCacheEntryTest, DoomedEntry) {
  InitCache();
  DoomEntry();
}

TEST_F(DiskCacheEntryTest, MemoryOnlyDoomedEntry) {
  SetMemoryOnlyMode();
  InitCache();
  DoomEntry();
}

