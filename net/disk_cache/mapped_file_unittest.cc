// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "net/disk_cache/disk_cache_test_base.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/mapped_file.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

int g_cache_tests_max_id;
volatile int g_cache_tests_received;
volatile bool g_cache_tests_error;

// Implementation of FileIOCallback for the tests.
class FileCallbackTest: public disk_cache::FileIOCallback {
 public:
  explicit FileCallbackTest(int id) : id_(id), reuse_(0) {}
  explicit FileCallbackTest(int id, bool reuse)
      : id_(id), reuse_(reuse_ ? 0 : 1) {}
  ~FileCallbackTest() {}

  virtual void OnFileIOComplete(int bytes_copied);
 private:
  int id_;
  int reuse_;
};

void FileCallbackTest::OnFileIOComplete(int bytes_copied) {
  if (id_ > g_cache_tests_max_id) {
    NOTREACHED();
    g_cache_tests_error = true;
  } else if (reuse_) {
    DCHECK(1 == reuse_);
    if (2 == reuse_)
      g_cache_tests_error = true;
    reuse_++;
  }

  g_cache_tests_received++;
}

// Wait up to 2 secs without callbacks, or until we receive expected callbacks.
void WaitForCallbacks(int expected) {
  if (!expected)
    return;

#if defined(OS_WIN)
  int iterations = 0;
  int last = 0;
  while (iterations < 40) {
    SleepEx(50, TRUE);
    if (expected == g_cache_tests_received)
      return;
    if (last == g_cache_tests_received)
      iterations++;
    else
      iterations = 0;
  }
#elif defined(OS_POSIX)
  // TODO(rvargas): Do something when async IO is implemented.
#endif
}

}  // namespace

TEST_F(DiskCacheTest, MappedFile_SyncIO) {
  std::wstring filename = GetCachePath();
  file_util::AppendToPath(&filename, L"a_test");
  scoped_refptr<disk_cache::MappedFile> file(new disk_cache::MappedFile);
  ASSERT_TRUE(CreateCacheTestFile(filename.c_str()));
  ASSERT_TRUE(file->Init(filename, 8192));

  char buffer1[20];
  char buffer2[20];
  CacheTestFillBuffer(buffer1, sizeof(buffer1), false);
  base::strlcpy(buffer1, "the data", arraysize(buffer1));
  EXPECT_TRUE(file->Write(buffer1, sizeof(buffer1), 8192));
  EXPECT_TRUE(file->Read(buffer2, sizeof(buffer2), 8192));
  EXPECT_STREQ(buffer1, buffer2);
}

TEST_F(DiskCacheTest, MappedFile_AsyncIO) {
  std::wstring filename = GetCachePath();
  file_util::AppendToPath(&filename, L"a_test");
  scoped_refptr<disk_cache::MappedFile> file(new disk_cache::MappedFile);
  ASSERT_TRUE(CreateCacheTestFile(filename.c_str()));
  ASSERT_TRUE(file->Init(filename, 8192));

  FileCallbackTest callback(1);
  g_cache_tests_error = false;
  g_cache_tests_max_id = 0;
  g_cache_tests_received = 0;

  MessageLoopHelper helper;

  char buffer1[20];
  char buffer2[20];
  CacheTestFillBuffer(buffer1, sizeof(buffer1), false);
  base::strlcpy(buffer1, "the data", arraysize(buffer1));
  bool completed;
  EXPECT_TRUE(file->Write(buffer1, sizeof(buffer1), 1024 * 1024, &callback,
              &completed));
  int expected = completed ? 0 : 1;

  g_cache_tests_max_id = 1;
  helper.WaitUntilCacheIoFinished(expected);

  EXPECT_TRUE(file->Read(buffer2, sizeof(buffer2), 1024 * 1024, &callback,
              &completed));
  if (!completed)
    expected++;

  helper.WaitUntilCacheIoFinished(expected);

  EXPECT_EQ(expected, g_cache_tests_received);
  EXPECT_FALSE(g_cache_tests_error);
  EXPECT_STREQ(buffer1, buffer2);
}

