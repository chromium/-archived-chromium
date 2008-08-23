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
#include "google/vcencoder.h"
#include <algorithm>
#include <cstdlib>  // free, posix_memalign
#include <cstring>  // memcpy
#include <string>
#include <vector>
#include "blockhash.h"
#include "checksum.h"
#include "logging.h"
#include "google/output_string.h"
#include "testing.h"
#include "varint_bigendian.h"
#include "google/vcdecoder.h"
#include "vcdiff_defs.h"

#ifdef HAVE_EXT_ROPE
#include <ext/rope>
#include "output_string_crope.h"
using __gnu_cxx::crope;
#endif  // HAVE_EXT_ROPE

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

static const size_t kFileHeaderSize = sizeof(DeltaFileHeader);

// This is to check the maximum possible encoding size
// if using a single ADD instruction, so assume that the
// dictionary size, the length of the ADD data, the size
// of the target window, and the length of the delta window
// are all two-byte Varints, that is, 128 <= length < 4096.
// This figure includes three extra bytes for a zero-sized
// ADD instruction with a two-byte Varint explicit size.
// Any additional COPY & ADD instructions must reduce
// the length of the encoding from this maximum.
static const size_t kWindowHeaderSize = 21;

class VerifyEncodedBytesTest : public testing::Test {
 public:
  VerifyEncodedBytesTest() : delta_index_(0) { }
  virtual ~VerifyEncodedBytesTest() { }

  void ExpectByte(unsigned char b) {
    EXPECT_EQ(b, static_cast<unsigned char>(delta_[delta_index_]));
    ++delta_index_;
  }

  void ExpectString(const char* s) {
    const size_t size = strlen(s);  // don't include terminating NULL char
    EXPECT_EQ(string(s, size),
              string(delta_data() + delta_index_, size));
    delta_index_ += size;
  }

  void ExpectNoMoreBytes() {
    EXPECT_EQ(delta_index_, delta_size());
  }

  void ExpectSize(size_t size) {
    const char* delta_size_pos = &delta_[delta_index_];
    EXPECT_EQ(size,
              static_cast<size_t>(
                  VarintBE<int32_t>::Parse(delta_data() + delta_size(),
                                           &delta_size_pos)));
    delta_index_ = delta_size_pos - delta_data();
  }

  void ExpectChecksum(VCDChecksum checksum) {
    const char* delta_checksum_pos = &delta_[delta_index_];
    EXPECT_EQ(checksum,
              static_cast<VCDChecksum>(
                  VarintBE<int64_t>::Parse(delta_data() + delta_size(),
                                           &delta_checksum_pos)));
    delta_index_ = delta_checksum_pos - delta_data();
  }

  const string& delta_as_const() const { return delta_; }
  string* delta() { return &delta_; }

  const char* delta_data() const { return delta_as_const().data(); }
  size_t delta_size() const { return delta_as_const().size(); }

 private:
  string delta_;
  size_t delta_index_;
};

class VCDiffEncoderTest : public VerifyEncodedBytesTest {
 protected:
  static const char kDictionary[];
  static const char kTarget[];

  VCDiffEncoderTest();
  virtual ~VCDiffEncoderTest() { }

  void TestWithFixedChunkSize(size_t chunk_size);
  void TestWithEncodedChunkVector(size_t chunk_size);

  HashedDictionary hashed_dictionary_;
  VCDiffStreamingEncoder encoder_;
  VCDiffStreamingDecoder decoder_;
  VCDiffEncoder simple_encoder_;
  VCDiffDecoder simple_decoder_;

  string result_target_;
};

const char VCDiffEncoderTest::kDictionary[] =
    "\"Just the place for a Snark!\" the Bellman cried,\n"
    "As he landed his crew with care;\n"
    "Supporting each man on the top of the tide\n"
    "By a finger entwined in his hair.\n";

const char VCDiffEncoderTest::kTarget[] =
    "\"Just the place for a Snark! I have said it twice:\n"
    "That alone should encourage the crew.\n"
    "Just the place for a Snark! I have said it thrice:\n"
    "What I tell you three times is true.\"\n";

VCDiffEncoderTest::VCDiffEncoderTest()
    : hashed_dictionary_(kDictionary, sizeof(kDictionary)),
      encoder_(&hashed_dictionary_,
               VCD_FORMAT_INTERLEAVED | VCD_FORMAT_CHECKSUM,
               /* look_for_target_matches = */ true),
      simple_encoder_(kDictionary, sizeof(kDictionary)) {
  EXPECT_TRUE(hashed_dictionary_.Init());
}

TEST_F(VCDiffEncoderTest, EncodeBeforeStartEncoding) {
  EXPECT_FALSE(encoder_.EncodeChunk(kTarget, strlen(kTarget), delta()));
}

TEST_F(VCDiffEncoderTest, FinishBeforeStartEncoding) {
  EXPECT_FALSE(encoder_.FinishEncoding(delta()));
}

