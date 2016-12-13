// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

const char kTestData[] = "0123456789";
const int kTestDataSize = arraysize(kTestData) - 1;

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

// Test the use of FileStream with a file handle provided at construction.
TEST_F(FileStreamTest, UseFileHandle) {
  bool created = false;

  // 1. Test reading with a file handle.
  ASSERT_EQ(kTestDataSize,
      file_util::WriteFile(temp_file_path(), kTestData, kTestDataSize));
  int flags = base::PLATFORM_FILE_OPEN_ALWAYS | base::PLATFORM_FILE_READ;
  base::PlatformFile file = base::CreatePlatformFile(
      temp_file_path().ToWStringHack(), flags, &created);

  // Seek to the beginning of the file and read.
  net::FileStream read_stream(file, flags);
  ASSERT_EQ(0, read_stream.Seek(net::FROM_BEGIN, 0));
  ASSERT_EQ(kTestDataSize, read_stream.Available());
  // Read into buffer and compare.
  char buffer[kTestDataSize];
  ASSERT_EQ(kTestDataSize, read_stream.Read(buffer, kTestDataSize, NULL));
  ASSERT_EQ(0, memcmp(kTestData, buffer, kTestDataSize));
  read_stream.Close();

  // 2. Test writing with a file handle.
  file_util::Delete(temp_file_path(), false);
  flags = base::PLATFORM_FILE_OPEN_ALWAYS | base::PLATFORM_FILE_WRITE;
  file = base::CreatePlatformFile(temp_file_path().ToWStringHack(),
                                  flags, &created);

  net::FileStream write_stream(file, flags);
  ASSERT_EQ(0, write_stream.Seek(net::FROM_BEGIN, 0));
  ASSERT_EQ(kTestDataSize, write_stream.Write(kTestData, kTestDataSize, NULL));
  write_stream.Close();

  // Read into buffer and compare to make sure the handle worked fine.
  ASSERT_EQ(kTestDataSize,
      file_util::ReadFile(temp_file_path(), buffer, kTestDataSize));
  ASSERT_EQ(0, memcmp(kTestData, buffer, kTestDataSize));
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
  int rv = stream.Read(buf, arraysize(buf), NULL);
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

  int total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    char buf[4];
    rv = stream.Read(buf, arraysize(buf), NULL);
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf, rv);
  }
  EXPECT_EQ(file_size, total_bytes_read);
  EXPECT_EQ(kTestData, data_read);
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

  int total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    char buf[4];
    rv = stream.Read(buf, arraysize(buf), &callback);
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf, rv);
  }
  EXPECT_EQ(file_size, total_bytes_read);
  EXPECT_EQ(kTestData, data_read);
}

