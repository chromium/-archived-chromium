// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "media/base/seekable_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SeekableBufferTest : public testing::Test {
 protected:
  virtual void SetUp() {
    // Setup seed.
    size_t seed = static_cast<int32>(base::Time::Now().ToInternalValue());
    srand(seed);
    LOG(INFO) << "Random seed: " << seed;

    // Creates a test data.
    data_.reset(new uint8[kDataSize]);
    for (size_t i = 0; i < kDataSize; i++)
      data_.get()[i] = static_cast<char>(rand());

    // Creates a temp buffer.
    write_buffer_.reset(new uint8[kDataSize]);

    // Setup |buffer_|.
    buffer_.reset(new media::SeekableBuffer(kBufferSize, kBufferSize));
  }

  size_t GetRandomInt(size_t maximum) {
    return rand() % maximum + 1;
  }

  static const size_t kDataSize = 409600;
  static const size_t kBufferSize = 4096;
  scoped_ptr<media::SeekableBuffer> buffer_;
  scoped_array<uint8> data_;
  scoped_array<uint8> write_buffer_;
};

TEST_F(SeekableBufferTest, RandomReadWrite) {
  size_t write_position = 0;
  size_t read_position = 0;
  while (read_position < kDataSize) {
    // Write a random amount of data.
    size_t write_size = GetRandomInt(kBufferSize);
    write_size = std::min(write_size, kDataSize - write_position);
    bool should_append =
        buffer_->Append(write_size, data_.get() + write_position);
    write_position += write_size;
    EXPECT_GE(write_position, read_position);
    EXPECT_EQ(write_position - read_position, buffer_->forward_bytes());
    EXPECT_EQ(should_append, buffer_->forward_bytes() < kBufferSize)
      << "Incorrect buffer full reported";

    // Read a random amount of data.
    size_t read_size = GetRandomInt(kBufferSize);
    size_t bytes_read = buffer_->Read(read_size, write_buffer_.get());
    EXPECT_GE(read_size, bytes_read);
    EXPECT_EQ(0, memcmp(write_buffer_.get(),
                        data_.get() + read_position,
                        bytes_read));
    read_position += bytes_read;
    EXPECT_GE(write_position, read_position);
    EXPECT_EQ(write_position - read_position, buffer_->forward_bytes());
  }
}

TEST_F(SeekableBufferTest, ReadWriteSeek) {
  const size_t kWriteSize = 512;
  const size_t kReadSize = kWriteSize / 4;

  size_t write_position = 0;
  size_t read_position = 0;
  size_t forward_bytes = 0;
  for (int i = 0; i < 10; ++i) {
    // Write until buffer is full.
    for (int j = kBufferSize / kWriteSize; j > 0; --j) {
      bool should_append =
          buffer_->Append(kWriteSize, data_.get() + write_position);
      EXPECT_EQ(j > 1, should_append) << "Incorrect buffer full reported";
      write_position += kWriteSize;
      forward_bytes += kWriteSize;
      EXPECT_EQ(forward_bytes, buffer_->forward_bytes());
    }

    // Simulate a read and seek pattern. Each loop reads 4 times, each time
    // reading a quarter of |kWriteSize|.
    for (size_t j = 0; j < kBufferSize / kWriteSize; ++j) {
      // Read.
      EXPECT_EQ(kReadSize, buffer_->Read(kReadSize, write_buffer_.get()));
      forward_bytes -= kReadSize;
      EXPECT_EQ(forward_bytes, buffer_->forward_bytes());
      EXPECT_EQ(0, memcmp(write_buffer_.get(),
                          data_.get() + read_position,
                          kReadSize));
      read_position += kReadSize;

      // Seek forward.
      EXPECT_TRUE(buffer_->Seek(2 * kReadSize));
      forward_bytes -= 2 * kReadSize;
      read_position += 2 * kReadSize;
      EXPECT_EQ(forward_bytes, buffer_->forward_bytes());

      // Read.
      EXPECT_EQ(kReadSize, buffer_->Read(kReadSize, write_buffer_.get()));
      forward_bytes -= kReadSize;
      EXPECT_EQ(forward_bytes, buffer_->forward_bytes());
      EXPECT_EQ(0, memcmp(write_buffer_.get(),
                          data_.get() + read_position,
                          kReadSize));
      read_position += kReadSize;

      // Seek backward.
      EXPECT_TRUE(buffer_->Seek(-3 * static_cast<int32>(kReadSize)));
      forward_bytes += 3 * kReadSize;
      read_position -= 3 * kReadSize;
      EXPECT_EQ(forward_bytes, buffer_->forward_bytes());

      // Read.
      EXPECT_EQ(kReadSize, buffer_->Read(kReadSize, write_buffer_.get()));
      forward_bytes -= kReadSize;
      EXPECT_EQ(forward_bytes, buffer_->forward_bytes());
      EXPECT_EQ(0, memcmp(write_buffer_.get(),
                          data_.get() + read_position,
                          kReadSize));
      read_position += kReadSize;

      // Read.
      EXPECT_EQ(kReadSize, buffer_->Read(kReadSize, write_buffer_.get()));
      forward_bytes -= kReadSize;
      EXPECT_EQ(forward_bytes, buffer_->forward_bytes());
      EXPECT_EQ(0, memcmp(write_buffer_.get(),
                          data_.get() + read_position,
                          kReadSize));
      read_position += kReadSize;

      // Seek forward.
      EXPECT_TRUE(buffer_->Seek(kReadSize));
      forward_bytes -= kReadSize;
      read_position += kReadSize;
      EXPECT_EQ(forward_bytes, buffer_->forward_bytes());
    }
  }
}

