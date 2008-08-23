// Copyright 2008 Google Inc.
// Author: Lincoln Smith
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <config.h>
#include "varint_bigendian.h"
#include <cstdlib>  // rand, srand
#include <string>
#include <vector>
#include "logging.h"
#include "testing.h"

namespace open_vcdiff {
namespace {

using std::string;

class VarintBETestCommon : public testing::Test {
 protected:
  VarintBETestCommon()
      : varint_buf_(VarintBE<int64_t>::kMaxBytes),
        verify_encoded_byte_index_(0),
        verify_expected_length_(0),
        parse_data_ptr_(parse_data_all_FFs) {
  }

  virtual ~VarintBETestCommon() { }

  void ExpectEncodedByte(char expected_byte) {
    EXPECT_EQ(expected_byte, varint_buf_[verify_encoded_byte_index_]);
    EXPECT_EQ(expected_byte, s_[verify_encoded_byte_index_]);
    ++verify_encoded_byte_index_;
  }

  static const char parse_data_all_FFs[];
  static const char parse_data_CADA1[];

  std::vector<char> varint_buf_;
  string s_;
  int verify_encoded_byte_index_;
  int verify_expected_length_;
  const char* parse_data_ptr_;
};

template <typename SignedIntegerType>
class VarintBETestTemplate : public VarintBETestCommon {
 protected:
  VarintBETestTemplate() { }

  virtual ~VarintBETestTemplate() { }

  typedef SignedIntegerType SignedIntType;
  typedef VarintBE<SignedIntegerType> VarintType;

  void StartEncodingTest(SignedIntegerType v, int expected_length) {
    verify_expected_length_ = expected_length;
    EXPECT_EQ(expected_length, VarintType::Length(v));
    EXPECT_EQ(expected_length, VarintType::Encode(v, &varint_buf_[0]));
    VarintType::AppendToString(v, &s_);
    EXPECT_EQ(static_cast<size_t>(expected_length), s_.length());
  }

  void TestEncodeInvalid(SignedIntegerType v) {
    EXPECT_DEATH(VarintType::Length(v), "v >= 0");
    EXPECT_DEATH(VarintType::Encode(v, &varint_buf_[0]), "v >= 0");
    EXPECT_DEATH(VarintType::AppendToString(v, &s_), ">= 0");
  }