TEST_F(FileStreamTest, AsyncRead_EarlyClose) {
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

  char buf[4];
  rv = stream.Read(buf, arraysize(buf), &callback);
  stream.Close();
  if (rv < 0) {
    EXPECT_EQ(net::ERR_IO_PENDING, rv);
    // The callback should not be called if the request is cancelled.
    MessageLoop::current()->RunAllPending();
    EXPECT_FALSE(callback.have_result());
  } else {
    EXPECT_EQ(std::string(kTestData, rv), std::string(buf, rv));
  }
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
    rv = stream.Read(buf, arraysize(buf), NULL);
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf, rv);
  }
  EXPECT_EQ(file_size - kOffset, total_bytes_read);
  EXPECT_TRUE(data_read == kTestData + kOffset);
  EXPECT_EQ(kTestData + kOffset, data_read);
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

  int total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    char buf[4];
    rv = stream.Read(buf, arraysize(buf), &callback);
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf, rv);
  }
  EXPECT_EQ(file_size - kOffset, total_bytes_read);
  EXPECT_EQ(kTestData + kOffset, data_read);
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
  int total_bytes_written = 0;

  while (total_bytes_written != kTestDataSize) {
    rv = stream.Write(kTestData + total_bytes_written,
                      kTestDataSize - total_bytes_written,
                      &callback);
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

TEST_F(FileStreamTest, AsyncWrite_EarlyClose) {
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
  int total_bytes_written = 0;

  rv = stream.Write(kTestData + total_bytes_written,
                    kTestDataSize - total_bytes_written,
                    &callback);
  stream.Close();
  if (rv < 0) {
    EXPECT_EQ(net::ERR_IO_PENDING, rv);
    // The callback should not be called if the request is cancelled.
    MessageLoop::current()->RunAllPending();
    EXPECT_FALSE(callback.have_result());
  } else {
    ok = file_util::GetFileSize(temp_file_path(), &file_size);
    EXPECT_TRUE(ok);
    EXPECT_EQ(file_size, rv);
  }
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
  int total_bytes_written = 0;

  while (total_bytes_written != kTestDataSize) {
    rv = stream.Write(kTestData + total_bytes_written,
                      kTestDataSize - total_bytes_written,
                      &callback);
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

  int total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    char buf[4];
    rv = stream.Read(buf, arraysize(buf), NULL);
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

TEST_F(FileStreamTest, BasicWriteRead) {
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

  int64 offset = stream.Seek(net::FROM_END, 0);
  EXPECT_EQ(offset, file_size);

  rv = stream.Write(kTestData, kTestDataSize, NULL);
  EXPECT_EQ(kTestDataSize, rv);

  offset = stream.Seek(net::FROM_BEGIN, 0);
  EXPECT_EQ(0, offset);

  int64 total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    char buf[4];
    rv = stream.Read(buf, arraysize(buf), NULL);
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf, rv);
  }
  stream.Close();

  ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kTestDataSize * 2, file_size);
  EXPECT_EQ(kTestDataSize * 2, total_bytes_read);

  const std::string kExpectedFileData =
      std::string(kTestData) + std::string(kTestData);
  EXPECT_EQ(kExpectedFileData, data_read);
}

TEST_F(FileStreamTest, BasicAsyncReadWrite) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  net::FileStream stream;
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_WRITE |
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
    rv = stream.Read(buf, arraysize(buf), &callback);
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

  int total_bytes_written = 0;

  while (total_bytes_written != kTestDataSize) {
    rv = stream.Write(kTestData + total_bytes_written,
                      kTestDataSize - total_bytes_written,
                      &callback);
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LT(0, rv);
    if (rv <= 0)
      break;
    total_bytes_written += rv;
  }

  stream.Close();

  ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kTestDataSize * 2, file_size);
}

TEST_F(FileStreamTest, BasicAsyncWriteRead) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  net::FileStream stream;
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_WRITE |
              base::PLATFORM_FILE_ASYNC;
  int rv = stream.Open(temp_file_path(), flags);
  EXPECT_EQ(net::OK, rv);

  int64 total_bytes_avail = stream.Available();
  EXPECT_EQ(file_size, total_bytes_avail);

  int64 offset = stream.Seek(net::FROM_END, 0);
  EXPECT_EQ(offset, file_size);

  TestCompletionCallback callback;
  int total_bytes_written = 0;

  while (total_bytes_written != kTestDataSize) {
    rv = stream.Write(kTestData + total_bytes_written,
                      kTestDataSize - total_bytes_written,
                      &callback);
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LT(0, rv);
    if (rv <= 0)
      break;
    total_bytes_written += rv;
  }

  EXPECT_EQ(kTestDataSize, total_bytes_written);

  offset = stream.Seek(net::FROM_BEGIN, 0);
  EXPECT_EQ(0, offset);

  int total_bytes_read = 0;

  std::string data_read;
  for (;;) {
    char buf[4];
    rv = stream.Read(buf, arraysize(buf), &callback);
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_LE(0, rv);
    if (rv <= 0)
      break;
    total_bytes_read += rv;
    data_read.append(buf, rv);
  }
  stream.Close();

  ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kTestDataSize * 2, file_size);

  EXPECT_EQ(kTestDataSize * 2, total_bytes_read);
  const std::string kExpectedFileData =
      std::string(kTestData) + std::string(kTestData);
  EXPECT_EQ(kExpectedFileData, data_read);
}

