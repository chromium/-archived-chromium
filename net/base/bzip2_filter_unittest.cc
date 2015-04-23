// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <iostream>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "net/base/bzip2_filter.h"
#include "net/base/filter_unittest.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/bzip2/bzlib.h"

namespace {

const char* kExtraData = "Test Data, More Test Data, Even More Data of Test";
const int kExtraDataBufferSize = 49;
const int kDefaultBufferSize = 4096;
const int kSmallBufferSize = 128;
const int kMaxBufferSize = 1048576;    // 1048576 == 2^20 == 1 MB

const char kApplicationOctetStream[] = "application/octet-stream";

// These tests use the path service, which uses autoreleased objects on the
// Mac, so this needs to be a PlatformTest.
class BZip2FilterUnitTest : public PlatformTest {
 protected:
  virtual void SetUp() {
    PlatformTest::SetUp();

    bzip2_encode_buffer_ = NULL;

    // Get the path of source data file.
    FilePath file_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
    file_path = file_path.AppendASCII("net");
    file_path = file_path.AppendASCII("data");
    file_path = file_path.AppendASCII("filter_unittests");
    file_path = file_path.AppendASCII("google.txt");

    // Read data from the file into buffer.
    file_util::ReadFileToString(file_path, &source_buffer_);

    // Append the extra data to end of source
    source_buffer_.append(kExtraData, kExtraDataBufferSize);

    // Encode the whole data with bzip2 for next testing
    bzip2_data_stream_.reset(new bz_stream);
    ASSERT_TRUE(bzip2_data_stream_.get());
    memset(bzip2_data_stream_.get(), 0, sizeof(bz_stream));

    int result = BZ2_bzCompressInit(bzip2_data_stream_.get(),
                                    9,   // 900k block size
                                    0,   // quiet
                                    0);  // default work factor
    ASSERT_EQ(BZ_OK, result);

    bzip2_encode_buffer_ = new char[kDefaultBufferSize];
    ASSERT_TRUE(bzip2_encode_buffer_ != NULL);
    bzip2_encode_len_ = kDefaultBufferSize;

    bzip2_data_stream_->next_in = const_cast<char*>(source_buffer());
    bzip2_data_stream_->avail_in = source_len();
    bzip2_data_stream_->next_out = bzip2_encode_buffer_;
    bzip2_data_stream_->avail_out = bzip2_encode_len_;
    do {
      result = BZ2_bzCompress(bzip2_data_stream_.get(), BZ_FINISH);
    } while (result == BZ_FINISH_OK);

    ASSERT_EQ(BZ_STREAM_END, result);
    result = BZ2_bzCompressEnd(bzip2_data_stream_.get());
    ASSERT_EQ(BZ_OK, result);
    bzip2_encode_len_ = bzip2_data_stream_->total_out_lo32;

    // Make sure we wrote something; otherwise not sure what to expect
    ASSERT_GT(bzip2_encode_len_, 0);
    ASSERT_LE(bzip2_encode_len_, kDefaultBufferSize);
  }

  virtual void TearDown() {
    delete[] bzip2_encode_buffer_;
    bzip2_encode_buffer_ = NULL;

    PlatformTest::TearDown();
  }

