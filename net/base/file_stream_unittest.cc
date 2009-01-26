// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

static const char kTestData[] = "0123456789";
static const int kTestDataSize = arraysize(kTestData) - 1;

class FileStreamTest : public PlatformTest {
 public:
  virtual void SetUp() {
    PlatformTest::SetUp();

    file_util::CreateTemporaryFileName(&temp_file_path_);
    file_util::WriteFile(temp_file_path_.ToWStringHack(),
                         kTestData, kTestDataSize);
  }
  virtual void TearDown() {
    file_util::Delete(temp_file_path_, false);

    PlatformTest::TearDown();
  }
  const FilePath temp_file_path() const { return temp_file_path_; }
 private:
  FilePath temp_file_path_;
};

TEST_F(FileStreamTest, BasicOpenClose) {
  net::FileStream stream;
  int rv = stream.Open(temp_file_path(), 
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ);
  EXPECT_EQ(net::OK, rv);
}

TEST_F(FileStreamTest, UseClosedStream) {
  net::FileStream stream;

  EXPECT_FALSE(stream.IsOpen());

  // Try seeking...
  int64 new_offset = stream.Seek(net::FROM_BEGIN, 5);
  EXPECT_EQ(net::ERR_UNEXPECTED, new_offset);

  // Try available...
  int64 avail = stream.Available();
  EXPECT_EQ(net::ERR_UNEXPECTED, avail);

  // Try reading...
  char buf[10];
  int rv = stream.Read(buf, sizeof(buf), NULL);
  EXPECT_EQ(net::ERR_UNEXPECTED, rv);
}

TEST_F(FileStreamTest, BasicRead) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  net::FileStream stream;
  int flags = base::PLATFORM_FILE_OPEN | 
              base::PLATFORM_FILE_READ;
  int rv = stream.Open(temp_file_path(), flags);
  EXPECT_EQ(net::OK, rv);

  int64 total_bytes_avail = stream.Available();
  EXPECT_EQ(file_size, total_bytes_avail);

  int64 total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    char buf[4];
    rv = stream.Read(buf, sizeof(buf), NULL);
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf, rv);
  }
  EXPECT_EQ(file_size, total_bytes_read);
  EXPECT_TRUE(data_read == kTestData);
}

TEST_F(FileStreamTest, AsyncRead) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  net::FileStream stream;
  int flags = base::PLATFORM_FILE_OPEN | 
              base::PLATFORM_FILE_READ | 
              base::PLATFORM_FILE_ASYNC;
  int rv = stream.Open(temp_file_path(), flags);
  EXPECT_EQ(net::OK, rv);

  int64 total_bytes_avail = stream.Available();
  EXPECT_EQ(file_size, total_bytes_avail);

  TestCompletionCallback callback;

  int64 total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    char buf[4];
    rv = stream.Read(buf, sizeof(buf), &callback);
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf, rv);
  }
  EXPECT_EQ(file_size, total_bytes_read);
  EXPECT_TRUE(data_read == kTestData);
}

TEST_F(FileStreamTest, BasicRead_FromOffset) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  net::FileStream stream;
  int flags = base::PLATFORM_FILE_OPEN | 
              base::PLATFORM_FILE_READ;
  int rv = stream.Open(temp_file_path(), flags);
  EXPECT_EQ(net::OK, rv);

  const int64 kOffset = 3;
  int64 new_offset = stream.Seek(net::FROM_BEGIN, kOffset);
  EXPECT_EQ(kOffset, new_offset);

  int64 total_bytes_avail = stream.Available();
  EXPECT_EQ(file_size - kOffset, total_bytes_avail);

  int64 total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    char buf[4];
    rv = stream.Read(buf, sizeof(buf), NULL);
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf, rv);
  }
  EXPECT_EQ(file_size - kOffset, total_bytes_read);
  EXPECT_TRUE(data_read == kTestData + kOffset);
}

TEST_F(FileStreamTest, AsyncRead_FromOffset) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  net::FileStream stream;
  int flags = base::PLATFORM_FILE_OPEN | 
              base::PLATFORM_FILE_READ | 
              base::PLATFORM_FILE_ASYNC;
  int rv = stream.Open(temp_file_path(), flags);
  EXPECT_EQ(net::OK, rv);

  const int64 kOffset = 3;
  int64 new_offset = stream.Seek(net::FROM_BEGIN, kOffset);
  EXPECT_EQ(kOffset, new_offset);

  int64 total_bytes_avail = stream.Available();
  EXPECT_EQ(file_size - kOffset, total_bytes_avail);

  TestCompletionCallback callback;

  int64 total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    char buf[4];
    rv = stream.Read(buf, sizeof(buf), &callback);
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf, rv);
  }
  EXPECT_EQ(file_size - kOffset, total_bytes_read);
  EXPECT_TRUE(data_read == kTestData + kOffset);
}