class TestWriteReadCompletionCallback : public Callback1<int>::Type {
 public:
  explicit TestWriteReadCompletionCallback(
      net::FileStream* stream,
      int* total_bytes_written,
      int* total_bytes_read,
      std::string* data_read)
      : result_(0),
        have_result_(false),
        waiting_for_result_(false),
        stream_(stream),
        total_bytes_written_(total_bytes_written),
        total_bytes_read_(total_bytes_read),
        data_read_(data_read) {}

  int WaitForResult() {
    DCHECK(!waiting_for_result_);
    while (!have_result_) {
      waiting_for_result_ = true;
      MessageLoop::current()->Run();
      waiting_for_result_ = false;
    }
    have_result_ = false;  // auto-reset for next callback
    return result_;
  }

 private:
  virtual void RunWithParams(const Tuple1<int>& params) {
    DCHECK_LT(0, params.a);
    *total_bytes_written_ += params.a;

    int rv;

    if (*total_bytes_written_ != kTestDataSize) {
      // Recurse to finish writing all data.
      int total_bytes_written = 0, total_bytes_read = 0;
      std::string data_read;
      TestWriteReadCompletionCallback callback(
          stream_, &total_bytes_written, &total_bytes_read, &data_read);
      rv = stream_->Write(kTestData + *total_bytes_written_,
                          kTestDataSize - *total_bytes_written_,
                          &callback);
      DCHECK_EQ(net::ERR_IO_PENDING, rv);
      rv = callback.WaitForResult();
      *total_bytes_written_ += total_bytes_written;
      *total_bytes_read_ += total_bytes_read;
      *data_read_ += data_read;
    } else {  // We're done writing all data.  Start reading the data.
      stream_->Seek(net::FROM_BEGIN, 0);

      TestCompletionCallback callback;
      for (;;) {
        char buf[4];
        rv = stream_->Read(buf, arraysize(buf), &callback);
        if (rv == net::ERR_IO_PENDING) {
          bool old_state = MessageLoop::current()->NestableTasksAllowed();
          MessageLoop::current()->SetNestableTasksAllowed(true);
          rv = callback.WaitForResult();
          MessageLoop::current()->SetNestableTasksAllowed(old_state);
        }
        EXPECT_LE(0, rv);
        if (rv <= 0)
          break;
        *total_bytes_read_ += rv;
        data_read_->append(buf, rv);
      }
    }

    result_ = *total_bytes_written_;
    have_result_ = true;
    if (waiting_for_result_)
      MessageLoop::current()->Quit();
  }

  int result_;
  bool have_result_;
  bool waiting_for_result_;
  net::FileStream* stream_;
  int* total_bytes_written_;
  int* total_bytes_read_;
  std::string* data_read_;

  DISALLOW_COPY_AND_ASSIGN(TestWriteReadCompletionCallback);
};

TEST_F(FileStreamTest, AsyncWriteRead) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  net::FileStream stream;
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_WRITE |
              base::PLATFORM_FILE_ASYNC;
  int rv = stream.Open(temp_file_path(), flags);
  EXPECT_EQ(net::OK, rv);

  int64 total_bytes_avail = stream.Available();
  EXPECT_EQ(file_size, total_bytes_avail);

  int64 offset = stream.Seek(net::FROM_END, 0);
  EXPECT_EQ(offset, file_size);

  int total_bytes_written = 0;
  int total_bytes_read = 0;
  std::string data_read;
  TestWriteReadCompletionCallback callback(&stream, &total_bytes_written,
                                           &total_bytes_read, &data_read);

  rv = stream.Write(kTestData + total_bytes_written,
                    kTestDataSize - static_cast<int>(total_bytes_written),
                    &callback);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_LT(0, rv);
  EXPECT_EQ(kTestDataSize, total_bytes_written);

  stream.Close();

  ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kTestDataSize * 2, file_size);

  EXPECT_EQ(kTestDataSize * 2, total_bytes_read);
  const std::string kExpectedFileData =
      std::string(kTestData) + std::string(kTestData);
  EXPECT_EQ(kExpectedFileData, data_read);
}

