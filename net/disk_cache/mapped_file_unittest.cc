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
}

}  // namespace

TEST(DiskCacheTest, MappedFile_SyncIO) {
  std::wstring filename = GetCachePath();
  file_util::AppendToPath(&filename, L"a_test");
  scoped_refptr<disk_cache::MappedFile> file(new disk_cache::MappedFile);
  ASSERT_TRUE(CreateCacheTestFile(filename.c_str()));
  ASSERT_TRUE(file->Init(filename, 8192));

  char buffer1[20];
  char buffer2[20];
  CacheTestFillBuffer(buffer1, sizeof(buffer1), false);
  strcpy_s(buffer1, "the data");
  EXPECT_TRUE(file->Write(buffer1, sizeof(buffer1), 8192));
  EXPECT_TRUE(file->Read(buffer2, sizeof(buffer2), 8192));
  EXPECT_STREQ(buffer1, buffer2);
}

TEST(DiskCacheTest, MappedFile_AsyncIO) {
  std::wstring filename = GetCachePath();
  file_util::AppendToPath(&filename, L"a_test");
  scoped_refptr<disk_cache::MappedFile> file(new disk_cache::MappedFile);
  ASSERT_TRUE(CreateCacheTestFile(filename.c_str()));
  ASSERT_TRUE(file->Init(filename, 8192));

  FileCallbackTest callback(1);
  g_cache_tests_error = false;
  g_cache_tests_max_id = 0;
  g_cache_tests_received = 0;

  char buffer1[20];
  char buffer2[20];
  CacheTestFillBuffer(buffer1, sizeof(buffer1), false);
  strcpy_s(buffer1, "the data");
  bool completed;
  EXPECT_TRUE(file->Write(buffer1, sizeof(buffer1), 1024 * 1024, &callback,
              &completed));
  int expected = completed ? 0 : 1;

  g_cache_tests_max_id = 1;
  WaitForCallbacks(expected);

  EXPECT_TRUE(file->Read(buffer2, sizeof(buffer2), 1024 * 1024, &callback,
              &completed));
  if (!completed)
    expected++;

  WaitForCallbacks(expected);

  EXPECT_EQ(expected, g_cache_tests_received);
  EXPECT_FALSE(g_cache_tests_error);
  EXPECT_STREQ(buffer1, buffer2);
}