TEST_F(FileStreamTest, SeekAround) {
  net::FileStream stream;
  int flags = base::PLATFORM_FILE_OPEN | 
              base::PLATFORM_FILE_READ;
  int rv = stream.Open(temp_file_path(), flags);
  EXPECT_EQ(net::OK, rv);

  const int64 kOffset = 3;
  int64 new_offset = stream.Seek(net::FROM_BEGIN, kOffset);
  EXPECT_EQ(kOffset, new_offset);

  new_offset = stream.Seek(net::FROM_CURRENT, kOffset);
  EXPECT_EQ(2 * kOffset, new_offset);

  new_offset = stream.Seek(net::FROM_CURRENT, -kOffset);
  EXPECT_EQ(kOffset, new_offset);

  const int kTestDataLen = arraysize(kTestData) - 1;

  new_offset = stream.Seek(net::FROM_END, -kTestDataLen);
  EXPECT_EQ(0, new_offset);
}

TEST_F(FileStreamTest, BasicWrite) {
  net::FileStream stream;
  int flags = base::PLATFORM_FILE_CREATE_ALWAYS | 
              base::PLATFORM_FILE_WRITE;
  int rv = stream.Open(temp_file_path(), flags);
  EXPECT_EQ(net::OK, rv);

  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(0, file_size);

  rv = stream.Write(kTestData, kTestDataSize, NULL);
  EXPECT_EQ(kTestDataSize, rv);
  stream.Close();

  ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kTestDataSize, file_size);
}

TEST_F(FileStreamTest, AsyncWrite) {
  net::FileStream stream;
  int flags = base::PLATFORM_FILE_CREATE_ALWAYS | 
              base::PLATFORM_FILE_WRITE | 
              base::PLATFORM_FILE_ASYNC;
  int rv = stream.Open(temp_file_path(), flags);
  EXPECT_EQ(net::OK, rv);

  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(0, file_size);

  TestCompletionCallback callback;
  int64 total_bytes_written = 0;

  while (total_bytes_written != kTestDataSize) {
    rv = stream.Write(kTestData, kTestDataSize, &callback);
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LT(0, rv);
    if (rv <= 0)
      break;
    total_bytes_written += rv;
  }
  ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(file_size, total_bytes_written);
}

TEST_F(FileStreamTest, BasicWrite_FromOffset) {
  net::FileStream stream;
  int flags = base::PLATFORM_FILE_OPEN | 
              base::PLATFORM_FILE_WRITE;
  int rv = stream.Open(temp_file_path(), flags);
  EXPECT_EQ(net::OK, rv);

  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kTestDataSize, file_size);

  const int64 kOffset = 0;
  int64 new_offset = stream.Seek(net::FROM_END, kOffset);
  EXPECT_EQ(kTestDataSize, new_offset);

  rv = stream.Write(kTestData, kTestDataSize, NULL);
  EXPECT_EQ(kTestDataSize, rv);
  stream.Close();

  ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kTestDataSize * 2, file_size);
}

TEST_F(FileStreamTest, AsyncWrite_FromOffset) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  net::FileStream stream;
  int flags = base::PLATFORM_FILE_OPEN | 
              base::PLATFORM_FILE_WRITE |
              base::PLATFORM_FILE_ASYNC;
  int rv = stream.Open(temp_file_path(), flags);
  EXPECT_EQ(net::OK, rv);

  const int64 kOffset = 0;
  int64 new_offset = stream.Seek(net::FROM_END, kOffset);
  EXPECT_EQ(kTestDataSize, new_offset);

  TestCompletionCallback callback;
  int64 total_bytes_written = 0;

  while (total_bytes_written != kTestDataSize) {
    rv = stream.Write(kTestData, kTestDataSize, &callback);
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LT(0, rv);
    if (rv <= 0)
      break;
    total_bytes_written += rv;
  }
  ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(file_size, kTestDataSize * 2);
}

TEST_F(FileStreamTest, BasicReadWrite) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  net::FileStream stream;
  int flags = base::PLATFORM_FILE_OPEN | 
              base::PLATFORM_FILE_READ | 
              base::PLATFORM_FILE_WRITE;
  int rv = stream.Open(temp_file_path(), flags);
  EXPECT_EQ(net::OK, rv);

  int64 total_bytes_avail = stream.Available();
  EXPECT_EQ(file_size, total_bytes_avail);

  int64 total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    char buf[4];
    rv = stream.Read(buf, sizeof(buf), NULL);
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf, rv);
  }
  EXPECT_EQ(file_size, total_bytes_read);
  EXPECT_TRUE(data_read == kTestData);

  rv = stream.Write(kTestData, kTestDataSize, NULL);
  EXPECT_EQ(kTestDataSize, rv);
  stream.Close();

  ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kTestDataSize * 2, file_size);
}

// TODO(erikkay): more READ_WRITE tests?
