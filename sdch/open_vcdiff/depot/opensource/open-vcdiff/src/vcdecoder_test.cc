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
#include "google/vcdecoder.h"
#include <cstdlib>  // free, posix_memalign
#include <cstring>  // memcpy
#include <string>
#include "checksum.h"
#include "codetable.h"
#include "testing.h"
#include "varint_bigendian.h"
#include "vcdiff_defs.h"

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif  // HAVE_MALLOC_H

#ifdef HAVE_SYS_MMAN_H
#define _XOPEN_SOURCE 600  // posix_memalign
#include <sys/mman.h>  // mprotect
#endif  // HAVE_SYS_MMAN_H

#ifdef HAVE_UNISTD_H
#include <unistd.h>  // getpagesize
#endif  // HAVE_UNISTD_H

namespace open_vcdiff {
namespace {

using std::string;

// A base class used for all the decoder tests.  Most tests use the same
// dictionary and target and construct the delta file in the same way.
// Those elements are provided as string members and can be modified or
// overwritten by each specific decoder test as needed.
class VCDiffDecoderTest : public testing::Test {
 protected:
  static const char kStandardFileHeader[];
  static const char kInterleavedFileHeader[];
  static const char kDictionary[];
  static const char kExpectedTarget[];
  static const char kExpectedAnnotatedTarget[];

  VCDiffDecoderTest();

  virtual ~VCDiffDecoderTest() {}

  virtual void SetUp();

  // This function is called by SetUp().  It populates delta_file_ with the
  // concatenated delta file header, delta window header, and delta window
  // body, plus (if UseChecksum() is true) the corresponding checksum.
  // It can be called again by a test that has modified the contents of
  // delta_file_ and needs to restore them to their original state.
  virtual void InitializeDeltaFile();

  // This function adds an Adler32 checksum to the delta window header.
  void AddChecksum(VCDChecksum checksum);

  // This function computes the Adler32 checksum for the expected target
  // and adds it to the delta window header.
  void ComputeAndAddChecksum();

  // Write the maximum expressible positive 32-bit VarintBE
  // (0x7FFFFFFF) at the given offset in the delta window.
  void WriteMaxVarintAtOffset(int offset, int bytes_to_replace);

  // Write a negative 32-bit VarintBE (0x80000000) at the given offset
  // in the delta window.
  void WriteNegativeVarintAtOffset(int offset, int bytes_to_replace);

  // Write a VarintBE that has too many continuation bytes
  // at the given offset in the delta window.
  void WriteInvalidVarintAtOffset(int offset, int bytes_to_replace);

  // This function iterates through a list of fuzzers (bit masks used to corrupt
  // bytes) and through positions in the delta file.  Each time it is called, it
  // attempts to corrupt a different byte in delta_file_ in a different way.  If
  // successful, it returns true. Once it exhausts the list of fuzzers and of
  // byte positions in delta_file_, it returns false.
  bool FuzzOneByteInDeltaFile();

  // Assuming the length of the given string can be expressed as a VarintBE
  // of length N, this function returns the byte at position which_byte, where
  // 0 <= which_byte < N.
  static char GetByteFromStringLength(const char* s, int which_byte) {
    char varint_buf[VarintBE<int32_t>::kMaxBytes];
    VarintBE<int32_t>::Encode(static_cast<int32_t>(strlen(s)), varint_buf);
    return varint_buf[which_byte];
  }

  // Assuming the length of the given string can be expressed as a one-byte
  // VarintBE, this function returns that byte value.
  static char StringLengthAsByte(const char* s) {
    return GetByteFromStringLength(s, 0);
  }

  // Assuming the length of the given string can be expressed as a two-byte
  // VarintBE, this function returns the first byte of its representation.
  static char FirstByteOfStringLength(const char* s) {
    return GetByteFromStringLength(s, 0);
  }

  // Assuming the length of the given string can be expressed as a two-byte
  // VarintBE, this function returns the second byte of its representation.
  static char SecondByteOfStringLength(const char* s) {
    return GetByteFromStringLength(s, 1);
  }

  VCDiffStreamingDecoder decoder_;

  // delta_file_ will be populated by InitializeDeltaFile() using the components
  // delta_file_header_, delta_window_header_, and delta_window_body_.
  string delta_file_;

  // This string is not populated during setup, but is used to receive the
  // decoded target file in each test.
  string output_;

  // Test fixtures that inherit from VCDiffDecoderTest can set these strings in
  // their constructors to override their default values (which come from
  // kDictionary, kExpectedTarget, etc.)
  string dictionary_;
  string expected_target_;
  string expected_annotated_target_;

  // The components that will be used to construct delta_file_.
  string delta_file_header_;
  string delta_window_header_;
  string delta_window_body_;

 private:
  // These two counters are used by FuzzOneByteInDeltaFile() to iterate through
  // different ways to corrupt the delta file.
  size_t fuzzer_;
  size_t fuzzed_byte_position_;
};

const char VCDiffDecoderTest::kStandardFileHeader[] = {
    0xD6,  // 'V' | 0x80
    0xC3,  // 'C' | 0x80
    0xC4,  // 'D' | 0x80
    0x00,  // Draft standard version number
    0x00   // Hdr_Indicator: no custom code table, no compression
  };

const char VCDiffDecoderTest::kInterleavedFileHeader[] = {
    0xD6,  // 'V' | 0x80
    0xC3,  // 'C' | 0x80
    0xC4,  // 'D' | 0x80
    'S',   // SDCH version code
    0x00   // Hdr_Indicator: no custom code table, no compression
  };

const char VCDiffDecoderTest::kDictionary[] =
  "\"Just the place for a Snark!\" the Bellman cried,\n"
  "As he landed his crew with care;\n"
  "Supporting each man on the top of the tide\n"
  "By a finger entwined in his hair.\n";

const char VCDiffDecoderTest::kExpectedTarget[] =
  "\"Just the place for a Snark! I have said it twice:\n"
  "That alone should encourage the crew.\n"
  "Just the place for a Snark! I have said it thrice:\n"
  "What I tell you three times is true.\"\n";

const char VCDiffDecoderTest::kExpectedAnnotatedTarget[] =
  "<dmatch>\"Just the place for a Snark!</dmatch>"
  "<literal> I have said it twice:\n"
  "That alone should encourage the crew.\n</literal>"
  "<bmatch>Just the place for a Snark! I have said it t</bmatch>"
  "<literal>hr</literal>"
  "<bmatch>ice:\n</bmatch>"
  "<literal>What I te</literal>"
  "<literal>ll</literal>"
  "<literal> you three times is true.\"\n</literal>";

VCDiffDecoderTest::VCDiffDecoderTest() : fuzzer_(0), fuzzed_byte_position_(0) {
  dictionary_ = kDictionary;
  expected_target_ = kExpectedTarget;
  expected_annotated_target_ = kExpectedAnnotatedTarget;
}

void VCDiffDecoderTest::SetUp() {
  InitializeDeltaFile();
}

void VCDiffDecoderTest::InitializeDeltaFile() {
  delta_file_ = delta_file_header_ + delta_window_header_ + delta_window_body_;
}

void VCDiffDecoderTest::AddChecksum(VCDChecksum checksum) {
  int32_t checksum_as_int32 = static_cast<int32_t>(checksum);
  delta_window_header_[0] |= VCD_CHECKSUM;
  VarintBE<int32_t>::AppendToString(checksum_as_int32, &delta_window_header_);
  // Adjust delta window size to include checksum.
  // This method wouldn't work if adding to the length caused the VarintBE
  // value to spill over into another byte.  Luckily, this test data happens
  // not to cause such an overflow.
  delta_window_header_[4] += VarintBE<int32_t>::Length(checksum_as_int32);
}

void VCDiffDecoderTest::ComputeAndAddChecksum() {
  AddChecksum(ComputeAdler32(expected_target_.data(),
                             expected_target_.size()));
}

// Write the maximum expressible positive 32-bit VarintBE
// (0x7FFFFFFF) at the given offset in the delta window.
void VCDiffDecoderTest::WriteMaxVarintAtOffset(int offset,
                                               int bytes_to_replace) {
  static const char kMaxVarint[] = { 0x87, 0xFF, 0xFF, 0xFF, 0x7F };
  delta_file_.replace(delta_file_header_.size() + offset,
                      bytes_to_replace,
                      kMaxVarint,
                      sizeof(kMaxVarint));
}

// Write a negative 32-bit VarintBE (0x80000000) at the given offset
// in the delta window.
void VCDiffDecoderTest::WriteNegativeVarintAtOffset(int offset,
                                                    int bytes_to_replace) {
  static const char kNegativeVarint[] = { 0x88, 0x80, 0x80, 0x80, 0x00 };
  delta_file_.replace(delta_file_header_.size() + offset,
                      bytes_to_replace,
                      kNegativeVarint,
                      sizeof(kNegativeVarint));
}

// Write a VarintBE that has too many continuation bytes
// at the given offset in the delta window.
void VCDiffDecoderTest::WriteInvalidVarintAtOffset(int offset,
                                                   int bytes_to_replace) {
  static const char kInvalidVarint[] = { 0x87, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F };
  delta_file_.replace(delta_file_header_.size() + offset,
                      bytes_to_replace,
                      kInvalidVarint,
                      sizeof(kInvalidVarint));
}

bool VCDiffDecoderTest::FuzzOneByteInDeltaFile() {
  static const struct Fuzzer {
    char _and;
    char _or;
    char _xor;
  } fuzzers[] = {
    { 0xff, 0x80, 0x00 },
    { 0xff, 0xff, 0x00 },
    { 0xff, 0x00, 0x80 },
    { 0xff, 0x00, 0xff },
    { 0xff, 0x01, 0x00 },
    { 0x7f, 0x00, 0x00 },
  };

  for (; fuzzer_ < (sizeof(fuzzers) / sizeof(fuzzers[0])); ++fuzzer_) {
    for (; fuzzed_byte_position_ < delta_file_.size();
         ++fuzzed_byte_position_) {
      char fuzzed_byte = (((delta_file_[fuzzed_byte_position_]
                             & fuzzers[fuzzer_]._and)
                             | fuzzers[fuzzer_]._or)
                             ^ fuzzers[fuzzer_]._xor);
      if (fuzzed_byte != delta_file_[fuzzed_byte_position_]) {
        delta_file_[fuzzed_byte_position_] = fuzzed_byte;
        ++fuzzed_byte_position_;
        return true;
      }
    }
    fuzzed_byte_position_ = 0;
  }
  return false;
}

// The "standard" decoder test, which decodes a delta file that uses the
// standard VCDIFF (RFC 3284) format with no extensions.
class VCDiffStandardDecoderTest : public VCDiffDecoderTest {
 protected:
  VCDiffStandardDecoderTest();
  virtual ~VCDiffStandardDecoderTest() {}