  // Need one function for each test type that will be applied to
  // multiple classes
  void TemplateTestDISABLED_EncodeNegative();
  void TemplateTestEncodeZero();
  void TemplateTestEncodeEightBits();
  void TemplateTestEncodeCADAD1A();
  void TemplateTestEncode32BitMaxInt();
  void TemplateTestEncodeDoesNotOverwriteExistingString();
  void TemplateTestParseNullPointer();
  void TemplateTestEndPointerPrecedesBeginning();
  void TemplateTestParseVarintTooLong();
  void TemplateTestParseIncompleteVarint();
  void TemplateTestParseZero();
  void TemplateTestParseCADA1();
  void TemplateTestParseEmpty();
  void TemplateTestParse123456789();
  void TemplateTestDecode31Bits();
  void TemplateTestEncodeDecodeRandom();
};

typedef VarintBETestTemplate<int32_t> VarintBEInt32Test;
typedef VarintBETestTemplate<int64_t> VarintBEInt64Test;

#ifdef GTEST_HAS_DEATH_TEST
// These synonyms are needed for the tests that use ASSERT_DEATH
typedef VarintBEInt32Test VarintBEInt32DeathTest;
typedef VarintBEInt64Test VarintBEInt64DeathTest;
#endif  // GTEST_HAS_DEATH_TEST

const char VarintBETestCommon::parse_data_all_FFs[] =
    { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

const char VarintBETestCommon::parse_data_CADA1[] =
    { 0xCA, 0xDA, 0x01 };

// A macro to allow defining tests once and having them run against
// both VarintBE<int32_t> and VarintBE<int64_t>.
//
#define TEMPLATE_TEST_F(TEST_TYPE, TEST_NAME) \
    TEST_F(VarintBEInt32##TEST_TYPE, TEST_NAME) { \
      TemplateTest##TEST_NAME(); \
    } \
    TEST_F(VarintBEInt64##TEST_TYPE, TEST_NAME) { \
      TemplateTest##TEST_NAME(); \
    } \
    template <class CacheType> \
    void VarintBETestTemplate<CacheType>::TemplateTest##TEST_NAME()

// Encoding tests: Length(), Encode(), AppendToString(), AppendToBuffer()

#ifdef GTEST_HAS_DEATH_TEST
// This test hangs for non-debug build (DeathTest threading problem)
TEMPLATE_TEST_F(DeathTest, DISABLED_EncodeNegative) {
  TestEncodeInvalid(-1);
}
#endif  // GTEST_HAS_DEATH_TEST

TEMPLATE_TEST_F(Test, EncodeZero) {
  StartEncodingTest(/* value */ 0x00, /* expected length */ 1);
  ExpectEncodedByte(0x00);
  EXPECT_EQ(verify_expected_length_, verify_encoded_byte_index_);
}

TEMPLATE_TEST_F(Test, EncodeEightBits) {
  StartEncodingTest(/* value */ 0xFF, /* expected length */ 2);
  ExpectEncodedByte(0x81);
  ExpectEncodedByte(0x7F);
  EXPECT_EQ(verify_expected_length_, verify_encoded_byte_index_);
}

TEMPLATE_TEST_F(Test, EncodeCADAD1A) {
  StartEncodingTest(/* value */ 0x0CADAD1A, /* expected length */ 4);
  ExpectEncodedByte(0xE5);
  ExpectEncodedByte(0xB6);
  ExpectEncodedByte(0xDA);
  ExpectEncodedByte(0x1A);
  EXPECT_EQ(verify_expected_length_, verify_encoded_byte_index_);
}

TEMPLATE_TEST_F(Test, Encode32BitMaxInt) {
  StartEncodingTest(/* value */ 0x7FFFFFFF, /* expected length */ 5);
  ExpectEncodedByte(0x87);
  ExpectEncodedByte(0xFF);
  ExpectEncodedByte(0xFF);
  ExpectEncodedByte(0xFF);
  ExpectEncodedByte(0x7F);
  EXPECT_EQ(verify_expected_length_, verify_encoded_byte_index_);
}

#ifdef GTEST_HAS_DEATH_TEST
// This test hangs for non-debug build (DeathTest threading problem)
TEST_F(VarintBEInt32DeathTest, DISABLED_Encode32BitsTooBig) {
  TestEncodeInvalid(0x80000000);
}
#endif  // GTEST_HAS_DEATH_TEST

TEST_F(VarintBEInt64Test, Encode32Bits) {
  StartEncodingTest(/* value */ 0x80000000, /* expected length */ 5);
  ExpectEncodedByte(0x88);
  ExpectEncodedByte(0x80);
  ExpectEncodedByte(0x80);
  ExpectEncodedByte(0x80);
  ExpectEncodedByte(0x00);
  EXPECT_EQ(verify_expected_length_, verify_encoded_byte_index_);
}

TEST_F(VarintBEInt64Test, Encode63Bits) {
  StartEncodingTest(/* value */ 0x7FFFFFFFFFFFFFFFULL, /* expected length */ 9);
  ExpectEncodedByte(0xFF);
  ExpectEncodedByte(0xFF);
  ExpectEncodedByte(0xFF);
  ExpectEncodedByte(0xFF);
  ExpectEncodedByte(0xFF);
  ExpectEncodedByte(0xFF);
  ExpectEncodedByte(0xFF);
  ExpectEncodedByte(0xFF);
  ExpectEncodedByte(0x7F);
  EXPECT_EQ(verify_expected_length_, verify_encoded_byte_index_);
}

#ifdef GTEST_HAS_DEATH_TEST
// This test hangs for non-debug build (DeathTest threading problem)
TEST_F(VarintBEInt64DeathTest, DISABLED_Encode64BitsTooBig) {
  TestEncodeInvalid(0x8000000000000000ULL);
}
#endif  // GTEST_HAS_DEATH_TEST

TEMPLATE_TEST_F(Test, EncodeDoesNotOverwriteExistingString) {
  s_.append("Test");
  VarintType::AppendToString('1', &s_);
  EXPECT_EQ(strlen("Test1"), s_.length());
  EXPECT_EQ("Test1", s_);
}

// Decoding tests: Parse(), ParseFromBuffer()

TEMPLATE_TEST_F(Test, ParseVarintTooLong) {
  EXPECT_EQ(RESULT_ERROR,
            VarintType::Parse(parse_data_ptr_ + VarintType::kMaxBytes,
                              &parse_data_ptr_));
}

TEMPLATE_TEST_F(Test, ParseIncompleteVarint) {
  EXPECT_EQ(RESULT_END_OF_DATA,
            VarintType::Parse(parse_data_ptr_ + VarintType::kMaxBytes - 1,
                              &parse_data_ptr_));
}

TEMPLATE_TEST_F(Test, ParseZero) {
  const char zero_data[] = { 0x00 };
  parse_data_ptr_ = zero_data;
  EXPECT_EQ(0x00, VarintType::Parse(parse_data_ptr_ + 1, &parse_data_ptr_));
  EXPECT_EQ(zero_data + 1, parse_data_ptr_);
}

TEMPLATE_TEST_F(Test, ParseCADA1) {
  parse_data_ptr_ = parse_data_CADA1;
  EXPECT_EQ(0x12AD01,
            VarintType::Parse(parse_data_CADA1 + sizeof(parse_data_CADA1),
                              &parse_data_ptr_));
  EXPECT_EQ(parse_data_CADA1 + 3, parse_data_ptr_);
}

#ifdef GTEST_HAS_DEATH_TEST
TEMPLATE_TEST_F(DeathTest, ParseNullPointer) {
  // limit == NULL is not an error
  parse_data_ptr_ = parse_data_CADA1;
  EXPECT_EQ(0x12AD01, VarintType::Parse((const char*) NULL, &parse_data_ptr_));
}
#endif  // GTEST_HAS_DEATH_TEST

TEMPLATE_TEST_F(Test, EndPointerPrecedesBeginning) {
  // This is not an error.
  parse_data_ptr_ = parse_data_CADA1;
  EXPECT_EQ(0x12AD01, VarintType::Parse(parse_data_ptr_ - 1, &parse_data_ptr_));
}

TEMPLATE_TEST_F(Test, ParseEmpty) {
  EXPECT_EQ(RESULT_END_OF_DATA,
            VarintType::Parse(parse_data_ptr_, &parse_data_ptr_));
}

// This example is taken from the Varint description in RFC 3284, section 2.
TEMPLATE_TEST_F(Test, Parse123456789) {
  const char parse_data_123456789[] = { 0x80 + 58, 0x80 + 111, 0x80 + 26, 21 };
  parse_data_ptr_ = parse_data_123456789;
  EXPECT_EQ(123456789, VarintType::Parse(parse_data_123456789
                                             + sizeof(parse_data_123456789),
                                         &parse_data_ptr_));
}

TEMPLATE_TEST_F(Test, Decode31Bits) {
  const char parse_data_31_bits[] = { 0x87, 0xFF, 0xFF, 0xFF, 0x7F };
  parse_data_ptr_ = parse_data_31_bits;
  EXPECT_EQ(0x7FFFFFFF,
            VarintType::Parse(parse_data_31_bits + sizeof(parse_data_31_bits),
                              &parse_data_ptr_));
}

TEST_F(VarintBEInt32Test, Decode32Bits) {
  const char parse_data_32_bits[] = { 0x88, 0x80, 0x80, 0x80, 0x00 };
  parse_data_ptr_ = parse_data_32_bits;
  EXPECT_EQ(RESULT_ERROR,
            VarintType::Parse(parse_data_32_bits + sizeof(parse_data_32_bits),
                              &parse_data_ptr_));
}

TEST_F(VarintBEInt64Test, Decode32Bits) {
  const char parse_data_32_bits[] = { 0x88, 0x80, 0x80, 0x80, 0x00 };
  parse_data_ptr_ = parse_data_32_bits;
  EXPECT_EQ(0x80000000,
            VarintType::Parse(parse_data_32_bits + sizeof(parse_data_32_bits),
                              &parse_data_ptr_));
}

TEMPLATE_TEST_F(Test, EncodeDecodeRandom) {
  const int test_size = 1024;  // 1K random encode/decode operations
  char encode_buffer[VarintType::kMaxBytes];
  srand(1);
  for (int i = 0; i < test_size; ++i) {
    SignedIntType value = PortableRandomInRange(VarintType::kMaxVal);
    int length = VarintType::Encode(value, encode_buffer);
    EXPECT_EQ(length, VarintType::Length(value));
    const char* parse_pointer = encode_buffer;
    EXPECT_EQ(value, VarintType::Parse(encode_buffer + sizeof(encode_buffer),
                                       &parse_pointer));
    EXPECT_EQ(encode_buffer + length, parse_pointer);
  }
  for (int i = 0; i < test_size; ++i) {
    s_.clear();
    SignedIntType value = PortableRandomInRange(VarintType::kMaxVal);
    VarintType::AppendToString(value, &s_);
    const int varint_length = static_cast<int>(s_.length());
    EXPECT_EQ(VarintType::Length(value), varint_length);
    const char* parse_pointer = s_.c_str();
    const char* const buffer_end_pointer = s_.c_str() + s_.length();
    EXPECT_EQ(value, VarintType::Parse(buffer_end_pointer, &parse_pointer));
    EXPECT_EQ(buffer_end_pointer, parse_pointer);
  }
}

}  // anonymous namespace
}  // namespace open_vcdiff