  // Use filter to decode compressed data, and compare the decoding result with
  // the orginal Data.
  // Parameters: Source and source_len are original data and its size.
  // Encoded_source and encoded_source_len are compressed data and its size.
  // Output_buffer_size specifies the size of buffer to read out data from
  // filter.
  // get_extra_data specifies whether get the extra data because maybe some
  // server might send extra data after finish sending compress data.
  void DecodeAndCompareWithFilter(Filter* filter,
                                  const char* source,
                                  int source_len,
                                  const char* encoded_source,
                                  int encoded_source_len,
                                  int output_buffer_size,
                                  bool get_extra_data) {
    // Make sure we have enough space to hold the decoding output.
    ASSERT_LE(source_len, kDefaultBufferSize);
    ASSERT_LE(output_buffer_size, kDefaultBufferSize);

    int total_output_len = kDefaultBufferSize;
    if (get_extra_data)
      total_output_len += kExtraDataBufferSize;
    char decode_buffer[kDefaultBufferSize + kExtraDataBufferSize];
    char* decode_next = decode_buffer;
    int decode_avail_size = total_output_len;

    const char* encode_next = encoded_source;
    int encode_avail_size = encoded_source_len;

    Filter::FilterStatus code = Filter::FILTER_OK;
    while (code != Filter::FILTER_DONE) {
      int encode_data_len;
      if (get_extra_data && !encode_avail_size)
        break;
      encode_data_len = std::min(encode_avail_size,
                                 filter->stream_buffer_size());
      memcpy(filter->stream_buffer()->data(), encode_next, encode_data_len);
      filter->FlushStreamBuffer(encode_data_len);
      encode_next += encode_data_len;
      encode_avail_size -= encode_data_len;

      while (1) {
        int decode_data_len = std::min(decode_avail_size, output_buffer_size);

        code = filter->ReadData(decode_next, &decode_data_len);
        decode_next += decode_data_len;
        decode_avail_size -= decode_data_len;

        ASSERT_TRUE(code != Filter::FILTER_ERROR);

        if (code == Filter::FILTER_NEED_MORE_DATA ||
            code == Filter::FILTER_DONE) {
          if (code == Filter::FILTER_DONE && get_extra_data)
            code = Filter::FILTER_OK;
          else
            break;
        }
      }
    }

    // Compare the decoding result with source data
    int decode_total_data_len = total_output_len - decode_avail_size;
    EXPECT_TRUE(decode_total_data_len == source_len);
    EXPECT_EQ(memcmp(source, decode_buffer, source_len), 0);
  }

  // Unsafe function to use filter to decode compressed data.
  // Parameters: Source and source_len are compressed data and its size.
  // Dest is the buffer for decoding results. Upon entry, *dest_len is the size
  // of the dest buffer. Upon exit, *dest_len is the number of chars written
  // into the buffer.
  Filter::FilterStatus DecodeAllWithFilter(Filter* filter,
                                           const char* source,
                                           int source_len,
                                           char* dest,
                                           int* dest_len) {
    memcpy(filter->stream_buffer()->data(), source, source_len);
    filter->FlushStreamBuffer(source_len);
    return filter->ReadData(dest, dest_len);
  }

  const char* source_buffer() const { return source_buffer_.data(); }
  int source_len() const {
    return static_cast<int>(source_buffer_.size()) - kExtraDataBufferSize;
  }

  std::string source_buffer_;