 private:
  static const char kWindowHeader[];
  static const char kWindowBody[];
};

const char VCDiffStandardDecoderTest::kWindowHeader[] = {
    VCD_SOURCE,  // Win_Indicator: take source from dictionary
    FirstByteOfStringLength(kDictionary),  // Source segment size
    SecondByteOfStringLength(kDictionary),
    0x00,  // Source segment position: start of dictionary
    0x79,  // Length of the delta encoding
    FirstByteOfStringLength(kExpectedTarget),  // Size of the target window
    SecondByteOfStringLength(kExpectedTarget),
    0x00,  // Delta_indicator (no compression)
    0x64,  // length of data for ADDs and RUNs
    0x0C,  // length of instructions section
    0x03  // length of addresses for COPYs
  };

const char VCDiffStandardDecoderTest::kWindowBody[] = {
    // Data for ADDs: 1st section (length 61)
    ' ', 'I', ' ', 'h', 'a', 'v', 'e', ' ', 's', 'a', 'i', 'd', ' ',
    'i', 't', ' ', 't', 'w', 'i', 'c', 'e', ':', '\n',
    'T', 'h', 'a', 't', ' ',
    'a', 'l', 'o', 'n', 'e', ' ', 's', 'h', 'o', 'u', 'l', 'd', ' ',
    'e', 'n', 'c', 'o', 'u', 'r', 'a', 'g', 'e', ' ',
    't', 'h', 'e', ' ', 'c', 'r', 'e', 'w', '.', '\n',
    // Data for ADDs: 2nd section (length 2)
    'h', 'r',
    // Data for ADDs: 3rd section (length 9)
    'W', 'h', 'a', 't', ' ',
    'I', ' ', 't', 'e',
    // Data for RUN: 4th section (length 1)
    'l',
    // Data for ADD: 4th section (length 27)
    ' ', 'y', 'o', 'u', ' ',
    't', 'h', 'r', 'e', 'e', ' ', 't', 'i', 'm', 'e', 's', ' ', 'i', 's', ' ',
    't', 'r', 'u', 'e', '.', '\"', '\n',
    // Instructions and sizes (length 13)
    0x13,  // VCD_COPY mode VCD_SELF, size 0
    0x1C,  // Size of COPY (28)
    0x01,  // VCD_ADD size 0
    0x3D,  // Size of ADD (61)
    0x23,  // VCD_COPY mode VCD_HERE, size 0
    0x2C,  // Size of COPY (44)
    0xCB,  // VCD_ADD size 2 + VCD_COPY mode NEAR(1), size 5
    0x0A,  // VCD_ADD size 9
    0x00,  // VCD_RUN size 0
    0x02,  // Size of RUN (2)
    0x01,  // VCD_ADD size 0
    0x1B,  // Size of ADD (27)
    // Addresses for COPYs (length 3)
    0x00,  // Start of dictionary
    0x58,  // HERE mode address for 2nd copy (27+61 back from here_address)
    0x2D   // NEAR(1) mode address for 2nd copy (45 after prior address)
  };

VCDiffStandardDecoderTest::VCDiffStandardDecoderTest() {
  delta_file_header_.assign(kStandardFileHeader, sizeof(kStandardFileHeader));
  delta_window_header_.assign(kWindowHeader, sizeof(kWindowHeader));
  delta_window_body_.assign(kWindowBody, sizeof(kWindowBody));
}

TEST_F(VCDiffStandardDecoderTest, DecodeHeaderOnly) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_header_.data(),
                                   delta_file_header_.size(),
                                   &output_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, Decode) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_.data(),
                                   delta_file_.size(),
                                   &output_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

// If we add a checksum to a standard-format delta file (without using format
// extensions), it will be interpreted as random bytes inserted into the middle
// of the file.  The decode operation should fail, but where exactly it fails is
// not easy to predict.
TEST_F(VCDiffStandardDecoderTest, StandardFormatDoesNotSupportChecksum) {
  ComputeAndAddChecksum();
  InitializeDeltaFile();
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

// Remove one byte from the length of the chunk to process, and
// verify that an error is returned for FinishDecoding().
TEST_F(VCDiffStandardDecoderTest, FinishAfterDecodingPartialWindow) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_.data(),
                                   delta_file_.size() - 1,
                                   &output_));
  EXPECT_FALSE(decoder_.FinishDecoding());
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTest, FinishAfterDecodingPartialWindowHeader) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_.data(),
                                   delta_file_header_.size()
                                       + delta_window_header_.size() - 1,
                                   &output_));
  EXPECT_FALSE(decoder_.FinishDecoding());
  EXPECT_EQ("", output_);
}

// Fuzz bits to make sure decoder does not violently crash.
// This test has no expected behavior except that no crashes should occur.
// In some cases, changing bits will still decode to the correct target;
// for example, changing unused bits within a bitfield.
TEST_F(VCDiffStandardDecoderTest, FuzzBits) {
  while (FuzzOneByteInDeltaFile()) {
    decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
    if (decoder_.DecodeChunk(delta_file_.data(),
                             delta_file_.size(),
                             &output_)) {
      decoder_.FinishDecoding();
    }
    InitializeDeltaFile();
    output_.clear();
  }
}

TEST_F(VCDiffStandardDecoderTest, CheckAnnotatedOutput) {
  decoder_.EnableAnnotatedOutput();
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_.data(),
                                   delta_file_.size(),
                                   &output_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  string annotated_output;
  decoder_.GetAnnotatedOutput(&annotated_output);
  EXPECT_EQ(expected_annotated_target_, annotated_output);
}

// Change each element of the delta file window to an erroneous value
// and make sure it's caught as an error.