TEST_F(VCDiffEncoderTest, EncodeDecodeNothing) {
  HashedDictionary nothing_dictionary("", 0);
  EXPECT_TRUE(nothing_dictionary.Init());
  VCDiffStreamingEncoder nothing_encoder(&nothing_dictionary,
                                         VCD_STANDARD_FORMAT,
                                         false);
  EXPECT_TRUE(nothing_encoder.StartEncoding(delta()));
  EXPECT_TRUE(nothing_encoder.FinishEncoding(delta()));
  decoder_.StartDecoding("", 0);
  EXPECT_TRUE(decoder_.DecodeChunk(delta_data(),
                                   delta_size(),
                                   &result_target_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_TRUE(result_target_.empty());
}

// A NULL dictionary pointer is legal as long as the dictionary size is 0.
TEST_F(VCDiffEncoderTest, EncodeDecodeNullDictionaryPtr) {
  HashedDictionary null_dictionary(NULL, 0);
  EXPECT_TRUE(null_dictionary.Init());
  VCDiffStreamingEncoder null_encoder(&null_dictionary,
                                      VCD_STANDARD_FORMAT,
                                      false);
  EXPECT_TRUE(null_encoder.StartEncoding(delta()));
  EXPECT_TRUE(null_encoder.EncodeChunk(kTarget, strlen(kTarget), delta()));
  EXPECT_TRUE(null_encoder.FinishEncoding(delta()));
  EXPECT_GE(strlen(kTarget) + kFileHeaderSize + kWindowHeaderSize,
            delta_size());
  decoder_.StartDecoding(NULL, 0);
  EXPECT_TRUE(decoder_.DecodeChunk(delta_data(),
                                   delta_size(),
                                   &result_target_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(kTarget, result_target_);
}

TEST_F(VCDiffEncoderTest, EncodeDecodeSimple) {
  EXPECT_TRUE(simple_encoder_.Encode(kTarget, strlen(kTarget), delta()));
  EXPECT_GE(strlen(kTarget) + kFileHeaderSize + kWindowHeaderSize,
            delta_size());
  EXPECT_TRUE(simple_decoder_.Decode(kDictionary,
                                     sizeof(kDictionary),
                                     delta_as_const(),
                                     &result_target_));
  EXPECT_EQ(kTarget, result_target_);
}

TEST_F(VCDiffEncoderTest, EncodeDecodeInterleaved) {
  simple_encoder_.SetFormatFlags(VCD_FORMAT_INTERLEAVED);
  EXPECT_TRUE(simple_encoder_.Encode(kTarget, strlen(kTarget), delta()));
  EXPECT_GE(strlen(kTarget) + kFileHeaderSize + kWindowHeaderSize,
            delta_size());
  EXPECT_TRUE(simple_decoder_.Decode(kDictionary,
                                     sizeof(kDictionary),
                                     delta_as_const(),
                                     &result_target_));
  EXPECT_EQ(kTarget, result_target_);
}

TEST_F(VCDiffEncoderTest, EncodeDecodeInterleavedChecksum) {
  simple_encoder_.SetFormatFlags(VCD_FORMAT_INTERLEAVED | VCD_FORMAT_CHECKSUM);
  EXPECT_TRUE(simple_encoder_.Encode(kTarget,
                                     strlen(kTarget),
                                     delta()));
  EXPECT_GE(strlen(kTarget) + kFileHeaderSize + kWindowHeaderSize,
            delta_size());
  EXPECT_TRUE(simple_decoder_.Decode(kDictionary,
                                     sizeof(kDictionary),
                                     delta_as_const(),
                                     &result_target_));
  EXPECT_EQ(kTarget, result_target_);
}

TEST_F(VCDiffEncoderTest, EncodeDecodeSingleChunk) {
  EXPECT_TRUE(encoder_.StartEncoding(delta()));
  EXPECT_TRUE(encoder_.EncodeChunk(kTarget, strlen(kTarget), delta()));
  EXPECT_TRUE(encoder_.FinishEncoding(delta()));
  EXPECT_GE(strlen(kTarget) + kFileHeaderSize + kWindowHeaderSize,
            delta_size());
  decoder_.StartDecoding(kDictionary, sizeof(kDictionary));
  EXPECT_TRUE(decoder_.DecodeChunk(delta_data(),
                                   delta_size(),
                                   &result_target_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(kTarget, result_target_);
}

TEST_F(VCDiffEncoderTest, EncodeDecodeSeparate) {
  string delta_start, delta_encode, delta_finish;
  EXPECT_TRUE(encoder_.StartEncoding(&delta_start));
  EXPECT_TRUE(encoder_.EncodeChunk(kTarget, strlen(kTarget), &delta_encode));
  EXPECT_TRUE(encoder_.FinishEncoding(&delta_finish));
  EXPECT_GE(strlen(kTarget) + kFileHeaderSize + kWindowHeaderSize,
            delta_start.size() + delta_encode.size() + delta_finish.size());
  decoder_.StartDecoding(kDictionary, sizeof(kDictionary));
  EXPECT_TRUE(decoder_.DecodeChunk(delta_start.data(),
                                   delta_start.size(),
                                   &result_target_));
  EXPECT_TRUE(decoder_.DecodeChunk(delta_encode.data(),
                                   delta_encode.size(),
                                   &result_target_));
  EXPECT_TRUE(decoder_.DecodeChunk(delta_finish.data(),
                                   delta_finish.size(),
                                   &result_target_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(kTarget, result_target_);
}

#ifdef HAVE_EXT_ROPE
// Test that the crope class can be used in place of a string for encoding
// and decoding.
TEST_F(VCDiffEncoderTest, EncodeDecodeCrope) {
  crope delta_crope, result_crope;
  EXPECT_TRUE(encoder_.StartEncoding(&delta_crope));
  EXPECT_TRUE(encoder_.EncodeChunk(kTarget, strlen(kTarget), &delta_crope));
  EXPECT_TRUE(encoder_.FinishEncoding(&delta_crope));
  EXPECT_GE(strlen(kTarget) + kFileHeaderSize + kWindowHeaderSize,
            delta_crope.size());
  decoder_.StartDecoding(kDictionary, sizeof(kDictionary));
  // crope can't guarantee that its characters are contiguous, so the decoding
  // has to be done byte-by-byte.
  for (crope::const_iterator it = delta_crope.begin();
       it != delta_crope.end(); it++) {
    const char this_char = *it;
    EXPECT_TRUE(decoder_.DecodeChunk(&this_char, 1, &result_crope));
  }
  EXPECT_TRUE(decoder_.FinishDecoding());
  crope expected_target(kTarget);
  EXPECT_EQ(expected_target, result_crope);
}
#endif  // HAVE_EXT_ROPE

void VCDiffEncoderTest::TestWithFixedChunkSize(size_t chunk_size) {
  delta()->clear();
  EXPECT_TRUE(encoder_.StartEncoding(delta()));
  for (size_t chunk_start_index = 0;
       chunk_start_index < strlen(kTarget);
       chunk_start_index += chunk_size) {
    size_t this_chunk_size = chunk_size;
    const size_t bytes_available = strlen(kTarget) - chunk_start_index;
    if (this_chunk_size > bytes_available) {
      this_chunk_size = bytes_available;
    }
    EXPECT_TRUE(encoder_.EncodeChunk(&kTarget[chunk_start_index],
                                     this_chunk_size,
                                     delta()));
  }
  EXPECT_TRUE(encoder_.FinishEncoding(delta()));
  const size_t num_windows = (strlen(kTarget) / chunk_size) + 1;
  const size_t size_of_windows =
      strlen(kTarget) + (kWindowHeaderSize * num_windows);
  EXPECT_GE(kFileHeaderSize + size_of_windows, delta_size());
  result_target_.clear();
  decoder_.StartDecoding(kDictionary, sizeof(kDictionary));
  for (size_t chunk_start_index = 0;
       chunk_start_index < delta_size();
       chunk_start_index += chunk_size) {
    size_t this_chunk_size = chunk_size;
    const size_t bytes_available = delta_size() - chunk_start_index;
    if (this_chunk_size > bytes_available) {
      this_chunk_size = bytes_available;
    }
    EXPECT_TRUE(decoder_.DecodeChunk(delta_data() + chunk_start_index,
                                     this_chunk_size,
                                     &result_target_));
  }
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(kTarget, result_target_);
  LOG(INFO) << "Finished testing chunk_size = " << chunk_size << LOG_ENDL;
}

TEST_F(VCDiffEncoderTest, EncodeDecodeFixedChunkSizes) {
  // These specific chunk sizes have failed in the past
  TestWithFixedChunkSize(6);
  TestWithFixedChunkSize(45);
  TestWithFixedChunkSize(60);

  // Now loop through all possible chunk sizes
  for (size_t chunk_size = 1; chunk_size < strlen(kTarget); ++chunk_size) {
    TestWithFixedChunkSize(chunk_size);
  }
}

// Splits the text to be encoded into fixed-size chunks.  Encodes each
// chunk and puts it into a vector of strings.  Then decodes each string
// in the vector and appends the result into result_target_.
void VCDiffEncoderTest::TestWithEncodedChunkVector(size_t chunk_size) {
  std::vector<string> encoded_chunks;
  string this_encoded_chunk;
  size_t total_chunk_size = 0;
  EXPECT_TRUE(encoder_.StartEncoding(&this_encoded_chunk));
  encoded_chunks.push_back(this_encoded_chunk);
  total_chunk_size += this_encoded_chunk.size();
  for (size_t chunk_start_index = 0;
       chunk_start_index < strlen(kTarget);
       chunk_start_index += chunk_size) {
    size_t this_chunk_size = chunk_size;
    const size_t bytes_available = strlen(kTarget) - chunk_start_index;
    if (this_chunk_size > bytes_available) {
      this_chunk_size = bytes_available;
    }
    this_encoded_chunk.clear();
    EXPECT_TRUE(encoder_.EncodeChunk(&kTarget[chunk_start_index],
                                     this_chunk_size,
                                     &this_encoded_chunk));
    encoded_chunks.push_back(this_encoded_chunk);
    total_chunk_size += this_encoded_chunk.size();
  }
  this_encoded_chunk.clear();
  EXPECT_TRUE(encoder_.FinishEncoding(&this_encoded_chunk));
  encoded_chunks.push_back(this_encoded_chunk);
  total_chunk_size += this_encoded_chunk.size();
  const size_t num_windows = (strlen(kTarget) / chunk_size) + 1;
  const size_t size_of_windows =
      strlen(kTarget) + (kWindowHeaderSize * num_windows);
  EXPECT_GE(kFileHeaderSize + size_of_windows, total_chunk_size);
  result_target_.clear();
  decoder_.StartDecoding(kDictionary, sizeof(kDictionary));
  for (std::vector<string>::iterator it = encoded_chunks.begin();
       it != encoded_chunks.end(); ++it) {
    EXPECT_TRUE(decoder_.DecodeChunk(it->data(), it->size(), &result_target_));
  }
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(kTarget, result_target_);
  LOG(INFO) << "Finished testing chunk_size = " << chunk_size << LOG_ENDL;
}

TEST_F(VCDiffEncoderTest, EncodeDecodeStreamOfChunks) {
  // Loop through all possible chunk sizes
  for (size_t chunk_size = 1; chunk_size < strlen(kTarget); ++chunk_size) {
    TestWithEncodedChunkVector(chunk_size);
  }
}

// Verify that HashedDictionary stores a copy of the dictionary text,
// rather than just storing a pointer to it.  If the dictionary buffer
// is overwritten after creating a HashedDictionary from it, it shouldn't
// affect an encoder that uses that HashedDictionary.
TEST_F(VCDiffEncoderTest, DictionaryBufferOverwritten) {
  string dictionary_copy(kDictionary, sizeof(kDictionary));
  HashedDictionary hd_copy(dictionary_copy.data(), dictionary_copy.size());
  EXPECT_TRUE(hd_copy.Init());
  VCDiffStreamingEncoder copy_encoder(&hd_copy,
                                      VCD_FORMAT_INTERLEAVED
                                          | VCD_FORMAT_CHECKSUM,
                                      /* look_for_target_matches = */ true);
  // Produce a reference version of the encoded text.
  string delta_before;
  EXPECT_TRUE(copy_encoder.StartEncoding(&delta_before));
  EXPECT_TRUE(copy_encoder.EncodeChunk(kTarget,
                                       strlen(kTarget),
                                       &delta_before));
  EXPECT_TRUE(copy_encoder.FinishEncoding(&delta_before));
  EXPECT_GE(strlen(kTarget) + kFileHeaderSize + kWindowHeaderSize,
            delta_before.size());

  // Overwrite the dictionary text with all 'Q' characters.
  dictionary_copy.replace(0,
                          dictionary_copy.size(),
                          dictionary_copy.size(),
                          'Q');
  // When the encoder is used on the same target text after overwriting
  // the dictionary, it should produce the same encoded output.
  string delta_after;
  EXPECT_TRUE(copy_encoder.StartEncoding(&delta_after));
  EXPECT_TRUE(copy_encoder.EncodeChunk(kTarget, strlen(kTarget), &delta_after));
  EXPECT_TRUE(copy_encoder.FinishEncoding(&delta_after));
  EXPECT_EQ(delta_before, delta_after);
}

// Binary data test part 1: The dictionary and target data should not
// be treated as NULL-terminated.  An embedded NULL should be handled like
// any other byte of data.
TEST_F(VCDiffEncoderTest, DictionaryHasEmbeddedNULLs) {
  const char embedded_null_dictionary_text[] =
      { 0x00, 0xFF, 0xFE, 0xFD, 0x00, 0xFD, 0xFE, 0xFF, 0x00, 0x03 };
  const char embedded_null_target[] =
      { 0xFD, 0x00, 0xFD, 0xFE, 0x03, 0x00, 0x01, 0x00 };
  CHECK_EQ(10, sizeof(embedded_null_dictionary_text));
  CHECK_EQ(8, sizeof(embedded_null_target));
  HashedDictionary embedded_null_dictionary(embedded_null_dictionary_text,
      sizeof(embedded_null_dictionary_text));
  EXPECT_TRUE(embedded_null_dictionary.Init());
  VCDiffStreamingEncoder embedded_null_encoder(&embedded_null_dictionary,
      VCD_FORMAT_INTERLEAVED | VCD_FORMAT_CHECKSUM,
      /* look_for_target_matches = */ true);
  EXPECT_TRUE(embedded_null_encoder.StartEncoding(delta()));
  EXPECT_TRUE(embedded_null_encoder.EncodeChunk(embedded_null_target,
                                                sizeof(embedded_null_target),
                                                delta()));
  EXPECT_TRUE(embedded_null_encoder.FinishEncoding(delta()));
  decoder_.StartDecoding(embedded_null_dictionary_text,
                         sizeof(embedded_null_dictionary_text));
  EXPECT_TRUE(decoder_.DecodeChunk(delta_data(),
                                   delta_size(),
                                   &result_target_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(sizeof(embedded_null_target), result_target_.size());
  EXPECT_EQ(string(embedded_null_target,
                   sizeof(embedded_null_target)),
            result_target_);
}

// Binary data test part 2: An embedded CR or LF should be handled like
// any other byte of data.  No text-processing of the data should occur.
TEST_F(VCDiffEncoderTest, DictionaryHasEmbeddedNewlines) {
  const char embedded_null_dictionary_text[] =
      { 0x0C, 0xFF, 0xFE, 0x0C, 0x00, 0x0A, 0xFE, 0xFF, 0x00, 0x0A };
  const char embedded_null_target[] =
      { 0x0C, 0x00, 0x0A, 0xFE, 0x03, 0x00, 0x0A, 0x00 };
  CHECK_EQ(10, sizeof(embedded_null_dictionary_text));
  CHECK_EQ(8, sizeof(embedded_null_target));
  HashedDictionary embedded_null_dictionary(embedded_null_dictionary_text,
      sizeof(embedded_null_dictionary_text));
  EXPECT_TRUE(embedded_null_dictionary.Init());
  VCDiffStreamingEncoder embedded_null_encoder(&embedded_null_dictionary,
      VCD_FORMAT_INTERLEAVED | VCD_FORMAT_CHECKSUM,
      /* look_for_target_matches = */ true);
  EXPECT_TRUE(embedded_null_encoder.StartEncoding(delta()));
  EXPECT_TRUE(embedded_null_encoder.EncodeChunk(embedded_null_target,
                                                sizeof(embedded_null_target),
                                                delta()));
  EXPECT_TRUE(embedded_null_encoder.FinishEncoding(delta()));
  decoder_.StartDecoding(embedded_null_dictionary_text,
                         sizeof(embedded_null_dictionary_text));
  EXPECT_TRUE(decoder_.DecodeChunk(delta_data(),
                                   delta_size(),
                                   &result_target_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  EXPECT_EQ(sizeof(embedded_null_target), result_target_.size());
  EXPECT_EQ(string(embedded_null_target,
                   sizeof(embedded_null_target)),
            result_target_);
}

TEST_F(VCDiffEncoderTest, UsingWideCharacters) {
  const wchar_t wchar_dictionary_text[] =
      L"\"Just the place for a Snark!\" the Bellman cried,\n"
      L"As he landed his crew with care;\n"
      L"Supporting each man on the top of the tide\n"
      L"By a finger entwined in his hair.\n";

  const wchar_t wchar_target[] =
      L"\"Just the place for a Snark! I have said it twice:\n"
      L"That alone should encourage the crew.\n"
      L"Just the place for a Snark! I have said it thrice:\n"
      L"What I tell you three times is true.\"\n";

  HashedDictionary wchar_dictionary((const char*) wchar_dictionary_text,
                                    sizeof(wchar_dictionary_text));
  EXPECT_TRUE(wchar_dictionary.Init());
  VCDiffStreamingEncoder wchar_encoder(&wchar_dictionary,
                                       VCD_FORMAT_INTERLEAVED
                                           | VCD_FORMAT_CHECKSUM,
                                       /* look_for_target_matches = */ false);
  EXPECT_TRUE(wchar_encoder.StartEncoding(delta()));
  EXPECT_TRUE(wchar_encoder.EncodeChunk((const char*) wchar_target,
                                        sizeof(wchar_target),
                                        delta()));
  EXPECT_TRUE(wchar_encoder.FinishEncoding(delta()));
  decoder_.StartDecoding((const char*) wchar_dictionary_text,
                         sizeof(wchar_dictionary_text));
  EXPECT_TRUE(decoder_.DecodeChunk(delta_data(),
                                   delta_size(),
                                   &result_target_));
  EXPECT_TRUE(decoder_.FinishDecoding());
  const wchar_t* result_as_wchar = (const wchar_t*) result_target_.data();
  EXPECT_EQ(wcslen(wchar_target), wcslen(result_as_wchar));
  EXPECT_EQ(0, wcscmp(wchar_target, result_as_wchar));
}

#if defined(HAVE_MPROTECT) && \
   (defined(HAVE_MEMALIGN) || defined(HAVE_POSIX_MEMALIGN))
// Bug 1220602: Make sure the encoder doesn't read past the end of the input
// buffer.
TEST_F(VCDiffEncoderTest, ShouldNotReadPastEndOfBuffer) {
  const size_t target_size = strlen(kTarget);

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

  // Place the target string at the end of the first page.
  char* const target_with_guard = second_page - target_size;
  memcpy(target_with_guard, kTarget, target_size);

  // Make the second page unreadable.
  mprotect(second_page, page_size, PROT_NONE);

  // Now perform the encode operation, which will cause a segmentation fault
  // if it reads past the end of the buffer.
  EXPECT_TRUE(encoder_.StartEncoding(delta()));
  EXPECT_TRUE(encoder_.EncodeChunk(target_with_guard, target_size, delta()));
  EXPECT_TRUE(encoder_.FinishEncoding(delta()));

  // Undo the mprotect.
  mprotect(second_page, page_size, PROT_READ|PROT_WRITE);
  free(two_pages);
}

TEST_F(VCDiffEncoderTest, ShouldNotReadPastBeginningOfBuffer) {
  const size_t target_size = strlen(kTarget);

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

  // Place the target string at the beginning of the second page.
  char* const target_with_guard = second_page;
  memcpy(target_with_guard, kTarget, target_size);

  // Now perform the encode operation, which will cause a segmentation fault
  // if it reads past the beginning of the buffer.
  EXPECT_TRUE(encoder_.StartEncoding(delta()));
  EXPECT_TRUE(encoder_.EncodeChunk(target_with_guard, target_size, delta()));
  EXPECT_TRUE(encoder_.FinishEncoding(delta()));

  // Undo the mprotect.
  mprotect(first_page, page_size, PROT_READ|PROT_WRITE);
  free(two_pages);
}
#endif  // HAVE_MPROTECT && (HAVE_MEMALIGN || HAVE_POSIX_MEMALIGN)

class VCDiffMatchCountTest : public VerifyEncodedBytesTest {
 protected:
  virtual ~VCDiffMatchCountTest() { }

  void ExpectMatch(size_t match_size) {
    if (match_size >= expected_match_counts_.size()) {
      // Be generous to avoid resizing again
      expected_match_counts_.resize(match_size * 2, 0);
    }
    ++expected_match_counts_[match_size];
  }

  void VerifyMatchCounts() {
    EXPECT_TRUE(std::equal(expected_match_counts_.begin(),
                           expected_match_counts_.end(),
                           actual_match_counts_.begin()));
  }

  std::vector<int> expected_match_counts_;
  std::vector<int> actual_match_counts_;
};

class VCDiffHTML1Test : public VCDiffMatchCountTest {
 protected:
  static const char kDictionary[];
  static const char kTarget[];

  VCDiffHTML1Test();
  virtual ~VCDiffHTML1Test() { }

  void SimpleEncode();
  void StreamingEncode();

  HashedDictionary hashed_dictionary_;
  VCDiffStreamingEncoder encoder_;
  VCDiffStreamingDecoder decoder_;
  VCDiffEncoder simple_encoder_;
  VCDiffDecoder simple_decoder_;

  string result_target_;
};

const char VCDiffHTML1Test::kDictionary[] =
    "<html><font color=red>This part from the dict</font><br>";

const char VCDiffHTML1Test::kTarget[] =
    "<html><font color=red>This part from the dict</font><br>\n"
    "And this part is not...</html>";

VCDiffHTML1Test::VCDiffHTML1Test()
    : hashed_dictionary_(kDictionary, sizeof(kDictionary)),
      encoder_(&hashed_dictionary_,
               VCD_FORMAT_INTERLEAVED | VCD_FORMAT_CHECKSUM,
               /* look_for_target_matches = */ true),
      simple_encoder_(kDictionary, sizeof(kDictionary)) {
  EXPECT_TRUE(hashed_dictionary_.Init());
}

void VCDiffHTML1Test::SimpleEncode() {
  EXPECT_TRUE(simple_encoder_.Encode(kTarget, strlen(kTarget), delta()));
  EXPECT_GE(strlen(kTarget) + kFileHeaderSize + kWindowHeaderSize,
            delta_size());
  EXPECT_TRUE(simple_decoder_.Decode(kDictionary,
                                     sizeof(kDictionary),
                                     delta_as_const(),
                                     &result_target_));
  EXPECT_EQ(kTarget, result_target_);
}

void VCDiffHTML1Test::StreamingEncode() {
  EXPECT_TRUE(encoder_.StartEncoding(delta()));
  EXPECT_TRUE(encoder_.EncodeChunk(kTarget, strlen(kTarget), delta()));
  EXPECT_TRUE(encoder_.FinishEncoding(delta()));
}

TEST_F(VCDiffHTML1Test, CheckOutputOfSimpleEncoder) {
  SimpleEncode();
  // These values do not depend on the block size used for encoding
  ExpectByte(0xD6);  // 'V' | 0x80
  ExpectByte(0xC3);  // 'C' | 0x80
  ExpectByte(0xC4);  // 'D' | 0x80
  ExpectByte(0x00);  // Simple encoder never uses interleaved format
  ExpectByte(0x00);  // Hdr_Indicator
  ExpectByte(VCD_SOURCE);  // Win_Indicator: VCD_SOURCE (dictionary)
  ExpectByte(sizeof(kDictionary));  // Dictionary length
  ExpectByte(0x00);  // Source segment position: start of dictionary
  if (BlockHash::kBlockSize == 2) {
    // A very small block size will catch the "html>" match.
    ExpectByte(0x1F);  // Length of the delta encoding
    ExpectSize(strlen(kTarget));  // Size of the target window
    ExpectByte(0x00);  // Delta_indicator (no compression)
    ExpectByte(0x11);  // Length of the data section
    ExpectByte(0x06);  // Length of the instructions section
    ExpectByte(0x03);  // Length of the address section
    // Data section
    ExpectString("\nAnd t");      // Data for 1st ADD
    ExpectString("is not...</");  // Data for 2nd ADD
    // Instructions section
    ExpectByte(0x73);  // COPY size 0 mode VCD_SAME(0)
    ExpectByte(0x38);  // COPY size (56)
    ExpectByte(0x07);  // ADD size 6
    ExpectByte(0x19);  // COPY size 9 mode VCD_SELF
    ExpectByte(0x0C);  // ADD size 11
    ExpectByte(0x15);  // COPY size 5 mode VCD_SELF
    // Address section
    ExpectByte(0x00);  // COPY address (0) mode VCD_SAME(0)
    ExpectByte(0x17);  // COPY address (23) mode VCD_SELF
    ExpectByte(0x01);  // COPY address (1) mode VCD_SELF
  } else if (BlockHash::kBlockSize < 16) {
    // A medium block size will catch the "his part " match.
    ExpectByte(0x22);  // Length of the delta encoding
    ExpectSize(strlen(kTarget));  // Size of the target window
    ExpectByte(0x00);  // Delta_indicator (no compression)
    ExpectByte(0x16);  // Length of the data section
    ExpectByte(0x05);  // Length of the instructions section
    ExpectByte(0x02);  // Length of the address section
    // Data section
    ExpectString("\nAnd t");      // Data for 1st ADD
    ExpectString("is not...</html>");  // Data for 2nd ADD
    // Instructions section
    ExpectByte(0x73);  // COPY size 0 mode VCD_SAME(0)
    ExpectByte(0x38);  // COPY size (56)
    ExpectByte(0x07);  // ADD size 6
    ExpectByte(0x19);  // COPY size 9 mode VCD_SELF
    ExpectByte(0x11);  // ADD size 16
    // Address section
    ExpectByte(0x00);  // COPY address (0) mode VCD_SAME(0)
    ExpectByte(0x17);  // COPY address (23) mode VCD_SELF
  } else if (BlockHash::kBlockSize <= 56) {
    // Any block size up to 56 will catch the matching prefix string.
    ExpectByte(0x29);  // Length of the delta encoding
    ExpectSize(strlen(kTarget));  // Size of the target window
    ExpectByte(0x00);  // Delta_indicator (no compression)
    ExpectByte(0x1F);  // Length of the data section
    ExpectByte(0x04);  // Length of the instructions section
    ExpectByte(0x01);  // Length of the address section
    ExpectString("\nAnd this part is not...</html>");  // Data for ADD
    // Instructions section
    ExpectByte(0x73);  // COPY size 0 mode VCD_SAME(0)
    ExpectByte(0x38);  // COPY size (56)
    ExpectByte(0x01);  // ADD size 0
    ExpectByte(0x1F);  // Size of ADD (31)
    // Address section
    ExpectByte(0x00);  // COPY address (0) mode VCD_SAME(0)
  } else {
    // The matching string is 56 characters long, and the block size is
    // 64 or greater, so no match should be found.
    ExpectSize(strlen(kTarget) + 7);  // Delta encoding len
    ExpectSize(strlen(kTarget));  // Size of the target window
    ExpectByte(0x00);  // Delta_indicator (no compression)
    ExpectSize(strlen(kTarget));  // Length of the data section
    ExpectByte(0x02);  // Length of the instructions section
    ExpectByte(0x00);  // Length of the address section
    // Data section
    ExpectString(kTarget);
    ExpectByte(0x01);  // ADD size 0
    ExpectSize(strlen(kTarget));
  }
  ExpectNoMoreBytes();
}

TEST_F(VCDiffHTML1Test, MatchCounts) {
  StreamingEncode();
  encoder_.GetMatchCounts(&actual_match_counts_);
  if (BlockHash::kBlockSize == 2) {
    // A very small block size will catch the "html>" match.
    ExpectMatch(56);
    ExpectMatch(9);
    ExpectMatch(5);
  } else if (BlockHash::kBlockSize < 16) {
    // A medium block size will catch the "his part " match.
    ExpectMatch(56);
    ExpectMatch(9);
  } else if (BlockHash::kBlockSize <= 56) {
    // Any block size up to 56 will catch the matching prefix string.
    ExpectMatch(56);
  }
  VerifyMatchCounts();
}

#ifdef GTEST_HAS_DEATH_TEST
typedef VCDiffHTML1Test VCDiffHTML1DeathTest;

TEST_F(VCDiffHTML1DeathTest, NullMatchCounts) {
  EXPECT_DEBUG_DEATH(encoder_.GetMatchCounts(NULL), "GetMatchCounts");
}
#endif  // GTEST_HAS_DEATH_TEST

class VCDiffHTML2Test : public VCDiffMatchCountTest {
 protected:
  static const char kDictionary[];
  static const char kTarget[];

  VCDiffHTML2Test();
  virtual ~VCDiffHTML2Test() { }

  void SimpleEncode();
  void StreamingEncode();

  HashedDictionary hashed_dictionary_;
  VCDiffStreamingEncoder encoder_;
  VCDiffStreamingDecoder decoder_;
  VCDiffEncoder simple_encoder_;
  VCDiffDecoder simple_decoder_;

  string result_target_;
};

const char VCDiffHTML2Test::kDictionary[] = "10\nThis is a test";

const char VCDiffHTML2Test::kTarget[] = "This is a test!!!\n";

VCDiffHTML2Test::VCDiffHTML2Test()
    : hashed_dictionary_(kDictionary, sizeof(kDictionary)),
      encoder_(&hashed_dictionary_,
               VCD_FORMAT_INTERLEAVED | VCD_FORMAT_CHECKSUM,
               /* look_for_target_matches = */ true),
      simple_encoder_(kDictionary, sizeof(kDictionary)) {
  EXPECT_TRUE(hashed_dictionary_.Init());
}

void VCDiffHTML2Test::SimpleEncode() {
  EXPECT_TRUE(simple_encoder_.Encode(kTarget, strlen(kTarget), delta()));
  EXPECT_GE(strlen(kTarget) + kFileHeaderSize + kWindowHeaderSize,
            delta_size());
  EXPECT_TRUE(simple_decoder_.Decode(kDictionary,
                                     sizeof(kDictionary),
                                     delta_as_const(),
                                     &result_target_));
  EXPECT_EQ(kTarget, result_target_);
}

void VCDiffHTML2Test::StreamingEncode() {
  EXPECT_TRUE(encoder_.StartEncoding(delta()));
  EXPECT_TRUE(encoder_.EncodeChunk(kTarget, strlen(kTarget), delta()));
  EXPECT_GE(strlen(kTarget) + kFileHeaderSize + kWindowHeaderSize,
            delta_size());
  EXPECT_TRUE(simple_decoder_.Decode(kDictionary,
                                     sizeof(kDictionary),
                                     delta_as_const(),
                                     &result_target_));
  EXPECT_EQ(kTarget, result_target_);
}

TEST_F(VCDiffHTML2Test, VerifyOutputOfSimpleEncoder) {
  SimpleEncode();
  // These values do not depend on the block size used for encoding
  ExpectByte(0xD6);  // 'V' | 0x80
  ExpectByte(0xC3);  // 'C' | 0x80
  ExpectByte(0xC4);  // 'D' | 0x80
  ExpectByte(0x00);  // Simple encoder never uses interleaved format
  ExpectByte(0x00);  // Hdr_Indicator
  ExpectByte(VCD_SOURCE);  // Win_Indicator: VCD_SOURCE (dictionary)
  ExpectByte(sizeof(kDictionary));  // Dictionary length
  ExpectByte(0x00);  // Source segment position: start of dictionary
  if (BlockHash::kBlockSize <= 8) {
    ExpectByte(12);  // Length of the delta encoding
    ExpectSize(strlen(kTarget));  // Size of the target window
    ExpectByte(0x00);  // Delta_indicator (no compression)
    ExpectByte(0x04);  // Length of the data section
    ExpectByte(0x02);  // Length of the instructions section
    ExpectByte(0x01);  // Length of the address section
    ExpectByte('!');
    ExpectByte('!');
    ExpectByte('!');
    ExpectByte('\n');
    ExpectByte(0x1E);  // COPY size 14 mode VCD_SELF
    ExpectByte(0x05);  // ADD size 4
    ExpectByte(0x03);  // COPY address (3) mode VCD_SELF
  } else {
    // Larger block sizes will not catch any matches.
    ExpectSize(strlen(kTarget) + 7);  // Delta encoding len
    ExpectSize(strlen(kTarget));  // Size of the target window
    ExpectByte(0x00);  // Delta_indicator (no compression)
    ExpectSize(strlen(kTarget));  // Length of the data section
    ExpectByte(0x02);  // Length of the instructions section
    ExpectByte(0x00);  // Length of the address section
    // Data section
    ExpectString(kTarget);
    ExpectByte(0x01);  // ADD size 0
    ExpectSize(strlen(kTarget));
  }
  ExpectNoMoreBytes();
}

TEST_F(VCDiffHTML2Test, VerifyOutputWithChecksum) {
  StreamingEncode();
  const VCDChecksum html2_checksum = ComputeAdler32(kTarget, strlen(kTarget));
  CHECK_EQ(5, VarintBE<int64_t>::Length(html2_checksum));
  // These values do not depend on the block size used for encoding
  ExpectByte(0xD6);  // 'V' | 0x80
  ExpectByte(0xC3);  // 'C' | 0x80
  ExpectByte(0xC4);  // 'D' | 0x80
  ExpectByte('S');  // Format extensions
  ExpectByte(0x00);  // Hdr_Indicator
  ExpectByte(VCD_SOURCE | VCD_CHECKSUM);  // Win_Indicator
  ExpectByte(sizeof(kDictionary));  // Dictionary length
  ExpectByte(0x00);  // Source segment position: start of dictionary
  if (BlockHash::kBlockSize <= 8) {
    ExpectByte(17);  // Length of the delta encoding
    ExpectSize(strlen(kTarget));  // Size of the target window
    ExpectByte(0x00);  // Delta_indicator (no compression)
    ExpectByte(0x00);  // Length of the data section
    ExpectByte(0x07);  // Length of the instructions section
    ExpectByte(0x00);  // Length of the address section
    ExpectChecksum(html2_checksum);
    ExpectByte(0x1E);  // COPY size 14 mode VCD_SELF
    ExpectByte(0x03);  // COPY address (3) mode VCD_SELF
    ExpectByte(0x05);  // ADD size 4
    ExpectByte('!');
    ExpectByte('!');
    ExpectByte('!');
    ExpectByte('\n');
  } else {
    // Larger block sizes will not catch any matches.
    ExpectSize(strlen(kTarget) + 12);  // Delta encoding len
    ExpectSize(strlen(kTarget));  // Size of the target window
    ExpectByte(0x00);  // Delta_indicator (no compression)
    ExpectByte(0x00);  // Length of the data section
    ExpectSize(0x02 + strlen(kTarget));  // Interleaved
    ExpectByte(0x00);  // Length of the address section
    ExpectChecksum(html2_checksum);
    // Data section
    ExpectByte(0x01);  // ADD size 0
    ExpectSize(strlen(kTarget));
    ExpectString(kTarget);
  }
  ExpectNoMoreBytes();
}

TEST_F(VCDiffHTML2Test, MatchCounts) {
  StreamingEncode();
  encoder_.GetMatchCounts(&actual_match_counts_);
  if (BlockHash::kBlockSize <= 8) {
    ExpectMatch(14);
  }
  VerifyMatchCounts();
}

}  // anonymous namespace
}  // namespace open_vcdiff