  scoped_ptr<bz_stream> bzip2_data_stream_;
  char* bzip2_encode_buffer_;
  int bzip2_encode_len_;
};

// Basic scenario: decoding bzip2 data with big enough buffer.
TEST_F(BZip2FilterUnitTest, DecodeBZip2) {
  // Decode the compressed data with filter
  std::vector<Filter::FilterType> filter_types;
  filter_types.push_back(Filter::FILTER_TYPE_BZIP2);
  MockFilterContext filter_context(kDefaultBufferSize);
  scoped_ptr<Filter> filter(Filter::Factory(filter_types, filter_context));
  ASSERT_TRUE(filter.get());
  memcpy(filter->stream_buffer()->data(), bzip2_encode_buffer_,
         bzip2_encode_len_);
  filter->FlushStreamBuffer(bzip2_encode_len_);

  char bzip2_decode_buffer[kDefaultBufferSize];
  int bzip2_decode_size = kDefaultBufferSize;
  Filter::FilterStatus result =
      filter->ReadData(bzip2_decode_buffer, &bzip2_decode_size);
  ASSERT_EQ(Filter::FILTER_DONE, result);

  // Compare the decoding result with source data
  EXPECT_TRUE(bzip2_decode_size == source_len());
  EXPECT_EQ(memcmp(source_buffer(), bzip2_decode_buffer, source_len()), 0);
}

// Tests we can call filter repeatedly to get all the data decoded.
// To do that, we create a filter with a small buffer that can not hold all
// the input data.
TEST_F(BZip2FilterUnitTest, DecodeWithSmallInputBuffer) {
  std::vector<Filter::FilterType> filter_types;
  filter_types.push_back(Filter::FILTER_TYPE_BZIP2);
  MockFilterContext filter_context(kSmallBufferSize);
  scoped_ptr<Filter> filter(Filter::Factory(filter_types, filter_context));
  ASSERT_TRUE(filter.get());
  DecodeAndCompareWithFilter(filter.get(), source_buffer(), source_len(),
                             bzip2_encode_buffer_, bzip2_encode_len_,
                             kDefaultBufferSize, false);
}

// Tests we can decode when caller has small buffer to read out from filter.
TEST_F(BZip2FilterUnitTest, DecodeWithSmallOutputBuffer) {
  std::vector<Filter::FilterType> filter_types;
  filter_types.push_back(Filter::FILTER_TYPE_BZIP2);
  MockFilterContext filter_context(kDefaultBufferSize);
  scoped_ptr<Filter> filter(Filter::Factory(filter_types, filter_context));
  ASSERT_TRUE(filter.get());
  DecodeAndCompareWithFilter(filter.get(), source_buffer(), source_len(),
                             bzip2_encode_buffer_, bzip2_encode_len_,
                             kSmallBufferSize, false);
}

// Tests we can still decode with just 1 byte buffer in the filter.
// The purpose of this tests are two: (1) Verify filter can parse partial BZip2
// header correctly. (2) Sometimes the filter will consume input without
// generating output. Verify filter can handle it correctly.
TEST_F(BZip2FilterUnitTest, DecodeWithOneByteInputBuffer) {
  std::vector<Filter::FilterType> filter_types;
  filter_types.push_back(Filter::FILTER_TYPE_BZIP2);
  MockFilterContext filter_context(1);
  scoped_ptr<Filter> filter(Filter::Factory(filter_types, filter_context));
  ASSERT_TRUE(filter.get());
  DecodeAndCompareWithFilter(filter.get(), source_buffer(), source_len(),
                             bzip2_encode_buffer_, bzip2_encode_len_,
                             kDefaultBufferSize, false);
}

// Tests we can still decode with just 1 byte buffer in the filter and just 1
// byte buffer in the caller.
TEST_F(BZip2FilterUnitTest, DecodeWithOneByteInputAndOutputBuffer) {
  std::vector<Filter::FilterType> filter_types;
  filter_types.push_back(Filter::FILTER_TYPE_BZIP2);
  MockFilterContext filter_context(1);
  scoped_ptr<Filter> filter(Filter::Factory(filter_types, filter_context));
  ASSERT_TRUE(filter.get());
  DecodeAndCompareWithFilter(filter.get(), source_buffer(), source_len(),
                             bzip2_encode_buffer_, bzip2_encode_len_, 1, false);
}

// Decoding bzip2 stream with corrupted data.
TEST_F(BZip2FilterUnitTest, DecodeCorruptedData) {
  char corrupt_data[kDefaultBufferSize];
  int corrupt_data_len = bzip2_encode_len_;
  memcpy(corrupt_data, bzip2_encode_buffer_, bzip2_encode_len_);

  char corrupt_decode_buffer[kDefaultBufferSize];
  int corrupt_decode_size = kDefaultBufferSize;

  // Decode the correct data with filter
  std::vector<Filter::FilterType> filter_types;
  filter_types.push_back(Filter::FILTER_TYPE_BZIP2);
  MockFilterContext filter_context(kDefaultBufferSize);
  scoped_ptr<Filter> filter1(Filter::Factory(filter_types, filter_context));
  ASSERT_TRUE(filter1.get());

  Filter::FilterStatus code = DecodeAllWithFilter(filter1.get(),
                                                  corrupt_data,
                                                  corrupt_data_len,
                                                  corrupt_decode_buffer,
                                                  &corrupt_decode_size);

  // Expect failures
  EXPECT_TRUE(code == Filter::FILTER_DONE);

  // Decode the corrupted data with filter
  scoped_ptr<Filter> filter2(Filter::Factory(filter_types, filter_context));
  ASSERT_TRUE(filter2.get());

  int pos = corrupt_data_len / 2;
  corrupt_data[pos] = !corrupt_data[pos];

  code = DecodeAllWithFilter(filter2.get(),
                             corrupt_data,
                             corrupt_data_len,
                             corrupt_decode_buffer,
                             &corrupt_decode_size);

  // Expect failures
  EXPECT_TRUE(code != Filter::FILTER_DONE);
}

// Decoding bzip2 stream with missing data.
TEST_F(BZip2FilterUnitTest, DecodeMissingData) {
  char corrupt_data[kDefaultBufferSize];
  int corrupt_data_len = bzip2_encode_len_;
  memcpy(corrupt_data, bzip2_encode_buffer_, bzip2_encode_len_);

  int pos = corrupt_data_len / 2;
  int len = corrupt_data_len - pos - 1;
  memmove(&corrupt_data[pos], &corrupt_data[pos+1], len);
  --corrupt_data_len;

  // Decode the corrupted data with filter
  std::vector<Filter::FilterType> filter_types;
  filter_types.push_back(Filter::FILTER_TYPE_BZIP2);
  MockFilterContext filter_context(kDefaultBufferSize);
  scoped_ptr<Filter> filter(Filter::Factory(filter_types, filter_context));
  ASSERT_TRUE(filter.get());
  char corrupt_decode_buffer[kDefaultBufferSize];
  int corrupt_decode_size = kDefaultBufferSize;

  Filter::FilterStatus code = DecodeAllWithFilter(filter.get(),
                                                  corrupt_data,
                                                  corrupt_data_len,
                                                  corrupt_decode_buffer,
                                                  &corrupt_decode_size);
  // Expect failures
  EXPECT_TRUE(code != Filter::FILTER_DONE);
}

// Decoding bzip2 stream with corrupted header.
TEST_F(BZip2FilterUnitTest, DecodeCorruptedHeader) {
  char corrupt_data[kDefaultBufferSize];
  int corrupt_data_len = bzip2_encode_len_;
  memcpy(corrupt_data, bzip2_encode_buffer_, bzip2_encode_len_);

  corrupt_data[2] = !corrupt_data[2];

  // Decode the corrupted data with filter
  std::vector<Filter::FilterType> filter_types;
  filter_types.push_back(Filter::FILTER_TYPE_BZIP2);
  MockFilterContext filter_context(kDefaultBufferSize);
  scoped_ptr<Filter> filter(Filter::Factory(filter_types, filter_context));
  ASSERT_TRUE(filter.get());
  char corrupt_decode_buffer[kDefaultBufferSize];
  int corrupt_decode_size = kDefaultBufferSize;

  Filter::FilterStatus code = DecodeAllWithFilter(filter.get(),
                                                  corrupt_data,
                                                  corrupt_data_len,
                                                  corrupt_decode_buffer,
                                                  &corrupt_decode_size);

  // Expect failures
  EXPECT_TRUE(code == Filter::FILTER_ERROR);
}

// Tests we can decode all compress data and get extra data which is
// appended  to compress data stream by some server when it finish
// sending compress data.
TEST_F(BZip2FilterUnitTest, DecodeWithExtraDataAndSmallOutputBuffer) {
  char more_data[kDefaultBufferSize + kExtraDataBufferSize];
  int more_data_len = bzip2_encode_len_ + kExtraDataBufferSize;
  memcpy(more_data, bzip2_encode_buffer_, bzip2_encode_len_);
  memcpy(more_data + bzip2_encode_len_, kExtraData, kExtraDataBufferSize);

  std::vector<Filter::FilterType> filter_types;
  filter_types.push_back(Filter::FILTER_TYPE_BZIP2);
  MockFilterContext filter_context(kDefaultBufferSize);
  scoped_ptr<Filter> filter(Filter::Factory(filter_types, filter_context));
  ASSERT_TRUE(filter.get());
  DecodeAndCompareWithFilter(filter.get(),
                             source_buffer(),
                             source_len() + kExtraDataBufferSize,
                             more_data,
                             more_data_len,
                             kSmallBufferSize,
                             true);
}

TEST_F(BZip2FilterUnitTest, DecodeWithExtraDataAndSmallInputBuffer) {
  char more_data[kDefaultBufferSize + kExtraDataBufferSize];
  int more_data_len = bzip2_encode_len_ + kExtraDataBufferSize;
  memcpy(more_data, bzip2_encode_buffer_, bzip2_encode_len_);
  memcpy(more_data + bzip2_encode_len_, kExtraData, kExtraDataBufferSize);

  std::vector<Filter::FilterType> filter_types;
  filter_types.push_back(Filter::FILTER_TYPE_BZIP2);
  MockFilterContext filter_context(kSmallBufferSize);
  scoped_ptr<Filter> filter(Filter::Factory(filter_types, filter_context));
  ASSERT_TRUE(filter.get());
  DecodeAndCompareWithFilter(filter.get(),
                             source_buffer(),
                             source_len() + kExtraDataBufferSize,
                             more_data,
                             more_data_len,
                             kDefaultBufferSize,
                             true);
}

}  // namespace
