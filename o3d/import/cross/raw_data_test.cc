/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// Tests functionality of the RawData class

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/error.h"
#include "import/cross/memory_buffer.h"
#include "import/cross/raw_data.h"

namespace o3d {

// Test fixture for RawData testing.
class RawDataTest : public testing::Test {
};

// Test RawData
TEST_F(RawDataTest, Basic) {
  String uri("test_filename");
  const int kBufferLength = 1024;

  // Create a buffer and initialize it will some values
  MemoryBuffer<uint8> buffer(kBufferLength);
  for (int i = 0; i < kBufferLength; i++) {
    buffer[i] = i % 256;
  }


  RawData::Ref ref = RawData::Create(g_service_locator,
                                     uri,
                                     buffer,
                                     buffer.GetLength() );
  RawData *raw_data = ref;

  // Check that it got created.
  ASSERT_TRUE(raw_data != NULL);

  // Now, let's make sure it got the data length correct
  ASSERT_EQ(raw_data->GetLength(), buffer.GetLength());

  // Check the contents are correct
  ASSERT_EQ(0, memcmp(raw_data->GetData(), buffer, buffer.GetLength()));

  // Check that the uri is correct
  ASSERT_EQ(raw_data->uri(), uri);
}

namespace {

struct TestData {
  const uint8* data;
  size_t length;
  bool valid;
  size_t offset;
};

} // anonymous namespace

TEST_F(RawDataTest, StringValue) {
  // A BOM in the front (valid)
  static const uint8 data_0[] = {
    0xEF,
    0xBB,
    0xBF,
    0x65,
    0x66,
    0x65,
    0x67,
  };

  // A null in the string (invalid)
  static const uint8 data_1[] = {
    0x65,
    0x66,
    0x00,
    0x67,
  };

  // A valid string
  static const uint8 data_2[] = {
    0x65,
    0x66,
    0x65,
    0x67,
  };

  // Null at the end (invalid)
  static const uint8 data_3[] = {
    0x65,
    0x66,
    0x65,
    0x67,
    0x00,
  };

  // A badly formed utf-8 like string (invalid)
  static const uint8 data_4[] = {
    0xE9,
    0xBE,
    0xE0,
  };

  // A badly formed utf-8 like string (invalid)
  static const uint8 data_5[] = {
    0xC1,
    0x65,
    0x66,
  };

  // A badly formed utf-8 like string (invalid)
  static const uint8 data_6[] = {
    0x65,
    0x66,
    0xF4,
  };

  // A valid multi-byte utf-8 string (valid)
  static const uint8 data_7[] = {
    0xE9,
    0xBE,
    0x8D,
    0xF0,
    0x90,
    0x90,
    0x92,
  };

  // A UTF-8 but in D800-DFFF range
  static const uint8 data_8[] = {
    0xED,
    0xA0,
    0xA1,
  };

  TestData test_datas[] = {
    { &data_0[0], arraysize(data_0), true, 3, },
    { &data_1[0], arraysize(data_1), false, },
    { &data_2[0], arraysize(data_2), true, },
    { &data_3[0], arraysize(data_3), false, },
    { &data_4[0], arraysize(data_4), false, },
    { &data_5[0], arraysize(data_5), false, },
    { &data_6[0], arraysize(data_6), false, },
    { &data_7[0], arraysize(data_7), true, },
    { &data_8[0], arraysize(data_8), false, },
  };

  for (unsigned ii = 0; ii < arraysize(test_datas); ++ii) {
    TestData& test_data = test_datas[ii];
    RawData::Ref raw_data = RawData::Create(g_service_locator,
                                            "test",
                                            test_data.data,
                                            test_data.length);
    String str(raw_data->StringValue());
    if (test_data.valid) {
      size_t test_length = test_data.length - test_data.offset;
      const char* test_string = reinterpret_cast<const char*>(test_data.data +
                                                              test_data.offset);
      EXPECT_EQ(str.size(), test_length);
      EXPECT_EQ(str.compare(0, test_length, test_string, test_length), 0);
    } else {
      EXPECT_TRUE(str.empty());
    }
  }
}

}  // namespace o3d