TEST_F(VCDiffStandardDecoderTest, WinIndicatorHasBothSourceAndTarget) {
  delta_file_[delta_file_header_.size()] = VCD_SOURCE + VCD_TARGET;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, OkayToSetUpperBitsOfWinIndicator) {
  // It is not an error to set any of the other bits in Win_Indicator
  // besides VCD_SOURCE and VCD_TARGET.
  delta_file_[delta_file_header_.size()] = 0xFD;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_.data(),
                                   delta_file_.size(),
                                   &output_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

TEST_F(VCDiffStandardDecoderTest, CopyInstructionsShouldFailIfNoSourceSegment) {
  // Replace the Win_Indicator and the source size and source offset with a
  // single 0 byte (a Win_Indicator for a window with no source segment.)
  delta_window_header_.replace(0, 4, "\0", 1);
  InitializeDeltaFile();
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  // The first COPY instruction should fail, so there should be no output
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, SourceSegmentSizeExceedsDictionarySize) {
  ++delta_file_[delta_file_header_.size() + 2];  // increment size
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, SourceSegmentSizeMaxInt) {
  WriteMaxVarintAtOffset(1, 2);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, SourceSegmentSizeNegative) {
  WriteNegativeVarintAtOffset(1, 2);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, SourceSegmentSizeInvalid) {
  WriteInvalidVarintAtOffset(1, 2);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, SourceSegmentEndExceedsDictionarySize) {
  ++delta_file_[delta_file_header_.size() + 3];  // increment start pos
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, SourceSegmentPosMaxInt) {
  WriteMaxVarintAtOffset(3, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, SourceSegmentPosNegative) {
  WriteNegativeVarintAtOffset(3, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, SourceSegmentPosInvalid) {
  WriteInvalidVarintAtOffset(3, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, DeltaEncodingLengthZero) {
  delta_file_[delta_file_header_.size() + 4] = 0;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, DeltaEncodingLengthTooLargeByOne) {
  ++delta_file_[delta_file_header_.size() + 4];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, DeltaEncodingLengthTooSmallByOne) {
  --delta_file_[delta_file_header_.size() + 4];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, DeltaEncodingLengthMaxInt) {
  WriteMaxVarintAtOffset(4, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, DeltaEncodingLengthNegative) {
  WriteNegativeVarintAtOffset(4, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, DeltaEncodingLengthInvalid) {
  WriteInvalidVarintAtOffset(4, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, TargetWindowSizeZero) {
  static const char zero_size[] = { 0x00 };
  delta_file_.replace(delta_file_header_.size() + 5, 2, zero_size, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, TargetWindowSizeTooLargeByOne) {
  ++delta_file_[delta_file_header_.size() + 6];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, TargetWindowSizeTooSmallByOne) {
  --delta_file_[delta_file_header_.size() + 6];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, TargetWindowSizeMaxInt) {
  WriteMaxVarintAtOffset(5, 2);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, TargetWindowSizeNegative) {
  WriteNegativeVarintAtOffset(5, 2);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, TargetWindowSizeInvalid) {
  WriteInvalidVarintAtOffset(5, 2);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, OkayToSetUpperBitsOfDeltaIndicator) {
  delta_file_[delta_file_header_.size() + 7] = 0xF8;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_.data(),
                                   delta_file_.size(),
                                   &output_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

TEST_F(VCDiffStandardDecoderTest, DataCompressionNotSupported) {
  delta_file_[delta_file_header_.size() + 7] = 0x01;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, InstructionCompressionNotSupported) {
  delta_file_[delta_file_header_.size() + 7] = 0x02;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, AddressCompressionNotSupported) {
  delta_file_[delta_file_header_.size() + 7] = 0x04;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, AddRunDataSizeZero) {
  delta_file_[delta_file_header_.size() + 8] = 0;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, AddRunDataSizeTooLargeByOne) {
  ++delta_file_[delta_file_header_.size() + 8];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, AddRunDataSizeTooSmallByOne) {
  --delta_file_[delta_file_header_.size() + 8];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, AddRunDataSizeMaxInt) {
  WriteMaxVarintAtOffset(8, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, AddRunDataSizeNegative) {
  WriteNegativeVarintAtOffset(8, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, AddRunDataSizeInvalid) {
  WriteInvalidVarintAtOffset(8, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, InstructionsSizeZero) {
  delta_file_[delta_file_header_.size() + 9] = 0;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, InstructionsSizeTooLargeByOne) {
  ++delta_file_[delta_file_header_.size() + 9];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, InstructionsSizeTooSmallByOne) {
  --delta_file_[delta_file_header_.size() + 9];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, InstructionsSizeMaxInt) {
  WriteMaxVarintAtOffset(9, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, InstructionsSizeNegative) {
  WriteNegativeVarintAtOffset(9, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, InstructionsSizeInvalid) {
  WriteInvalidVarintAtOffset(9, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, CopyAddressSizeZero) {
  delta_file_[delta_file_header_.size() + 10] = 0;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, CopyAddressSizeTooLargeByOne) {
  ++delta_file_[delta_file_header_.size() + 10];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, CopyAddressSizeTooSmallByOne) {
  --delta_file_[delta_file_header_.size() + 10];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, CopyAddressSizeMaxInt) {
  WriteMaxVarintAtOffset(10, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, CopyAddressSizeNegative) {
  WriteNegativeVarintAtOffset(10, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, CopyAddressSizeInvalid) {
  WriteInvalidVarintAtOffset(10, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, InstructionsEndEarly) {
  --delta_file_[delta_file_header_.size() + 9];
  ++delta_file_[delta_file_header_.size() + 10];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

// From this point on, the tests should also be run against the interleaved
// format.

TEST_F(VCDiffStandardDecoderTest, CopyMoreThanExpectedTarget) {
  delta_file_[delta_file_header_.size() + 0x70] =
      FirstByteOfStringLength(kExpectedTarget);
  delta_file_[delta_file_header_.size() + 0x71] =
      SecondByteOfStringLength(kExpectedTarget) + 1;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, CopySizeZero) {
  delta_file_[delta_file_header_.size() + 0x70] = 0;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, CopySizeTooLargeByOne) {
  ++delta_file_[delta_file_header_.size() + 0x70];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, CopySizeTooSmallByOne) {
  --delta_file_[delta_file_header_.size() + 0x70];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, CopySizeMaxInt) {
  WriteMaxVarintAtOffset(0x70, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, CopySizeNegative) {
  WriteNegativeVarintAtOffset(0x70, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, CopySizeInvalid) {
  WriteInvalidVarintAtOffset(0x70, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, CopyAddressBeyondHereAddress) {
  delta_file_[delta_file_header_.size() + 0x7B] =
      FirstByteOfStringLength(kDictionary);
  delta_file_[delta_file_header_.size() + 0x7C] =
      SecondByteOfStringLength(kDictionary);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, CopyAddressMaxInt) {
  WriteMaxVarintAtOffset(0x7B, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, CopyAddressNegative) {
  WriteNegativeVarintAtOffset(0x70, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, CopyAddressInvalid) {
  WriteInvalidVarintAtOffset(0x70, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, AddMoreThanExpectedTarget) {
  delta_file_[delta_file_header_.size() + 0x72] =
      FirstByteOfStringLength(kExpectedTarget);
  delta_file_[delta_file_header_.size() + 0x73] =
      SecondByteOfStringLength(kExpectedTarget) + 1;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, AddSizeZero) {
  delta_file_[delta_file_header_.size() + 0x72] = 0;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, AddSizeTooLargeByOne) {
  ++delta_file_[delta_file_header_.size() + 0x72];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, AddSizeTooSmallByOne) {
  --delta_file_[delta_file_header_.size() + 0x72];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, AddSizeMaxInt) {
  WriteMaxVarintAtOffset(0x72, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, AddSizeNegative) {
  WriteNegativeVarintAtOffset(0x72, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, AddSizeInvalid) {
  WriteInvalidVarintAtOffset(0x72, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, RunMoreThanExpectedTarget) {
  delta_file_[delta_file_header_.size() + 0x78] =
      FirstByteOfStringLength(kExpectedTarget);
  delta_file_[delta_file_header_.size() + 0x79] =
      SecondByteOfStringLength(kExpectedTarget) + 1;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, RunSizeZero) {
  delta_file_[delta_file_header_.size() + 0x78] = 0;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, RunSizeTooLargeByOne) {
  ++delta_file_[delta_file_header_.size() + 0x78];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, RunSizeTooSmallByOne) {
  --delta_file_[delta_file_header_.size() + 0x78];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, RunSizeMaxInt) {
  WriteMaxVarintAtOffset(0x78, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, RunSizeNegative) {
  WriteNegativeVarintAtOffset(0x78, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTest, RunSizeInvalid) {
  WriteInvalidVarintAtOffset(0x78, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

// These are the same tests as for VCDiffStandardDecoderTest, with the added
// complication that instead of calling DecodeChunk() once with the entire data
// set, DecodeChunk() is called once for each byte of input.  This is intended
// to shake out any bugs with rewind and resume while parsing chunked data.

typedef VCDiffStandardDecoderTest VCDiffStandardDecoderTestByteByByte;

TEST_F(VCDiffStandardDecoderTestByteByByte, DecodeHeaderOnly) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_header_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_header_[i], 1, &output_));
  }
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, Decode) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i], 1, &output_));
  }
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

// Remove one byte from the length of the chunk to process, and
// verify that an error is returned for FinishDecoding().
TEST_F(VCDiffStandardDecoderTestByteByByte, FinishAfterDecodingPartialWindow) {
  delta_file_.resize(delta_file_.size() - 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i], 1, &output_));
  }
  EXPECT_FALSE(decoder_.FinishDecoding());
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte,
       FinishAfterDecodingPartialWindowHeader) {
  delta_file_.resize(delta_file_header_.size()
                       + delta_window_header_.size() - 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i], 1, &output_));
  }
  EXPECT_FALSE(decoder_.FinishDecoding());
  EXPECT_EQ("", output_);
}

// If we add a checksum to a standard-format delta file (without using format
// extensions), it will be interpreted as random bytes inserted into the middle
// of the file.  The decode operation should fail, but where exactly it fails is
// undefined.
TEST_F(VCDiffStandardDecoderTestByteByByte,
       StandardFormatDoesNotSupportChecksum) {
  ComputeAndAddChecksum();
  InitializeDeltaFile();
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

// Fuzz bits to make sure decoder does not violently crash.
// This test has no expected behavior except that no crashes should occur.
// In some cases, changing bits will still decode to the correct target;
// for example, changing unused bits within a bitfield.
TEST_F(VCDiffStandardDecoderTestByteByByte, FuzzBits) {
  while (FuzzOneByteInDeltaFile()) {
    decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
    bool failed = false;
    for (size_t i = 0; i < delta_file_.size(); ++i) {
      if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
        failed = true;
        break;
      }
    }
    if (!failed) {
      decoder_.FinishDecoding();
    }
    // The decoder should not create more target bytes than were expected.
    EXPECT_GE(expected_target_.size(), output_.size());
    InitializeDeltaFile();
    output_.clear();
  }
}

TEST_F(VCDiffStandardDecoderTestByteByByte, CheckAnnotatedOutput) {
  decoder_.EnableAnnotatedOutput();
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i], 1, &output_));
  }
  EXPECT_TRUE(decoder_.FinishDecoding());
  string annotated_output;
  decoder_.GetAnnotatedOutput(&annotated_output);
  EXPECT_EQ(expected_annotated_target_, annotated_output);
}

// Change each element of the delta file window to an erroneous value
// and make sure it's caught as an error.

TEST_F(VCDiffStandardDecoderTestByteByByte,
       WinIndicatorHasBothSourceAndTarget) {
  delta_file_[delta_file_header_.size()] = VCD_SOURCE + VCD_TARGET;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail at the position that was altered
      EXPECT_EQ(delta_file_header_.size(), i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, OkayToSetUpperBitsOfWinIndicator) {
  // It is not an error to set any of the other bits in Win_Indicator
  // besides VCD_SOURCE and VCD_TARGET.
  delta_file_[delta_file_header_.size()] = 0xFD;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i], 1, &output_));
  }
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte,
       CopyInstructionsShouldFailIfNoSourceSegment) {
  // Replace the Win_Indicator and the source size and source offset with a
  // single 0 byte (a Win_Indicator for a window with no source segment.)
  delta_window_header_.replace(0, 4, "\0", 1);
  InitializeDeltaFile();
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // The first COPY instruction should fail.  With the standard format,
      // it may need to see the whole delta window before knowing that it is
      // invalid.
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte,
       SourceSegmentSizeExceedsDictionarySize) {
  ++delta_file_[delta_file_header_.size() + 2];  // increment size
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the source segment size
      EXPECT_EQ(delta_file_header_.size() + 2, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, SourceSegmentSizeMaxInt) {
  WriteMaxVarintAtOffset(1, 2);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the source segment size
      EXPECT_EQ(delta_file_header_.size() + 5, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, SourceSegmentSizeNegative) {
  WriteNegativeVarintAtOffset(1, 2);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the source segment size
      EXPECT_EQ(delta_file_header_.size() + 5, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, SourceSegmentSizeInvalid) {
  WriteInvalidVarintAtOffset(1, 2);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the source segment size
      EXPECT_GE(delta_file_header_.size() + 6, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte,
       SourceSegmentEndExceedsDictionarySize) {
  ++delta_file_[delta_file_header_.size() + 3];  // increment start pos
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the source segment end
      EXPECT_EQ(delta_file_header_.size() + 3, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, SourceSegmentPosMaxInt) {
  WriteMaxVarintAtOffset(3, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the source segment pos
      EXPECT_EQ(delta_file_header_.size() + 7, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, SourceSegmentPosNegative) {
  WriteNegativeVarintAtOffset(3, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the source segment pos
      EXPECT_EQ(delta_file_header_.size() + 7, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, SourceSegmentPosInvalid) {
  WriteInvalidVarintAtOffset(3, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the source segment pos
      EXPECT_GE(delta_file_header_.size() + 8, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, DeltaEncodingLengthZero) {
  delta_file_[delta_file_header_.size() + 4] = 0;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the copy address segment size
      EXPECT_EQ(delta_file_header_.size() + 10, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, DeltaEncodingLengthTooLargeByOne) {
  ++delta_file_[delta_file_header_.size() + 4];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the copy address segment size
      EXPECT_EQ(delta_file_header_.size() + 10, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, DeltaEncodingLengthTooSmallByOne) {
  --delta_file_[delta_file_header_.size() + 4];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the copy address segment size
      EXPECT_EQ(delta_file_header_.size() + 10, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, DeltaEncodingLengthMaxInt) {
  WriteMaxVarintAtOffset(4, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail before finishing the window header
      EXPECT_GE(delta_file_header_.size() + delta_window_header_.size() + 4,
                i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, DeltaEncodingLengthNegative) {
  WriteNegativeVarintAtOffset(4, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the delta encoding length
      EXPECT_EQ(delta_file_header_.size() + 8, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, DeltaEncodingLengthInvalid) {
  WriteInvalidVarintAtOffset(4, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the delta encoding length
      EXPECT_GE(delta_file_header_.size() + 9, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, TargetWindowSizeZero) {
  static const char zero_size[] = { 0x00 };
  delta_file_.replace(delta_file_header_.size() + 5, 2, zero_size, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, TargetWindowSizeTooLargeByOne) {
  ++delta_file_[delta_file_header_.size() + 6];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, TargetWindowSizeTooSmallByOne) {
  --delta_file_[delta_file_header_.size() + 6];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, TargetWindowSizeMaxInt) {
  WriteMaxVarintAtOffset(5, 2);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the target window size
      EXPECT_EQ(delta_file_header_.size() + 9, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, TargetWindowSizeNegative) {
  WriteNegativeVarintAtOffset(5, 2);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the target window size
      EXPECT_EQ(delta_file_header_.size() + 9, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, TargetWindowSizeInvalid) {
  WriteInvalidVarintAtOffset(5, 2);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the target window size
      EXPECT_GE(delta_file_header_.size() + 10, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte,
       OkayToSetUpperBitsOfDeltaIndicator) {
  delta_file_[delta_file_header_.size() + 7] = 0xF8;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i], 1, &output_));
  }
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, DataCompressionNotSupported) {
  delta_file_[delta_file_header_.size() + 7] = 0x01;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the delta indicator
      EXPECT_EQ(delta_file_header_.size() + 7, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte,
       InstructionCompressionNotSupported) {
  delta_file_[delta_file_header_.size() + 7] = 0x02;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the delta indicator
      EXPECT_EQ(delta_file_header_.size() + 7, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, AddressCompressionNotSupported) {
  delta_file_[delta_file_header_.size() + 7] = 0x04;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the delta indicator
      EXPECT_EQ(delta_file_header_.size() + 7, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, AddRunDataSizeZero) {
  delta_file_[delta_file_header_.size() + 8] = 0;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the copy address segment size
      EXPECT_EQ(delta_file_header_.size() + 10, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, AddRunDataSizeTooLargeByOne) {
  ++delta_file_[delta_file_header_.size() + 8];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the copy address segment size
      EXPECT_EQ(delta_file_header_.size() + 10, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, AddRunDataSizeTooSmallByOne) {
  --delta_file_[delta_file_header_.size() + 8];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the copy address segment size
      EXPECT_EQ(delta_file_header_.size() + 10, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, AddRunDataSizeMaxInt) {
  WriteMaxVarintAtOffset(8, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail before finishing the window header
      EXPECT_GE(delta_file_header_.size() + delta_window_header_.size() + 4,
                i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, AddRunDataSizeNegative) {
  WriteNegativeVarintAtOffset(8, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the add/run data segment size
      EXPECT_EQ(delta_file_header_.size() + 12, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, AddRunDataSizeInvalid) {
  WriteInvalidVarintAtOffset(8, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the add/run data segment size
      EXPECT_GE(delta_file_header_.size() + 13, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, InstructionsSizeZero) {
  delta_file_[delta_file_header_.size() + 9] = 0;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the copy address segment size
      EXPECT_EQ(delta_file_header_.size() + 10, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, InstructionsSizeTooLargeByOne) {
  ++delta_file_[delta_file_header_.size() + 9];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the copy address segment size
      EXPECT_EQ(delta_file_header_.size() + 10, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, InstructionsSizeTooSmallByOne) {
  --delta_file_[delta_file_header_.size() + 9];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the copy address segment size
      EXPECT_EQ(delta_file_header_.size() + 10, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, InstructionsSizeMaxInt) {
  WriteMaxVarintAtOffset(9, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail before finishing the window header
      EXPECT_GE(delta_file_header_.size() + delta_window_header_.size() + 4,
                i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, InstructionsSizeNegative) {
  WriteNegativeVarintAtOffset(9, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the instructions segment size
      EXPECT_EQ(delta_file_header_.size() + 13, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, InstructionsSizeInvalid) {
  WriteInvalidVarintAtOffset(9, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the instructions segment size
      EXPECT_GE(delta_file_header_.size() + 14, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, CopyAddressSizeZero) {
  delta_file_[delta_file_header_.size() + 10] = 0;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the copy address segment size
      EXPECT_EQ(delta_file_header_.size() + 10, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, CopyAddressSizeTooLargeByOne) {
  ++delta_file_[delta_file_header_.size() + 10];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the copy address segment size
      EXPECT_EQ(delta_file_header_.size() + 10, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, CopyAddressSizeTooSmallByOne) {
  --delta_file_[delta_file_header_.size() + 10];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the copy address segment size
      EXPECT_EQ(delta_file_header_.size() + 10, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, CopyAddressSizeMaxInt) {
  WriteMaxVarintAtOffset(10, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the copy address segment size
      EXPECT_EQ(delta_file_header_.size() + 14, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, CopyAddressSizeNegative) {
  WriteNegativeVarintAtOffset(10, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the copy address segment size
      EXPECT_EQ(delta_file_header_.size() + 14, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, CopyAddressSizeInvalid) {
  WriteInvalidVarintAtOffset(10, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the copy address segment size
      EXPECT_GE(delta_file_header_.size() + 15, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffStandardDecoderTestByteByByte, InstructionsEndEarly) {
  --delta_file_[delta_file_header_.size() + 9];
  ++delta_file_[delta_file_header_.size() + 10];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

// From this point on, the tests should also be run against the interleaved
// format.

TEST_F(VCDiffStandardDecoderTestByteByByte, CopyMoreThanExpectedTarget) {
  delta_file_[delta_file_header_.size() + 0x70] =
      FirstByteOfStringLength(kExpectedTarget);
  delta_file_[delta_file_header_.size() + 0x71] =
      SecondByteOfStringLength(kExpectedTarget) + 1;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, CopySizeZero) {
  delta_file_[delta_file_header_.size() + 0x70] = 0;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, CopySizeTooLargeByOne) {
  ++delta_file_[delta_file_header_.size() + 0x70];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, CopySizeTooSmallByOne) {
  --delta_file_[delta_file_header_.size() + 0x70];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, CopySizeMaxInt) {
  WriteMaxVarintAtOffset(0x70, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, CopySizeNegative) {
  WriteNegativeVarintAtOffset(0x70, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, CopySizeInvalid) {
  WriteInvalidVarintAtOffset(0x70, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, CopyAddressBeyondHereAddress) {
  delta_file_[delta_file_header_.size() + 0x7B] =
      FirstByteOfStringLength(kDictionary);
  delta_file_[delta_file_header_.size() + 0x7C] =
      SecondByteOfStringLength(kDictionary);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, CopyAddressMaxInt) {
  WriteMaxVarintAtOffset(0x7B, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, CopyAddressNegative) {
  WriteNegativeVarintAtOffset(0x70, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, CopyAddressInvalid) {
  WriteInvalidVarintAtOffset(0x70, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, AddMoreThanExpectedTarget) {
  delta_file_[delta_file_header_.size() + 0x72] =
      FirstByteOfStringLength(kExpectedTarget);
  delta_file_[delta_file_header_.size() + 0x73] =
      SecondByteOfStringLength(kExpectedTarget) + 1;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, AddSizeZero) {
  delta_file_[delta_file_header_.size() + 0x72] = 0;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, AddSizeTooLargeByOne) {
  ++delta_file_[delta_file_header_.size() + 0x72];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, AddSizeTooSmallByOne) {
  --delta_file_[delta_file_header_.size() + 0x72];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, AddSizeMaxInt) {
  WriteMaxVarintAtOffset(0x72, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, AddSizeNegative) {
  WriteNegativeVarintAtOffset(0x72, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, AddSizeInvalid) {
  WriteInvalidVarintAtOffset(0x72, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, RunMoreThanExpectedTarget) {
  delta_file_[delta_file_header_.size() + 0x78] =
      FirstByteOfStringLength(kExpectedTarget);
  delta_file_[delta_file_header_.size() + 0x79] =
      SecondByteOfStringLength(kExpectedTarget) + 1;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, RunSizeZero) {
  delta_file_[delta_file_header_.size() + 0x78] = 0;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, RunSizeTooLargeByOne) {
  ++delta_file_[delta_file_header_.size() + 0x78];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, RunSizeTooSmallByOne) {
  --delta_file_[delta_file_header_.size() + 0x78];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, RunSizeMaxInt) {
  WriteMaxVarintAtOffset(0x78, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, RunSizeNegative) {
  WriteNegativeVarintAtOffset(0x78, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffStandardDecoderTestByteByByte, RunSizeInvalid) {
  WriteInvalidVarintAtOffset(0x78, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

// The "interleaved" decoder test, which decodes a delta file that uses the
// SDCH extension of interleaving instructions, addresses, and literal data
// instead of placing them in three separate sections.
class VCDiffInterleavedDecoderTest : public VCDiffDecoderTest {
 protected:
  VCDiffInterleavedDecoderTest();
  virtual ~VCDiffInterleavedDecoderTest() {}

 private:
  static const char kWindowHeader[];
  static const char kWindowBody[];
};

const char VCDiffInterleavedDecoderTest::kWindowHeader[] = {
    VCD_SOURCE,  // Win_Indicator: take source from dictionary
    FirstByteOfStringLength(kDictionary),  // Source segment size
    SecondByteOfStringLength(kDictionary),
    0x00,  // Source segment position: start of dictionary
    0x79,  // Length of the delta encoding
    FirstByteOfStringLength(kExpectedTarget),  // Size of the target window
    SecondByteOfStringLength(kExpectedTarget),
    0x00,  // Delta_indicator (no compression)
    0x00,  // length of data for ADDs and RUNs (unused)
    0x73,  // length of interleaved section
    0x00  // length of addresses for COPYs (unused)
  };

const char VCDiffInterleavedDecoderTest::kWindowBody[] = {
    0x13,  // VCD_COPY mode VCD_SELF, size 0
    0x1C,  // Size of COPY (28)
    0x00,  // Address of COPY: Start of dictionary
    0x01,  // VCD_ADD size 0
    0x3D,  // Size of ADD (61)
    // Data for ADD (length 61)
    ' ', 'I', ' ', 'h', 'a', 'v', 'e', ' ', 's', 'a', 'i', 'd', ' ',
    'i', 't', ' ', 't', 'w', 'i', 'c', 'e', ':', '\n',
    'T', 'h', 'a', 't', ' ',
    'a', 'l', 'o', 'n', 'e', ' ', 's', 'h', 'o', 'u', 'l', 'd', ' ',
    'e', 'n', 'c', 'o', 'u', 'r', 'a', 'g', 'e', ' ',
    't', 'h', 'e', ' ', 'c', 'r', 'e', 'w', '.', '\n',
    0x23,  // VCD_COPY mode VCD_HERE, size 0
    0x2C,  // Size of COPY (44)
    0x58,  // HERE mode address (27+61 back from here_address)
    0xCB,  // VCD_ADD size 2 + VCD_COPY mode NEAR(1), size 5
    // Data for ADDs: 2nd section (length 2)
    'h', 'r',
    0x2D,  // NEAR(1) mode address (45 after prior address)
    0x0A,  // VCD_ADD size 9
    // Data for ADDs: 3rd section (length 9)
    'W', 'h', 'a', 't', ' ',
    'I', ' ', 't', 'e',
    0x00,  // VCD_RUN size 0
    0x02,  // Size of RUN (2)
    // Data for RUN: 4th section (length 1)
    'l',
    0x01,  // VCD_ADD size 0
    0x1B,  // Size of ADD (27)
    // Data for ADD: 4th section (length 27)
    ' ', 'y', 'o', 'u', ' ',
    't', 'h', 'r', 'e', 'e', ' ', 't', 'i', 'm', 'e', 's', ' ', 'i', 's', ' ',
    't', 'r', 'u', 'e', '.', '\"', '\n'
  };

VCDiffInterleavedDecoderTest::VCDiffInterleavedDecoderTest() {
  delta_file_header_.assign(kInterleavedFileHeader,
                            sizeof(kInterleavedFileHeader));
  delta_window_header_.assign(kWindowHeader, sizeof(kWindowHeader));
  delta_window_body_.assign(kWindowBody, sizeof(kWindowBody));
}

// Test headers, valid and invalid.

TEST_F(VCDiffInterleavedDecoderTest, DecodeHeaderOnly) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_header_.data(),
                                   delta_file_header_.size(),
                                   &output_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, PartialHeaderNotEnough) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_header_.data(),
                                   delta_file_header_.size() - 2,
                                   &output_));
  EXPECT_FALSE(decoder_.FinishDecoding());
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, BadMagicNumber) {
  delta_file_[1] = 'Q' | 0x80;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, BadVersionNumber) {
  delta_file_[3] = 0x01;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, SecondaryCompressionNotSupported) {
  delta_file_[4] = 0x01;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, Decode) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_.data(),
                                   delta_file_.size(),
                                   &output_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

TEST_F(VCDiffInterleavedDecoderTest, DecodeWithChecksum) {
  ComputeAndAddChecksum();
  InitializeDeltaFile();
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_.data(),
                                   delta_file_.size(),
                                   &output_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

TEST_F(VCDiffInterleavedDecoderTest, ChecksumDoesNotMatch) {
  AddChecksum(0xBADBAD);
  InitializeDeltaFile();
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

// Remove one byte from the length of the chunk to process, and
// verify that an error is returned for FinishDecoding().
TEST_F(VCDiffInterleavedDecoderTest, FinishAfterDecodingPartialWindow) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_.data(),
                                   delta_file_.size() - 1,
                                   &output_));
  EXPECT_FALSE(decoder_.FinishDecoding());
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffInterleavedDecoderTest, FinishAfterDecodingPartialWindowHeader) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_.data(),
                                   delta_file_header_.size()
                                       + delta_window_header_.size() - 1,
                                   &output_));
  EXPECT_FALSE(decoder_.FinishDecoding());
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

// Fuzz bits to make sure decoder does not violently crash.
// This test has no expected behavior except that no crashes should occur.
// In some cases, changing bits will still decode to the correct target;
// for example, changing unused bits within a bitfield.
TEST_F(VCDiffInterleavedDecoderTest, FuzzBits) {
  while (FuzzOneByteInDeltaFile()) {
    decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
    if (decoder_.DecodeChunk(delta_file_.data(),
                             delta_file_.size(),
                             &output_)) {
      decoder_.FinishDecoding();
    }
    InitializeDeltaFile();
    output_.clear();
  }
}

// If a checksum is present, then fuzzing any of the bits may produce an error,
// but it should not result in an incorrect target being produced without
// an error.
TEST_F(VCDiffInterleavedDecoderTest, FuzzBitsWithChecksum) {
  ComputeAndAddChecksum();
  InitializeDeltaFile();
  while (FuzzOneByteInDeltaFile()) {
    decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
    if (decoder_.DecodeChunk(delta_file_.data(),
                             delta_file_.size(),
                             &output_)) {
      if (decoder_.FinishDecoding()) {
        // Decoding succeeded.  Make sure the correct target was produced.
        EXPECT_EQ(expected_target_, output_);
      }
    } else {
      EXPECT_EQ("", output_);
    }
    InitializeDeltaFile();
    output_.clear();
  }
}

TEST_F(VCDiffInterleavedDecoderTest, CheckAnnotatedOutput) {
  decoder_.EnableAnnotatedOutput();
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_.data(),
                                   delta_file_.size(),
                                   &output_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  string annotated_output;
  decoder_.GetAnnotatedOutput(&annotated_output);
  EXPECT_EQ(expected_annotated_target_, annotated_output);
}

TEST_F(VCDiffInterleavedDecoderTest, CopyMoreThanExpectedTarget) {
  delta_file_[delta_file_header_.size() + 0x0C] =
      FirstByteOfStringLength(kExpectedTarget);
  delta_file_[delta_file_header_.size() + 0x0D] =
      SecondByteOfStringLength(kExpectedTarget) + 1;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, CopySizeZero) {
  delta_file_[delta_file_header_.size() + 0x0C] = 0;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, CopySizeTooLargeByOne) {
  ++delta_file_[delta_file_header_.size() + 0x0C];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, CopySizeTooSmallByOne) {
  --delta_file_[delta_file_header_.size() + 0x0C];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, CopySizeMaxInt) {
  WriteMaxVarintAtOffset(0x0C, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, CopySizeNegative) {
  WriteNegativeVarintAtOffset(0x0C, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, CopySizeInvalid) {
  WriteInvalidVarintAtOffset(0x0C, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, CopyAddressBeyondHereAddress) {
  delta_file_[delta_file_header_.size() + 0x0D] =
      FirstByteOfStringLength(kDictionary);
  delta_file_[delta_file_header_.size() + 0x0E] =
      SecondByteOfStringLength(kDictionary);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, CopyAddressMaxInt) {
  WriteMaxVarintAtOffset(0x0D, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, CopyAddressNegative) {
  WriteNegativeVarintAtOffset(0x0D, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, CopyAddressInvalid) {
  WriteInvalidVarintAtOffset(0x0D, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, AddMoreThanExpectedTarget) {
  delta_file_[delta_file_header_.size() + 0x0F] =
      FirstByteOfStringLength(kExpectedTarget);
  delta_file_[delta_file_header_.size() + 0x10] =
      SecondByteOfStringLength(kExpectedTarget) + 1;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, AddSizeZero) {
  delta_file_[delta_file_header_.size() + 0x0F] = 0;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, AddSizeTooLargeByOne) {
  ++delta_file_[delta_file_header_.size() + 0x0F];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, AddSizeTooSmallByOne) {
  --delta_file_[delta_file_header_.size() + 0x0F];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, AddSizeMaxInt) {
  WriteMaxVarintAtOffset(0x0F, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, AddSizeNegative) {
  WriteNegativeVarintAtOffset(0x0F, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, AddSizeInvalid) {
  WriteInvalidVarintAtOffset(0x0F, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, RunMoreThanExpectedTarget) {
  delta_file_[delta_file_header_.size() + 0x5F] =
      FirstByteOfStringLength(kExpectedTarget);
  delta_file_[delta_file_header_.size() + 0x60] =
      SecondByteOfStringLength(kExpectedTarget) + 1;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, RunSizeZero) {
  delta_file_[delta_file_header_.size() + 0x5F] = 0;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, RunSizeTooLargeByOne) {
  ++delta_file_[delta_file_header_.size() + 0x5F];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, RunSizeTooSmallByOne) {
  --delta_file_[delta_file_header_.size() + 0x5F];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, RunSizeMaxInt) {
  WriteMaxVarintAtOffset(0x5F, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, RunSizeNegative) {
  WriteNegativeVarintAtOffset(0x5F, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTest, RunSizeInvalid) {
  WriteInvalidVarintAtOffset(0x5F, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

#if defined(HAVE_MPROTECT) && \
   (defined(HAVE_MEMALIGN) || defined(HAVE_POSIX_MEMALIGN))
TEST_F(VCDiffInterleavedDecoderTest, ShouldNotReadPastEndOfBuffer) {
  // Allocate two memory pages.
  const int page_size = getpagesize();
  void* two_pages = NULL;
#ifdef HAVE_POSIX_MEMALIGN
  posix_memalign(&two_pages, page_size, 2 * page_size);
#else  // !HAVE_POSIX_MEMALIGN
  two_pages = memalign(page_size, 2 * page_size);
#endif  // HAVE_POSIX_MEMALIGN
  char* const first_page = reinterpret_cast<char*>(two_pages);
  char* const second_page = first_page + page_size;

  // Place the delta string at the end of the first page.
  char* delta_with_guard = second_page - delta_file_.size();
  memcpy(delta_with_guard, delta_file_.data(), delta_file_.size());

  // Make the second page unreadable.
  mprotect(second_page, page_size, PROT_NONE);

  // Now perform the decode operation, which will cause a segmentation fault
  // if it reads past the end of the buffer.
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_with_guard,
                                   delta_file_.size(),
                                   &output_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);

  // Undo the mprotect.
  mprotect(second_page, page_size, PROT_READ|PROT_WRITE);
  free(two_pages);
}

TEST_F(VCDiffInterleavedDecoderTest, ShouldNotReadPastBeginningOfBuffer) {
  // Allocate two memory pages.
  const int page_size = getpagesize();
  void* two_pages = NULL;
#ifdef HAVE_POSIX_MEMALIGN
  posix_memalign(&two_pages, page_size, 2 * page_size);
#else  // !HAVE_POSIX_MEMALIGN
  two_pages = memalign(page_size, 2 * page_size);
#endif  // HAVE_POSIX_MEMALIGN
  char* const first_page = reinterpret_cast<char*>(two_pages);
  char* const second_page = first_page + page_size;

  // Make the first page unreadable.
  mprotect(first_page, page_size, PROT_NONE);

  // Place the delta string at the beginning of the second page.
  char* delta_with_guard = second_page;
  memcpy(delta_with_guard, delta_file_.data(), delta_file_.size());

  // Now perform the decode operation, which will cause a segmentation fault
  // if it reads past the beginning of the buffer.
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_with_guard,
                                   delta_file_.size(),
                                   &output_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);

  // Undo the mprotect.
  mprotect(first_page, page_size, PROT_READ|PROT_WRITE);
  free(two_pages);
}
#endif  // HAVE_MPROTECT && (HAVE_MEMALIGN || HAVE_POSIX_MEMALIGN)

// These are the same tests as for VCDiffInterleavedDecoderTest, with the added
// complication that instead of calling DecodeChunk() once with the entire data
// set, DecodeChunk() is called once for each byte of input.  This is intended
// to shake out any bugs with rewind and resume while parsing chunked data.

typedef VCDiffInterleavedDecoderTest VCDiffInterleavedDecoderTestByteByByte;

// Test headers, valid and invalid.

TEST_F(VCDiffInterleavedDecoderTestByteByByte, DecodeHeaderOnly) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_header_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_header_[i], 1, &output_));
  }
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, PartialHeaderNotEnough) {
  delta_file_.resize(delta_file_header_.size() - 2);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i], 1, &output_));
  }
  EXPECT_FALSE(decoder_.FinishDecoding());
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, BadMagicNumber) {
  delta_file_[1] = 'Q' | 0x80;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      // It should fail at the position that was altered
      EXPECT_EQ(1U, i);
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, BadVersionNumber) {
  delta_file_[3] = 0x01;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail at the position that was altered
      EXPECT_EQ(3U, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte,
       SecondaryCompressionNotSupported) {
  delta_file_[4] = 0x01;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail at the position that was altered
      EXPECT_EQ(4U, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, Decode) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i], 1, &output_));
  }
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, DecodeWithChecksum) {
  ComputeAndAddChecksum();
  InitializeDeltaFile();
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i], 1, &output_));
  }
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, ChecksumDoesNotMatch) {
  AddChecksum(0xBADBAD);
  InitializeDeltaFile();
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail after decoding the entire delta file
      EXPECT_EQ(delta_file_.size() - 1, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

// Fuzz bits to make sure decoder does not violently crash.
// This test has no expected behavior except that no crashes should occur.
// In some cases, changing bits will still decode to the correct target;
// for example, changing unused bits within a bitfield.
TEST_F(VCDiffInterleavedDecoderTestByteByByte, FuzzBits) {
  while (FuzzOneByteInDeltaFile()) {
    decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
    bool failed = false;
    for (size_t i = 0; i < delta_file_.size(); ++i) {
      if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
        failed = true;
        break;
      }
    }
    if (!failed) {
      decoder_.FinishDecoding();
    }
    InitializeDeltaFile();
    output_.clear();
  }
}

// If a checksum is present, then fuzzing any of the bits may produce an error,
// but it should not result in an incorrect target being produced without
// an error.
TEST_F(VCDiffInterleavedDecoderTestByteByByte, FuzzBitsWithChecksum) {
  ComputeAndAddChecksum();
  InitializeDeltaFile();
  while (FuzzOneByteInDeltaFile()) {
    decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
    bool failed = false;
    for (size_t i = 0; i < delta_file_.size(); ++i) {
      if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
        failed = true;
        break;
      }
    }
    if (!failed) {
      if (decoder_.FinishDecoding()) {
        // Decoding succeeded.  Make sure the correct target was produced.
        EXPECT_EQ(expected_target_, output_);
      }
    }
    // The decoder should not create more target bytes than were expected.
    EXPECT_GE(expected_target_.size(), output_.size());
    InitializeDeltaFile();
    output_.clear();
  }
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, CheckAnnotatedOutput) {
  decoder_.EnableAnnotatedOutput();
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i], 1, &output_));
  }
  EXPECT_TRUE(decoder_.FinishDecoding());
  string annotated_output;
  decoder_.GetAnnotatedOutput(&annotated_output);
  EXPECT_EQ(expected_annotated_target_, annotated_output);
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte,
       CopyInstructionsShouldFailIfNoSourceSegment) {
  // Replace the Win_Indicator and the source size and source offset with a
  // single 0 byte (a Win_Indicator for a window with no source segment.)
  delta_window_header_.replace(0, 4, "\0", 1);
  InitializeDeltaFile();
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // The first COPY instruction should fail.
      EXPECT_EQ(delta_file_header_.size() + delta_window_header_.size() + 2, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, CopyMoreThanExpectedTarget) {
  delta_file_[delta_file_header_.size() + 0x0C] =
      FirstByteOfStringLength(kExpectedTarget);
  delta_file_[delta_file_header_.size() + 0x0D] =
      SecondByteOfStringLength(kExpectedTarget) + 1;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail at the position that was altered
      EXPECT_EQ(delta_file_header_.size() + 0x0D, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

// A COPY instruction with an explicit size of 0 is not illegal according to the
// standard, although it is inefficient and should not be generated by any
// reasonable encoder.  Changing the size of a COPY instruction to zero will
// cause a failure because the generated target window size will not match the
// expected target size.
TEST_F(VCDiffInterleavedDecoderTestByteByByte, CopySizeZero) {
  delta_file_[delta_file_header_.size() + 0x0C] = 0;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, CopySizeTooLargeByOne) {
  ++delta_file_[delta_file_header_.size() + 0x0C];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, CopySizeTooSmallByOne) {
  --delta_file_[delta_file_header_.size() + 0x0C];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, CopySizeMaxInt) {
  WriteMaxVarintAtOffset(0x0C, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail at the position that was altered
      EXPECT_EQ(delta_file_header_.size() + 0x10, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, CopySizeNegative) {
  WriteNegativeVarintAtOffset(0x0C, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail at the position that was altered
      EXPECT_EQ(delta_file_header_.size() + 0x10, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, CopySizeInvalid) {
  WriteInvalidVarintAtOffset(0x0C, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail at the position that was altered
      EXPECT_EQ(delta_file_header_.size() + 0x10, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, CopyAddressBeyondHereAddress) {
  delta_file_[delta_file_header_.size() + 0x0D] =
      FirstByteOfStringLength(kDictionary);
  delta_file_[delta_file_header_.size() + 0x0E] =
      SecondByteOfStringLength(kDictionary);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail at the position that was altered
      EXPECT_EQ(delta_file_header_.size() + 0x0E, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, CopyAddressMaxInt) {
  WriteMaxVarintAtOffset(0x0D, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail at the position that was altered
      EXPECT_EQ(delta_file_header_.size() + 0x11, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, CopyAddressNegative) {
  WriteNegativeVarintAtOffset(0x0D, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail at the position that was altered
      EXPECT_EQ(delta_file_header_.size() + 0x11, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, CopyAddressInvalid) {
  WriteInvalidVarintAtOffset(0x0D, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail at the position that was altered
      EXPECT_EQ(delta_file_header_.size() + 0x11, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, AddMoreThanExpectedTarget) {
  delta_file_[delta_file_header_.size() + 0x0F] =
      FirstByteOfStringLength(kExpectedTarget);
  delta_file_[delta_file_header_.size() + 0x10] =
      SecondByteOfStringLength(kExpectedTarget) + 1;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail at the position that was altered
      EXPECT_EQ(delta_file_header_.size() + 0x10, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

// An ADD instruction with an explicit size of 0 is not illegal according to the
// standard, although it is inefficient and should not be generated by any
// reasonable encoder.  Changing the size of an ADD instruction to zero will
// cause a failure because the generated target window size will not match the
// expected target size.
TEST_F(VCDiffInterleavedDecoderTestByteByByte, AddSizeZero) {
  delta_file_[delta_file_header_.size() + 0x0F] = 0;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, AddSizeTooLargeByOne) {
  ++delta_file_[delta_file_header_.size() + 0x0F];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, AddSizeTooSmallByOne) {
  --delta_file_[delta_file_header_.size() + 0x0F];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, AddSizeMaxInt) {
  WriteMaxVarintAtOffset(0x0F, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail at the position that was altered
      EXPECT_EQ(delta_file_header_.size() + 0x13, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, AddSizeNegative) {
  WriteNegativeVarintAtOffset(0x0F, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail at the position that was altered
      EXPECT_EQ(delta_file_header_.size() + 0x13, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, AddSizeInvalid) {
  WriteInvalidVarintAtOffset(0x0F, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail at the position that was altered
      EXPECT_EQ(delta_file_header_.size() + 0x13, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, RunMoreThanExpectedTarget) {
  delta_file_[delta_file_header_.size() + 0x5F] =
      FirstByteOfStringLength(kExpectedTarget);
  delta_file_[delta_file_header_.size() + 0x60] =
      SecondByteOfStringLength(kExpectedTarget) + 1;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail at the position that was altered
      EXPECT_EQ(delta_file_header_.size() + 0x60, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

// A RUN instruction with an explicit size of 0 is not illegal according to the
// standard, although it is inefficient and should not be generated by any
// reasonable encoder.  Changing the size of a RUN instruction to zero will
// cause a failure because the generated target window size will not match the
// expected target size.
TEST_F(VCDiffInterleavedDecoderTestByteByByte, RunSizeZero) {
  delta_file_[delta_file_header_.size() + 0x5F] = 0;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, RunSizeTooLargeByOne) {
  ++delta_file_[delta_file_header_.size() + 0x5F];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, RunSizeTooSmallByOne) {
  --delta_file_[delta_file_header_.size() + 0x5F];
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, RunSizeMaxInt) {
  WriteMaxVarintAtOffset(0x5F, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail at the position that was altered
      EXPECT_EQ(delta_file_header_.size() + 0x63, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, RunSizeNegative) {
  WriteNegativeVarintAtOffset(0x5F, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail at the position that was altered
      EXPECT_EQ(delta_file_header_.size() + 0x63, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

TEST_F(VCDiffInterleavedDecoderTestByteByByte, RunSizeInvalid) {
  WriteInvalidVarintAtOffset(0x5F, 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      // It should fail at the position that was altered
      EXPECT_EQ(delta_file_header_.size() + 0x63, i);
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

// Use the interleaved file header with the standard encoding.  Should work.
class VCDiffDecoderInterleavedAllowedButNotUsed
    : public VCDiffStandardDecoderTest {
 public:
  VCDiffDecoderInterleavedAllowedButNotUsed() {
    delta_file_header_.assign(kInterleavedFileHeader,
                              sizeof(kInterleavedFileHeader));
  }
  virtual ~VCDiffDecoderInterleavedAllowedButNotUsed() { }
};

TEST_F(VCDiffDecoderInterleavedAllowedButNotUsed, Decode) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_.data(),
                                   delta_file_.size(),
                                   &output_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

TEST_F(VCDiffDecoderInterleavedAllowedButNotUsed, DecodeWithChecksum) {
  ComputeAndAddChecksum();
  InitializeDeltaFile();
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_.data(),
                                   delta_file_.size(),
                                   &output_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

typedef VCDiffDecoderInterleavedAllowedButNotUsed
    VCDiffDecoderInterleavedAllowedButNotUsedByteByByte;

TEST_F(VCDiffDecoderInterleavedAllowedButNotUsedByteByByte, Decode) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i], 1, &output_));
  }
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

TEST_F(VCDiffDecoderInterleavedAllowedButNotUsedByteByByte,
       DecodeWithChecksum) {
  ComputeAndAddChecksum();
  InitializeDeltaFile();
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i], 1, &output_));
  }
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

// Use the standard file header with the interleaved encoding.  Should fail.
class VCDiffDecoderInterleavedUsedButNotSupported
    : public VCDiffInterleavedDecoderTest {
 public:
  VCDiffDecoderInterleavedUsedButNotSupported() {
    delta_file_header_.assign(kStandardFileHeader, sizeof(kStandardFileHeader));
  }
  virtual ~VCDiffDecoderInterleavedUsedButNotSupported() { }
};

TEST_F(VCDiffDecoderInterleavedUsedButNotSupported, DecodeShouldFail) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                    delta_file_.size(),
                                    &output_));
  EXPECT_EQ("", output_);
}

TEST_F(VCDiffDecoderInterleavedUsedButNotSupported,
       DecodeByteByByteShouldFail) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  bool failed = false;
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    if (!decoder_.DecodeChunk(&delta_file_[i], 1, &output_)) {
      failed = true;
      break;
    }
  }
  EXPECT_TRUE(failed);
  // The decoder should not create more target bytes than were expected.
  EXPECT_GE(expected_target_.size(), output_.size());
}

// Divides up the standard encoding into eight separate delta file windows.
// Each delta instruction appears in its own window.
class VCDiffStandardWindowDecoderTest : public VCDiffDecoderTest {
 protected:
  VCDiffStandardWindowDecoderTest();
  virtual ~VCDiffStandardWindowDecoderTest() {}

 private:
  static const char kExpectedAnnotatedTarget[];
  static const char kWindowBody[];
};

const char VCDiffStandardWindowDecoderTest::kWindowBody[] = {
// Window 1:
    VCD_SOURCE,  // Win_Indicator: take source from dictionary
    FirstByteOfStringLength(kDictionary),  // Source segment size
    SecondByteOfStringLength(kDictionary),
    0x00,  // Source segment position: start of dictionary
    0x08,  // Length of the delta encoding
    0x1C,  // Size of the target window (28)
    0x00,  // Delta_indicator (no compression)
    0x00,  // length of data for ADDs and RUNs
    0x02,  // length of instructions section
    0x01,  // length of addresses for COPYs
    // No data for ADDs and RUNs
    // Instructions and sizes (length 2)
    0x13,  // VCD_COPY mode VCD_SELF, size 0
    0x1C,  // Size of COPY (28)
    // Addresses for COPYs (length 1)
    0x00,  // Start of dictionary
// Window 2:
    0x00,  // Win_Indicator: No source segment (ADD only)
    0x44,  // Length of the delta encoding
    0x3D,  // Size of the target window (61)
    0x00,  // Delta_indicator (no compression)
    0x3D,  // length of data for ADDs and RUNs
    0x02,  // length of instructions section
    0x00,  // length of addresses for COPYs
    // Data for ADD (length 61)
    ' ', 'I', ' ', 'h', 'a', 'v', 'e', ' ', 's', 'a', 'i', 'd', ' ',
    'i', 't', ' ', 't', 'w', 'i', 'c', 'e', ':', '\n',
    'T', 'h', 'a', 't', ' ',
    'a', 'l', 'o', 'n', 'e', ' ', 's', 'h', 'o', 'u', 'l', 'd', ' ',
    'e', 'n', 'c', 'o', 'u', 'r', 'a', 'g', 'e', ' ',
    't', 'h', 'e', ' ', 'c', 'r', 'e', 'w', '.', '\n',
    // Instructions and sizes (length 2)
    0x01,  // VCD_ADD size 0
    0x3D,  // Size of ADD (61)
    // No addresses for COPYs
// Window 3:
    VCD_TARGET,  // Win_Indicator: take source from decoded data
    0x59,  // Source segment size: length of data decoded so far
    0x00,  // Source segment position: start of decoded data
    0x08,  // Length of the delta encoding
    0x2C,  // Size of the target window
    0x00,  // Delta_indicator (no compression)
    0x00,  // length of data for ADDs and RUNs
    0x02,  // length of instructions section
    0x01,  // length of addresses for COPYs
    // No data for ADDs and RUNs
    // Instructions and sizes (length 2)
    0x23,  // VCD_COPY mode VCD_HERE, size 0
    0x2C,  // Size of COPY (44)
    // Addresses for COPYs (length 1)
    0x58,  // HERE mode address (27+61 back from here_address)
// Window 4:
    VCD_TARGET,  // Win_Indicator: take source from decoded data
    0x05,  // Source segment size: only 5 bytes needed for this COPY
    0x2E,  // Source segment position: offset for COPY
    0x09,  // Length of the delta encoding
    0x07,  // Size of the target window
    0x00,  // Delta_indicator (no compression)
    0x02,  // length of data for ADDs and RUNs
    0x01,  // length of instructions section
    0x01,  // length of addresses for COPYs
    // Data for ADD (length 2)
    'h', 'r',
    // Instructions and sizes (length 1)
    0xA7,  // VCD_ADD size 2 + VCD_COPY mode SELF size 5
    // Addresses for COPYs (length 1)
    0x00,  // SELF mode address (start of source segment)
// Window 5:
    0x00,  // Win_Indicator: No source segment (ADD only)
    0x0F,  // Length of the delta encoding
    0x09,  // Size of the target window
    0x00,  // Delta_indicator (no compression)
    0x09,  // length of data for ADDs and RUNs
    0x01,  // length of instructions section
    0x00,  // length of addresses for COPYs
    // Data for ADD (length 9)
    'W', 'h', 'a', 't', ' ', 'I', ' ', 't', 'e',
    // Instructions and sizes (length 1)
    0x0A,       // VCD_ADD size 9
    // No addresses for COPYs
// Window 6:
    0x00,  // Win_Indicator: No source segment (RUN only)
    0x08,  // Length of the delta encoding
    0x02,  // Size of the target window
    0x00,  // Delta_indicator (no compression)
    0x01,  // length of data for ADDs and RUNs
    0x02,  // length of instructions section
    0x00,  // length of addresses for COPYs
    // Data for RUN (length 1)
    'l',
    // Instructions and sizes (length 2)
    0x00,  // VCD_RUN size 0
    0x02,  // Size of RUN (2)
    // No addresses for COPYs
// Window 7:
    0x00,  // Win_Indicator: No source segment (ADD only)
    0x22,  // Length of the delta encoding
    0x1B,  // Size of the target window
    0x00,  // Delta_indicator (no compression)
    0x1B,  // length of data for ADDs and RUNs
    0x02,  // length of instructions section
    0x00,  // length of addresses for COPYs
    // Data for ADD: 4th section (length 27)
    ' ', 'y', 'o', 'u', ' ',
    't', 'h', 'r', 'e', 'e', ' ', 't', 'i', 'm', 'e', 's', ' ', 'i', 's', ' ',
    't', 'r', 'u', 'e', '.', '\"', '\n',
    // Instructions and sizes (length 2)
    0x01,  // VCD_ADD size 0
    0x1B,  // Size of ADD (27)
    // No addresses for COPYs
  };

// The window encoding should produce the same target file as the standard
// encoding, but the annotated target will be different because some of the
// <bmatch> tags (copying from the previously decoded data in the current target
// window) are changed to <dmatch> (copying from the previously decoded data in
// another target window, which is used as the source window for the current
// delta window.)
const char VCDiffStandardWindowDecoderTest::kExpectedAnnotatedTarget[] =
    "<dmatch>\"Just the place for a Snark!</dmatch>"
    "<literal> I have said it twice:\n"
    "That alone should encourage the crew.\n</literal>"
    "<dmatch>Just the place for a Snark! I have said it t</dmatch>"
    "<literal>hr</literal>"
    "<dmatch>ice:\n</dmatch>"
    "<literal>What I te</literal>"
    "<literal>ll</literal>"
    "<literal> you three times is true.\"\n</literal>";

VCDiffStandardWindowDecoderTest::VCDiffStandardWindowDecoderTest() {
  delta_file_header_.assign(kStandardFileHeader, sizeof(kStandardFileHeader));
  expected_annotated_target_.assign(kExpectedAnnotatedTarget);
  delta_window_body_.assign(kWindowBody, sizeof(kWindowBody));
}

TEST_F(VCDiffStandardWindowDecoderTest, Decode) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_.data(),
                                   delta_file_.size(),
                                   &output_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

// Bug 1287926: If DecodeChunk() stops in the middle of the window header,
// and the expected size of the current target window is smaller than the
// cumulative target bytes decoded so far, an underflow occurs and the decoder
// tries to allocate ~MAX_INT bytes.
TEST_F(VCDiffStandardWindowDecoderTest, DecodeBreakInFourthWindowHeader) {
  // Parse file header + first two windows.
  const int chunk_1_size = sizeof(kStandardFileHeader) + 83;
  // Parse third window, plus everything up to "Size of the target window" field
  // of fourth window, but do not parse complete header of fourth window.
  const int chunk_2_size = 12 + 5;
  CHECK_EQ(VCD_TARGET, static_cast<unsigned char>(delta_file_[chunk_1_size]));
  CHECK_EQ(0x00, static_cast<int>(delta_file_[chunk_1_size + chunk_2_size]));
  string output_chunk1, output_chunk2, output_chunk3;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[0],
                                   chunk_1_size,
                                   &output_chunk1));
  EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[chunk_1_size],
                                   chunk_2_size,
                                   &output_chunk2));
  EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[chunk_1_size + chunk_2_size],
                                   delta_file_.size()
                                       - (chunk_1_size + chunk_2_size),
                                   &output_chunk3));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_chunk1 + output_chunk2 + output_chunk3);
}

TEST_F(VCDiffStandardWindowDecoderTest, DecodeInTwoParts) {
  const size_t delta_file_size = delta_file_.size();
  for (size_t i = 1; i < delta_file_size; i++) {
    string output_chunk1, output_chunk2;
    decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[0],
                                     i,
                                     &output_chunk1));
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i],
                                     delta_file_size - i,
                                     &output_chunk2));
    EXPECT_TRUE(decoder_.FinishDecoding());
    EXPECT_EQ(expected_target_, output_chunk1 + output_chunk2);
  }
}

TEST_F(VCDiffStandardWindowDecoderTest, DecodeInThreeParts) {
  const size_t delta_file_size = delta_file_.size();
  for (size_t i = 1; i < delta_file_size - 1; i++) {
    for (size_t j = i + 1; j < delta_file_size; j++) {
      string output_chunk1, output_chunk2, output_chunk3;
      decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
      EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[0],
                                       i,
                                       &output_chunk1));
      EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i],
                                       j - i,
                                       &output_chunk2));
      EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[j],
                                       delta_file_size - j,
                                       &output_chunk3));
      EXPECT_TRUE(decoder_.FinishDecoding());
      EXPECT_EQ(expected_target_,
                output_chunk1 + output_chunk2 + output_chunk3);
    }
  }
}

typedef VCDiffStandardWindowDecoderTest
    VCDiffStandardWindowDecoderTestByteByByte;
TEST_F(VCDiffStandardWindowDecoderTestByteByByte, Decode) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i], 1, &output_));
  }
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

// Divides up the interleaved encoding into eight separate delta file windows.
class VCDiffInterleavedWindowDecoderTest
    : public VCDiffStandardWindowDecoderTest {
 protected:
  VCDiffInterleavedWindowDecoderTest();
  virtual ~VCDiffInterleavedWindowDecoderTest() {}
 private:
  static const char kWindowBody[];
};

const char VCDiffInterleavedWindowDecoderTest::kWindowBody[] = {
// Window 1:
    VCD_SOURCE,  // Win_Indicator: take source from dictionary
    FirstByteOfStringLength(kDictionary),  // Source segment size
    SecondByteOfStringLength(kDictionary),
    0x00,  // Source segment position: start of dictionary
    0x08,  // Length of the delta encoding
    0x1C,  // Size of the target window (28)
    0x00,  // Delta_indicator (no compression)
    0x00,  // length of data for ADDs and RUNs
    0x03,  // length of instructions section
    0x00,  // length of addresses for COPYs
    0x13,  // VCD_COPY mode VCD_SELF, size 0
    0x1C,  // Size of COPY (28)
    0x00,  // Start of dictionary
// Window 2:
    0x00,  // Win_Indicator: No source segment (ADD only)
    0x44,  // Length of the delta encoding
    0x3D,  // Size of the target window (61)
    0x00,  // Delta_indicator (no compression)
    0x00,  // length of data for ADDs and RUNs
    0x3F,  // length of instructions section
    0x00,  // length of addresses for COPYs
    0x01,  // VCD_ADD size 0
    0x3D,  // Size of ADD (61)
    ' ', 'I', ' ', 'h', 'a', 'v', 'e', ' ', 's', 'a', 'i', 'd', ' ',
    'i', 't', ' ', 't', 'w', 'i', 'c', 'e', ':', '\n',
    'T', 'h', 'a', 't', ' ',
    'a', 'l', 'o', 'n', 'e', ' ', 's', 'h', 'o', 'u', 'l', 'd', ' ',
    'e', 'n', 'c', 'o', 'u', 'r', 'a', 'g', 'e', ' ',
    't', 'h', 'e', ' ', 'c', 'r', 'e', 'w', '.', '\n',
// Window 3:
    VCD_TARGET,  // Win_Indicator: take source from decoded data
    0x59,  // Source segment size: length of data decoded so far
    0x00,  // Source segment position: start of decoded data
    0x08,  // Length of the delta encoding
    0x2C,  // Size of the target window
    0x00,  // Delta_indicator (no compression)
    0x00,  // length of data for ADDs and RUNs
    0x03,  // length of instructions section
    0x00,  // length of addresses for COPYs
    0x23,  // VCD_COPY mode VCD_HERE, size 0
    0x2C,  // Size of COPY (44)
    0x58,  // HERE mode address (27+61 back from here_address)
// Window 4:
    VCD_TARGET,  // Win_Indicator: take source from decoded data
    0x05,  // Source segment size: only 5 bytes needed for this COPY
    0x2E,  // Source segment position: offset for COPY
    0x09,  // Length of the delta encoding
    0x07,  // Size of the target window
    0x00,  // Delta_indicator (no compression)
    0x00,  // length of data for ADDs and RUNs
    0x04,  // length of instructions section
    0x00,  // length of addresses for COPYs
    0xA7,  // VCD_ADD size 2 + VCD_COPY mode SELF, size 5
    'h', 'r',
    0x00,  // SELF mode address (start of source segment)
// Window 5:
    0x00,  // Win_Indicator: No source segment (ADD only)
    0x0F,  // Length of the delta encoding
    0x09,  // Size of the target window
    0x00,  // Delta_indicator (no compression)
    0x00,  // length of data for ADDs and RUNs
    0x0A,  // length of instructions section
    0x00,  // length of addresses for COPYs
    0x0A,       // VCD_ADD size 9
    'W', 'h', 'a', 't', ' ', 'I', ' ', 't', 'e',
// Window 6:
    0x00,  // Win_Indicator: No source segment (RUN only)
    0x08,  // Length of the delta encoding
    0x02,  // Size of the target window
    0x00,  // Delta_indicator (no compression)
    0x00,  // length of data for ADDs and RUNs
    0x03,  // length of instructions section
    0x00,  // length of addresses for COPYs
    0x00,  // VCD_RUN size 0
    0x02,  // Size of RUN (2)
    'l',
// Window 7:
    0x00,  // Win_Indicator: No source segment (ADD only)
    0x22,  // Length of the delta encoding
    0x1B,  // Size of the target window
    0x00,  // Delta_indicator (no compression)
    0x00,  // length of data for ADDs and RUNs
    0x1D,  // length of instructions section
    0x00,  // length of addresses for COPYs
    0x01,  // VCD_ADD size 0
    0x1B,  // Size of ADD (27)
    ' ', 'y', 'o', 'u', ' ',
    't', 'h', 'r', 'e', 'e', ' ', 't', 'i', 'm', 'e', 's', ' ', 'i', 's', ' ',
    't', 'r', 'u', 'e', '.', '\"', '\n',
  };

VCDiffInterleavedWindowDecoderTest::VCDiffInterleavedWindowDecoderTest() {
  delta_file_header_.assign(kInterleavedFileHeader,
                            sizeof(kInterleavedFileHeader));
  // delta_window_header_ is left blank.  All window headers and bodies are
  // lumped together in delta_window_body_.  This means that AddChecksum()
  // cannot be used to test the checksum feature.
  delta_window_body_.assign(kWindowBody, sizeof(kWindowBody));
}

TEST_F(VCDiffInterleavedWindowDecoderTest, Decode) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_.data(),
                                   delta_file_.size(),
                                   &output_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

TEST_F(VCDiffInterleavedWindowDecoderTest, DecodeInTwoParts) {
  const size_t delta_file_size = delta_file_.size();
  for (size_t i = 1; i < delta_file_size; i++) {
    string output_chunk1, output_chunk2;
    decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[0],
                                     i,
                                     &output_chunk1));
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i],
                                     delta_file_size - i,
                                     &output_chunk2));
    EXPECT_TRUE(decoder_.FinishDecoding());
    EXPECT_EQ(expected_target_, output_chunk1 + output_chunk2);
  }
}

TEST_F(VCDiffInterleavedWindowDecoderTest, DecodeInThreeParts) {
  const size_t delta_file_size = delta_file_.size();
  for (size_t i = 1; i < delta_file_size - 1; i++) {
    for (size_t j = i + 1; j < delta_file_size; j++) {
      string output_chunk1, output_chunk2, output_chunk3;
      decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
      EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[0],
                                       i,
                                       &output_chunk1));
      EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i],
                                       j - i,
                                       &output_chunk2));
      EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[j],
                                       delta_file_size - j,
                                       &output_chunk3));
      EXPECT_TRUE(decoder_.FinishDecoding());
      EXPECT_EQ(expected_target_,
                output_chunk1 + output_chunk2 + output_chunk3);
    }
  }
}

typedef VCDiffInterleavedWindowDecoderTest
    VCDiffInterleavedWindowDecoderTestByteByByte;

TEST_F(VCDiffInterleavedWindowDecoderTestByteByByte, Decode) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i], 1, &output_));
  }
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

// The original version of VCDiffDecoder did not allow the caller to modify the
// contents of output_string between calls to DecodeChunk().  That restriction
// has been removed.  Verify that the same result is still produced if the
// output string is cleared after each call to DecodeChunk().  Use the window
// encoding because it refers back to the previously decoded target data, which
// is the feature that would fail if the restriction still applied.
//
TEST_F(VCDiffInterleavedWindowDecoderTest, OutputStringCanBeModified) {
  string temp_output;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i], 1, &temp_output));
    output_.append(temp_output);
    temp_output.clear();
  }
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

TEST_F(VCDiffInterleavedWindowDecoderTest, OutputStringIsPreserved) {
  const string previous_data("Previous data");
  output_ = previous_data;
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i], 1, &output_));
  }
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(previous_data + expected_target_, output_);
}

// A decode job that tests the ability to COPY across the boundary between
// source data and target data.
class VCDiffStandardCrossDecoderTest : public VCDiffDecoderTest {
 protected:
  static const char kExpectedTarget[];
  static const char kExpectedAnnotatedTarget[];
  static const char kWindowHeader[];
  static const char kWindowBody[];

  VCDiffStandardCrossDecoderTest();
  virtual ~VCDiffStandardCrossDecoderTest() {}
};

const char VCDiffStandardCrossDecoderTest::kWindowHeader[] = {
    VCD_SOURCE,  // Win_Indicator: take source from dictionary
    FirstByteOfStringLength(kDictionary),  // Source segment size
    SecondByteOfStringLength(kDictionary),
    0x00,  // Source segment position: start of dictionary
    0x15,  // Length of the delta encoding
    StringLengthAsByte(kExpectedTarget),  // Size of the target window
    0x00,  // Delta_indicator (no compression)
    0x07,  // length of data for ADDs and RUNs
    0x06,  // length of instructions section
    0x03   // length of addresses for COPYs
  };

const char VCDiffStandardCrossDecoderTest::kWindowBody[] = {
    // Data for ADD (length 7)
    'S', 'p', 'i', 'd', 'e', 'r', 's',
    // Instructions and sizes (length 6)
    0x01,  // VCD_ADD size 0
    0x07,  // Size of ADD (7)
    0x23,  // VCD_COPY mode VCD_HERE, size 0
    0x19,  // Size of COPY (25)
    0x14,  // VCD_COPY mode VCD_SELF, size 4
    0x25,  // VCD_COPY mode VCD_HERE, size 5
    // Addresses for COPYs (length 3)
    0x15,  // HERE mode address for 1st copy (21 back from here_address)
    0x06,  // SELF mode address for 2nd copy
    0x14   // HERE mode address for 3rd copy
  };

const char VCDiffStandardCrossDecoderTest::kExpectedTarget[] =
    "Spiders in his hair.\n"
    "Spiders in the air.\n";

const char VCDiffStandardCrossDecoderTest::kExpectedAnnotatedTarget[] =
    "<literal>Spiders</literal>"
    "<dmatch> in his hair.\n</dmatch>"  // crosses source-target boundary
    "<bmatch>Spiders in </bmatch>"
    "<dmatch>the </dmatch>"
    "<bmatch>air.\n</bmatch>";

VCDiffStandardCrossDecoderTest::VCDiffStandardCrossDecoderTest() {
  delta_file_header_.assign(kStandardFileHeader, sizeof(kStandardFileHeader));
  delta_window_header_.assign(kWindowHeader, sizeof(kWindowHeader));
  delta_window_body_.assign(kWindowBody, sizeof(kWindowBody));
  expected_target_.assign(kExpectedTarget);
  expected_annotated_target_.assign(kExpectedAnnotatedTarget);
}

TEST_F(VCDiffStandardCrossDecoderTest, Decode) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_.data(),
                                   delta_file_.size(),
                                   &output_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

typedef VCDiffStandardCrossDecoderTest VCDiffStandardCrossDecoderTestByteByByte;

TEST_F(VCDiffStandardCrossDecoderTestByteByByte, Decode) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i], 1, &output_));
  }
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

// The same decode job that tests the ability to COPY across the boundary
// between source data and target data, but using the interleaved format rather
// than the standard format.
class VCDiffInterleavedCrossDecoderTest
    : public VCDiffStandardCrossDecoderTest {
 protected:
  VCDiffInterleavedCrossDecoderTest();
  virtual ~VCDiffInterleavedCrossDecoderTest() {}

 private:
  static const char kWindowHeader[];
  static const char kWindowBody[];
};

const char VCDiffInterleavedCrossDecoderTest::kWindowHeader[] = {
    VCD_SOURCE,  // Win_Indicator: take source from dictionary
    FirstByteOfStringLength(kDictionary),  // Source segment size
    SecondByteOfStringLength(kDictionary),
    0x00,  // Source segment position: start of dictionary
    0x15,  // Length of the delta encoding
    StringLengthAsByte(kExpectedTarget),  // Size of the target window
    0x00,  // Delta_indicator (no compression)
    0x00,  // length of data for ADDs and RUNs
    0x10,  // length of instructions section
    0x00,  // length of addresses for COPYs
  };

const char VCDiffInterleavedCrossDecoderTest::kWindowBody[] = {
    0x01,  // VCD_ADD size 0
    0x07,  // Size of ADD (7)
    // Data for ADD (length 7)
    'S', 'p', 'i', 'd', 'e', 'r', 's',
    0x23,  // VCD_COPY mode VCD_HERE, size 0
    0x19,  // Size of COPY (25)
    0x15,  // HERE mode address for 1st copy (21 back from here_address)
    0x14,  // VCD_COPY mode VCD_SELF, size 4
    0x06,  // SELF mode address for 2nd copy
    0x25,  // VCD_COPY mode VCD_HERE, size 5
    0x14   // HERE mode address for 3rd copy
  };

VCDiffInterleavedCrossDecoderTest::VCDiffInterleavedCrossDecoderTest() {
  delta_file_header_.assign(kInterleavedFileHeader,
                            sizeof(kInterleavedFileHeader));
  delta_window_header_.assign(kWindowHeader, sizeof(kWindowHeader));
  delta_window_body_.assign(kWindowBody, sizeof(kWindowBody));
}

TEST_F(VCDiffInterleavedCrossDecoderTest, Decode) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_.data(),
                                   delta_file_.size(),
                                   &output_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

TEST_F(VCDiffInterleavedCrossDecoderTest, DecodeWithChecksum) {
  ComputeAndAddChecksum();
  InitializeDeltaFile();
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_.data(),
                                   delta_file_.size(),
                                   &output_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

typedef VCDiffInterleavedCrossDecoderTest
    VCDiffInterleavedCrossDecoderTestByteByByte;

TEST_F(VCDiffInterleavedCrossDecoderTestByteByByte, Decode) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i], 1, &output_));
  }
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

TEST_F(VCDiffInterleavedCrossDecoderTestByteByByte, DecodeWithChecksum) {
  ComputeAndAddChecksum();
  InitializeDeltaFile();
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i], 1, &output_));
  }
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

// Test using a custom code table and custom cache sizes with interleaved
// format.
class VCDiffCustomCodeTableDecoderTest : public VCDiffInterleavedDecoderTest {
 protected:
  static const char kFileHeader[];
  static const char kWindowHeader[];
  static const char kWindowBody[];
  static const char kEncodedCustomCodeTable[];

  VCDiffCustomCodeTableDecoderTest();
  virtual ~VCDiffCustomCodeTableDecoderTest() {}
};

const char VCDiffCustomCodeTableDecoderTest::kFileHeader[] = {
    0xD6,  // 'V' | 0x80
    0xC3,  // 'C' | 0x80
    0xC4,  // 'D' | 0x80
    'S',   // SDCH version code
    0x02   // Hdr_Indicator: Use custom code table
  };

// Make a custom code table that includes exactly the instructions we need
// to encode the first test's data without using any explicit length values.
// Be careful not to replace any existing opcodes that have size 0,
// to ensure that the custom code table is valid (can express all possible
// values of inst (also known as instruction type) and mode with size 0.)
// This encoding uses interleaved format, which is easier to read.
//
// Here are the changes to the standard code table:
// ADD size 2 (opcode 3) => RUN size 2 (inst1[3] = VCD_RUN)
// ADD size 16 (opcode 17) => ADD size 27 (size1[17] = 27)
// ADD size 17 (opcode 18) => ADD size 61 (size1[18] = 61)
// COPY mode 0 size 18 (opcode 34) => COPY mode 0 size 28 (size1[34] = 28)
// COPY mode 1 size 18 (opcode 50) => COPY mode 1 size 44 (size1[50] = 44)
//
const char VCDiffCustomCodeTableDecoderTest::kEncodedCustomCodeTable[] = {
    0xD6,  // 'V' | 0x80
    0xC3,  // 'C' | 0x80
    0xC4,  // 'D' | 0x80
    'S',   // SDCH version code
    0x00,  // Hdr_Indicator: no custom code table, no compression
    VCD_SOURCE,  // Win_Indicator: take source from dictionary
    (sizeof(VCDiffCodeTableData) >> 7) | 0x80,  // First byte of table length
    sizeof(VCDiffCodeTableData) & 0x7F,  // Second byte of table length
    0x00,  // Source segment position: start of default code table
    0x1F,  // Length of the delta encoding
    (sizeof(VCDiffCodeTableData) >> 7) | 0x80,  // First byte of table length
    sizeof(VCDiffCodeTableData) & 0x7F,  // Second byte of table length
    0x00,  // Delta_indicator (no compression)
    0x00,  // length of data for ADDs and RUNs (unused)
    0x19,  // length of interleaved section
    0x00,  // length of addresses for COPYs (unused)
    0x05,  // VCD_ADD size 4
    // Data for ADD (length 4)
    VCD_RUN, VCD_ADD, VCD_ADD, VCD_RUN,
    0x13,  // VCD_COPY mode VCD_SELF size 0
    0x84,  // Size of copy: upper bits (512 - 4 + 17 = 525)
    0x0D,  // Size of copy: lower bits
    0x04,  // Address of COPY
    0x03,  // VCD_ADD size 2
    // Data for ADD (length 2)
    0x1B, 0x3D,
    0x3F,  // VCD_COPY mode VCD_NEAR(0) size 15
    0x84,  // Address of copy: upper bits (525 + 2 = 527)
    0x0F,  // Address of copy: lower bits
    0x02,  // VCD_ADD size 1
    // Data for ADD (length 1)
    0x1C,
    0x4F,  // VCD_COPY mode VCD_NEAR(1) size 15
    0x10,  // Address of copy
    0x02,  // VCD_ADD size 1
    // Data for ADD (length 1)
    0x2C,
    0x53,  // VCD_COPY mode VCD_NEAR(2) size 0
    0x87,  // Size of copy: upper bits (256 * 4 - 51 = 973)
    0x4D,  // Size of copy: lower bits
    0x10   // Address of copy
  };

// This is similar to VCDiffInterleavedDecoderTest, but uses the custom code
// table to eliminate the need to explicitly encode instruction sizes.
// Notice that NEAR(0) mode is used here where NEAR(1) mode was used in
// VCDiffInterleavedDecoderTest.  This is because the custom code table
// has the size of the NEAR cache set to 1; only the most recent
// COPY instruction is available.  This will also be a test of
// custom cache sizes.
const char VCDiffCustomCodeTableDecoderTest::kWindowHeader[] = {
    VCD_SOURCE,  // Win_Indicator: take source from dictionary
    FirstByteOfStringLength(kDictionary),  // Source segment size
    SecondByteOfStringLength(kDictionary),
    0x00,  // Source segment position: start of dictionary
    0x74,  // Length of the delta encoding
    FirstByteOfStringLength(kExpectedTarget),  // Size of the target window
    SecondByteOfStringLength(kExpectedTarget),
    0x00,  // Delta_indicator (no compression)
    0x00,  // length of data for ADDs and RUNs (unused)
    0x6E,  // length of interleaved section
    0x00   // length of addresses for COPYs (unused)
  };

const char VCDiffCustomCodeTableDecoderTest::kWindowBody[] = {
    0x22,  // VCD_COPY mode VCD_SELF, size 28
    0x00,  // Address of COPY: Start of dictionary
    0x12,  // VCD_ADD size 61
    // Data for ADD (length 61)
    ' ', 'I', ' ', 'h', 'a', 'v', 'e', ' ', 's', 'a', 'i', 'd', ' ',
    'i', 't', ' ', 't', 'w', 'i', 'c', 'e', ':', '\n',
    'T', 'h', 'a', 't', ' ',
    'a', 'l', 'o', 'n', 'e', ' ', 's', 'h', 'o', 'u', 'l', 'd', ' ',
    'e', 'n', 'c', 'o', 'u', 'r', 'a', 'g', 'e', ' ',
    't', 'h', 'e', ' ', 'c', 'r', 'e', 'w', '.', '\n',
    0x32,  // VCD_COPY mode VCD_HERE, size 44
    0x58,  // HERE mode address (27+61 back from here_address)
    0xBF,  // VCD_ADD size 2 + VCD_COPY mode NEAR(0), size 5
    // Data for ADDs: 2nd section (length 2)
    'h', 'r',
    0x2D,  // NEAR(0) mode address (45 after prior address)
    0x0A,  // VCD_ADD size 9
    // Data for ADDs: 3rd section (length 9)
    'W', 'h', 'a', 't', ' ',
    'I', ' ', 't', 'e',
    0x03,  // VCD_RUN size 2
    // Data for RUN: 4th section (length 1)
    'l',
    0x11,  // VCD_ADD size 27
    // Data for ADD: 4th section (length 27)
    ' ', 'y', 'o', 'u', ' ',
    't', 'h', 'r', 'e', 'e', ' ', 't', 'i', 'm', 'e', 's', ' ', 'i', 's', ' ',
    't', 'r', 'u', 'e', '.', '\"', '\n'
  };

VCDiffCustomCodeTableDecoderTest::VCDiffCustomCodeTableDecoderTest() {
  delta_file_header_.assign(kFileHeader, sizeof(kFileHeader));
  delta_file_header_.push_back(0x01);  // NEAR cache size (custom)
  delta_file_header_.push_back(0x06);  // SAME cache size (custom)
  delta_file_header_.append(kEncodedCustomCodeTable,
                            sizeof(kEncodedCustomCodeTable));
  delta_window_header_.assign(kWindowHeader, sizeof(kWindowHeader));
  delta_window_body_.assign(kWindowBody, sizeof(kWindowBody));
}

TEST_F(VCDiffCustomCodeTableDecoderTest, CustomCodeTableEncodingMatches) {
  VCDiffCodeTableData custom_code_table(
    VCDiffCodeTableData::kDefaultCodeTableData);
  custom_code_table.inst1[3] = VCD_RUN;
  custom_code_table.size1[17] = 27;
  custom_code_table.size1[18] = 61;
  custom_code_table.size1[34] = 28;
  custom_code_table.size1[50] = 44;

  decoder_.StartDecoding(
      reinterpret_cast<const char*>(
          &VCDiffCodeTableData::kDefaultCodeTableData),
      sizeof(VCDiffCodeTableData::kDefaultCodeTableData));
  EXPECT_TRUE(decoder_.DecodeChunk(kEncodedCustomCodeTable,
                                   sizeof(kEncodedCustomCodeTable),
                                   &output_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(sizeof(custom_code_table), output_.size());
  const VCDiffCodeTableData* decoded_table =
      reinterpret_cast<const VCDiffCodeTableData*>(output_.data());
  EXPECT_EQ(VCD_RUN, decoded_table->inst1[0]);
  EXPECT_EQ(VCD_RUN, decoded_table->inst1[3]);
  EXPECT_EQ(27, decoded_table->size1[17]);
  EXPECT_EQ(61, decoded_table->size1[18]);
  EXPECT_EQ(28, decoded_table->size1[34]);
  EXPECT_EQ(44, decoded_table->size1[50]);
  for (int i = 0; i < VCDiffCodeTableData::kCodeTableSize; ++i) {
    EXPECT_EQ(custom_code_table.inst1[i], decoded_table->inst1[i]);
    EXPECT_EQ(custom_code_table.inst2[i], decoded_table->inst2[i]);
    EXPECT_EQ(custom_code_table.size1[i], decoded_table->size1[i]);
    EXPECT_EQ(custom_code_table.size2[i], decoded_table->size2[i]);
    EXPECT_EQ(custom_code_table.mode1[i], decoded_table->mode1[i]);
    EXPECT_EQ(custom_code_table.mode2[i], decoded_table->mode2[i]);
  }
}

TEST_F(VCDiffCustomCodeTableDecoderTest, DecodeUsingCustomCodeTable) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_.data(),
                                   delta_file_.size(),
                                   &output_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

TEST_F(VCDiffCustomCodeTableDecoderTest, IncompleteCustomCodeTable) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_TRUE(decoder_.DecodeChunk(delta_file_header_.data(),
                                   delta_file_header_.size() - 1,
                                   &output_));
  EXPECT_FALSE(decoder_.FinishDecoding());
  EXPECT_EQ("", output_);
}

typedef VCDiffCustomCodeTableDecoderTest
    VCDiffCustomCodeTableDecoderTestByteByByte;

TEST_F(VCDiffCustomCodeTableDecoderTestByteByByte, DecodeUsingCustomCodeTable) {
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i], 1, &output_));
  }
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(expected_target_, output_);
}

TEST_F(VCDiffCustomCodeTableDecoderTestByteByByte, IncompleteCustomCodeTable) {
  delta_file_.resize(delta_file_header_.size() - 1);
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  for (size_t i = 0; i < delta_file_.size(); ++i) {
    EXPECT_TRUE(decoder_.DecodeChunk(&delta_file_[i], 1, &output_));
  }
  EXPECT_FALSE(decoder_.FinishDecoding());
  EXPECT_EQ("", output_);
}

#ifdef GTEST_HAS_DEATH_TEST
typedef VCDiffCustomCodeTableDecoderTest VCDiffCustomCodeTableDecoderDeathTest;

TEST_F(VCDiffCustomCodeTableDecoderDeathTest, BadCustomCacheSizes) {
  delta_file_header_.assign(kFileHeader, sizeof(kFileHeader));
  delta_file_header_.push_back(0x81);  // NEAR cache size (top bit)
  delta_file_header_.push_back(0x10);  // NEAR cache size (custom value 0x90)
  delta_file_header_.push_back(0x81);  // SAME cache size (top bit)
  delta_file_header_.push_back(0x10);  // SAME cache size (custom value 0x90)
  delta_file_header_.append(kEncodedCustomCodeTable,
                            sizeof(kEncodedCustomCodeTable));
  InitializeDeltaFile();
  decoder_.StartDecoding(dictionary_.data(), dictionary_.size());
  EXPECT_DEBUG_DEATH(EXPECT_FALSE(decoder_.DecodeChunk(delta_file_.data(),
                                                       delta_file_.size(),
                                                       &output_)),
                     "cache");
  EXPECT_EQ("", output_);
}
#endif  // GTEST_HAS_DEATH_TEST

}  // namespace open_vcdiff
}  // unnamed namespace