class TestWriteCloseCompletionCallback : public Callback1<int>::Type {
 public:
  explicit TestWriteCloseCompletionCallback(net::FileStream* stream,
                                            int* total_bytes_written)
      : result_(0),
        have_result_(false),
        waiting_for_result_(false),
        stream_(stream),
        total_bytes_written_(total_bytes_written) {}

  int WaitForResult() {
    DCHECK(!waiting_for_result_);
    while (!have_result_) {
      waiting_for_result_ = true;
      MessageLoop::current()->Run();
      waiting_for_result_ = false;
    }
    have_result_ = false;  // auto-reset for next callback
    return result_;
  }

 private:
  virtual void RunWithParams(const Tuple1<int>& params) {
    DCHECK_LT(0, params.a);
    *total_bytes_written_ += params.a;

    int rv;

    if (*total_bytes_written_ != kTestDataSize) {
      // Recurse to finish writing all data.
      int total_bytes_written = 0;
      TestWriteCloseCompletionCallback callback(stream_, &total_bytes_written);
      rv = stream_->Write(kTestData + *total_bytes_written_,
                          kTestDataSize - *total_bytes_written_,
                          &callback);
      DCHECK_EQ(net::ERR_IO_PENDING, rv);
      rv = callback.WaitForResult();
      *total_bytes_written_ += total_bytes_written;
    } else {  // We're done writing all data.  Close the file.
      stream_->Close();
    }

    result_ = *total_bytes_written_;
    have_result_ = true;
    if (waiting_for_result_)
      MessageLoop::current()->Quit();
  }

  int result_;
  bool have_result_;
  bool waiting_for_result_;
  net::FileStream* stream_;
  int* total_bytes_written_;

  DISALLOW_COPY_AND_ASSIGN(TestWriteCloseCompletionCallback);
};

TEST_F(FileStreamTest, AsyncWriteClose) {
  int64 file_size;
  bool ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);

  net::FileStream stream;
  int flags = base::PLATFORM_FILE_OPEN |
              base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_WRITE |
              base::PLATFORM_FILE_ASYNC;
  int rv = stream.Open(temp_file_path(), flags);
  EXPECT_EQ(net::OK, rv);

  int64 total_bytes_avail = stream.Available();
  EXPECT_EQ(file_size, total_bytes_avail);

  int64 offset = stream.Seek(net::FROM_END, 0);
  EXPECT_EQ(offset, file_size);

  int total_bytes_written = 0;
  TestWriteCloseCompletionCallback callback(&stream, &total_bytes_written);

  rv = stream.Write(kTestData, kTestDataSize, &callback);
  if (rv == net::ERR_IO_PENDING)
    total_bytes_written = callback.WaitForResult();
  EXPECT_LT(0, total_bytes_written);
  EXPECT_EQ(kTestDataSize, total_bytes_written);

  ok = file_util::GetFileSize(temp_file_path(), &file_size);
  EXPECT_TRUE(ok);
  EXPECT_EQ(kTestDataSize * 2, file_size);
}

// Tests truncating a file.
TEST_F(FileStreamTest, Truncate) {
  int flags = base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE;

  net::FileStream write_stream;
  ASSERT_EQ(net::OK, write_stream.Open(temp_file_path(), flags));

  // Write some data to the file.
  const char test_data[] = "0123456789";
  write_stream.Write(test_data, arraysize(test_data), NULL);

  // Truncate the file.
  ASSERT_EQ(4, write_stream.Truncate(4));

  // Write again.
  write_stream.Write(test_data, 4, NULL);

  // Close the stream.
  write_stream.Close();

  // Read in the contents and make sure we get back what we expected.
  std::string read_contents;
  file_util::ReadFileToString(temp_file_path(), &read_contents);

  EXPECT_EQ("01230123", read_contents);
}

}  // namespace