TEST_F(SeekableBufferTest, BufferFull) {
  const size_t kWriteSize = 512;

  // Write and expect the buffer to be not full.
  size_t write_position = 0;
  for (size_t i = 0; i < kBufferSize / kWriteSize - 1; ++i) {
    EXPECT_TRUE(buffer_->Append(kWriteSize, data_.get() + write_position));
    write_position += kWriteSize;
    EXPECT_EQ(write_position, buffer_->forward_bytes());
  }

  // Write 10 more times, the buffer is full.
  for (int i = 0; i < 10; ++i) {
    EXPECT_FALSE(buffer_->Append(kWriteSize, data_.get() + write_position));
    write_position += kWriteSize;
    EXPECT_EQ(write_position, buffer_->forward_bytes());
  }

  // Read until the buffer is empty.
  size_t read_position = 0;
  while (buffer_->forward_bytes()) {
    // Read a random amount of data.
    size_t read_size = GetRandomInt(kBufferSize);
    size_t forward_bytes = buffer_->forward_bytes();
    size_t bytes_read = buffer_->Read(read_size, write_buffer_.get());
    EXPECT_EQ(0, memcmp(write_buffer_.get(),
                        data_.get() + read_position,
                        bytes_read));
    if (read_size > forward_bytes)
      EXPECT_EQ(forward_bytes, bytes_read);
    else
      EXPECT_EQ(read_size, bytes_read);
    read_position += bytes_read;
    EXPECT_GE(write_position, read_position);
    EXPECT_EQ(write_position - read_position, buffer_->forward_bytes());
  }

  // Expect we have no bytes left.
  EXPECT_EQ(0u, buffer_->forward_bytes());
  EXPECT_EQ(0u, buffer_->Read(1, write_buffer_.get()));
}

TEST_F(SeekableBufferTest, SeekBackward) {
  EXPECT_EQ(0u, buffer_->forward_bytes());
  EXPECT_EQ(0u, buffer_->backward_bytes());
  EXPECT_FALSE(buffer_->Seek(1));
  EXPECT_FALSE(buffer_->Seek(-1));

  const size_t kWriteSize = 512;
  const size_t kReadSize = 256;

  // Write into buffer until it's full.
  size_t write_position = 0;
  for (size_t i = 0; i < kBufferSize / kWriteSize; ++i) {
    // Write a random amount of data.
    buffer_->Append(kWriteSize, data_.get() + write_position);
    write_position += kWriteSize;
  }

  // Read until buffer is empty.
  size_t read_position = 0;
  for (size_t i = 0; i < kBufferSize / kReadSize; ++i) {
    EXPECT_EQ(kReadSize, buffer_->Read(kReadSize, write_buffer_.get()));
    EXPECT_EQ(0, memcmp(write_buffer_.get(),
                        data_.get() + read_position,
                        kReadSize));
    read_position += kReadSize;
  }

  // Seek backward.
  EXPECT_TRUE(buffer_->Seek(-static_cast<int32>(kBufferSize)));
  EXPECT_FALSE(buffer_->Seek(-1));

  // Read again.
  read_position = 0;
  for (size_t i = 0; i < kBufferSize / kReadSize; ++i) {
    EXPECT_EQ(kReadSize, buffer_->Read(kReadSize, write_buffer_.get()));
    EXPECT_EQ(0, memcmp(write_buffer_.get(),
                        data_.get() + read_position,
                        kReadSize));
    read_position += kReadSize;
  }
}

TEST_F(SeekableBufferTest, SeekForward) {
  size_t write_position = 0;
  size_t read_position = 0;
  while (read_position < kDataSize) {
    for (int i = 0; i < 10; ++i) {
      // Write a random amount of data.
      size_t write_size = GetRandomInt(kBufferSize);
      write_size = std::min(write_size, kDataSize - write_position);
      if (!write_size)
        break;
      bool should_append =
        buffer_->Append(write_size, data_.get() + write_position);
      write_position += write_size;
      EXPECT_GE(write_position, read_position);
      EXPECT_EQ(write_position - read_position, buffer_->forward_bytes());
      EXPECT_EQ(should_append, buffer_->forward_bytes() < kBufferSize)
         << "Incorrect buffer full status reported";
    }

    // Read a random amount of data.
    size_t seek_size = GetRandomInt(kBufferSize);
    if (buffer_->Seek(seek_size))
      read_position += seek_size;
    EXPECT_GE(write_position, read_position);
    EXPECT_EQ(write_position - read_position, buffer_->forward_bytes());

    // Read a random amount of data.
    size_t read_size = GetRandomInt(kBufferSize);
    size_t bytes_read = buffer_->Read(read_size, write_buffer_.get());
    EXPECT_GE(read_size, bytes_read);
    EXPECT_EQ(0, memcmp(write_buffer_.get(),
                        data_.get() + read_position,
                        bytes_read));
    read_position += bytes_read;
    EXPECT_GE(write_position, read_position);
    EXPECT_EQ(write_position - read_position, buffer_->forward_bytes());
  }
}

TEST_F(SeekableBufferTest, AllMethods) {
  EXPECT_EQ(0u, buffer_->Read(0, write_buffer_.get()));
  EXPECT_EQ(0u, buffer_->Read(1, write_buffer_.get()));
  EXPECT_TRUE(buffer_->Seek(0));
  EXPECT_FALSE(buffer_->Seek(-1));
  EXPECT_FALSE(buffer_->Seek(1));
  EXPECT_EQ(0u, buffer_->forward_bytes());
  EXPECT_EQ(0u, buffer_->backward_bytes());
}

}
