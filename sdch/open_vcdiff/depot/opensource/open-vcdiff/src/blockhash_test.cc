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
#include "blockhash.h"
#include <climits>  // INT_MIN
#include <memory>  // auto_ptr
#include "encodetable.h"
#include "logging.h"
#include "rolling_hash.h"
#include "testing.h"

namespace open_vcdiff {

const int kBlockSize = BlockHash::kBlockSize;

class BlockHashTest : public testing::Test {
 protected:
  static const int kTimingTestSize = 1 << 21;  // 2M
  static const int kTimingTestIterations = 32;

  BlockHashTest() {
    dh_.reset(BlockHash::CreateDictionaryHash(sample_text,
                                              strlen(sample_text)));
    th_.reset(BlockHash::CreateTargetHash(sample_text, strlen(sample_text), 0));
    EXPECT_TRUE(dh_.get() != NULL);
    EXPECT_TRUE(th_.get() != NULL);
  }

  // BlockHashTest is a friend to BlockHash.  Expose the protected functions
  // that will be tested by the children of BlockHashTest.
  static bool BlockContentsMatch(const char* block1, const char* block2) {
    return BlockHash::BlockContentsMatch(block1, block2);
  }

  int FirstMatchingBlock(const BlockHash& block_hash,
                         uint32_t hash_value,
                         const char* block_ptr) const {
    return block_hash.FirstMatchingBlock(hash_value, block_ptr);
  }

  int NextMatchingBlock(const BlockHash& block_hash,
                        int block_number,
                        const char* block_ptr) const {
    return block_hash.NextMatchingBlock(block_number, block_ptr);
  }

  static int MatchingBytesToLeft(const char* source_match_start,
                                 const char* target_match_start,
                                 int max_bytes) {
    return BlockHash::MatchingBytesToLeft(source_match_start,
                                          target_match_start,
                                          max_bytes);
  }

  static int MatchingBytesToRight(const char* source_match_end,
                                  const char* target_match_end,
                                  int max_bytes) {
    return BlockHash::MatchingBytesToRight(source_match_end,
                                           target_match_end,
                                           max_bytes);
  }

  static int StringLengthAsInt(const char* s) {
    return static_cast<int>(strlen(s));
  }

  void InitBlocksToDifferAtNthByte(int n) {
    CHECK(n < kBlockSize);
    memset(compare_buffer_1_, 0xBE, kTimingTestSize);
    memset(compare_buffer_2_, 0xBE, kTimingTestSize);
    for (int index = n; index < kTimingTestSize; index += kBlockSize) {
      compare_buffer_1_[index] = 0x00;
      compare_buffer_2_[index] = 0x01;
    }
  }

  void TestAndPrintTimesForCompareFunctions(bool should_be_identical) {
    CHECK(compare_buffer_1_ != NULL);
    CHECK(compare_buffer_2_ != NULL);
    // Prime the memory cache.
    prime_result_ =
        memcmp(compare_buffer_1_, compare_buffer_2_, kTimingTestSize);
    const char* const block1_limit =
        &compare_buffer_1_[kTimingTestSize - kBlockSize];
    int memcmp_result = 0;
    CycleTimer memcmp_timer;
    memcmp_timer.Start();
    for (int i = 0; i < kTimingTestIterations; ++i) {
      const char* block1 = compare_buffer_1_;
      const char* block2 = compare_buffer_2_;
      while (block1 < block1_limit) {
        if (memcmp(block1, block2, kBlockSize) != 0) {
          ++memcmp_result;
        }
        block1 += kBlockSize;
        block2 += kBlockSize;
      }
    }
    memcmp_timer.Stop();
    double time_for_memcmp = static_cast<double>(memcmp_timer.GetInUsec())
        / (kTimingTestSize * kTimingTestIterations);
    int block_contents_match_result = 0;
    CycleTimer block_contents_match_timer;
    block_contents_match_timer.Start();
    for (int i = 0; i < kTimingTestIterations; ++i) {
      const char* block1 = compare_buffer_1_;
      const char* block2 = compare_buffer_2_;
      while (block1 < block1_limit) {
        if (!BlockHash::BlockContentsMatch(block1, block2)) {
          ++block_contents_match_result;
        }
        block1 += kBlockSize;
        block2 += kBlockSize;
      }
    }
    block_contents_match_timer.Stop();
    double time_for_block_contents_match =
        static_cast<double>(block_contents_match_timer.GetInUsec())
        / (kTimingTestSize * kTimingTestIterations);
    EXPECT_EQ(block_contents_match_result, memcmp_result);
    if (should_be_identical) {
      CHECK_EQ(0, memcmp_result);
    } else {
      CHECK_GT(memcmp_result, 0);
    }
    LOG(INFO) << "memcmp: " << time_for_memcmp << " us per operation"
              << LOG_ENDL;
    LOG(INFO) << "BlockHash::BlockContentsMatch: "
              << time_for_block_contents_match << " us per operation"
              << LOG_ENDL;
    if (time_for_memcmp > 0) {
      LOG(INFO) << "% change: "
                << (((time_for_block_contents_match - time_for_memcmp)
                       / time_for_memcmp) * 100.0) << "%" << LOG_ENDL;
    }
#ifdef NDEBUG
    // Only check timings for optimized build
    const double error_margin = 0.05;
    EXPECT_GT(time_for_memcmp * (1.0 + error_margin),
              time_for_block_contents_match);
#endif  // NDEBUG
  }

  void TimingTestForBlocksThatDifferAtByte(int n) {
    InitBlocksToDifferAtNthByte(n);
    LOG(INFO) << "Comparing blocks that differ at byte " << n << LOG_ENDL;
    TestAndPrintTimesForCompareFunctions(false);
  }

  // Copy sample_text_without_spaces and search_string_without_spaces
  // into newly allocated sample_text and search_string buffers,
  // but pad them with space characters so that every character
  // in sample_text_without_spaces matches (kBlockSize - 1)
  // space characters in sample_text, followed by that character.
  // For example:
  // Since sample_text_without_spaces begins "The only thing"...,
  // if kBlockSize is 4, then 3 space characters will be inserted
  // between each letter of sample_text, as follows:
  // "   T   h   e       o   n   l   y       t   h   i   n   g"...
  // This makes testing simpler, because finding a kBlockSize-byte match
  // between the sample text and search string only depends on the
  // trailing letter in each block.
  static void MakeEachLetterABlock(const char* string_without_spaces,
                                   const char** result) {
    const size_t length_without_spaces = strlen(string_without_spaces);
    char* padded_text = new char[(kBlockSize * length_without_spaces) + 1];
    memset(padded_text, ' ', kBlockSize * length_without_spaces);
    char* padded_text_ptr = padded_text + (kBlockSize - 1);
    for (size_t i = 0; i < length_without_spaces; ++i) {
      *padded_text_ptr = string_without_spaces[i];
      padded_text_ptr += kBlockSize;
    }
    padded_text[kBlockSize * length_without_spaces] = '\0';
    *result = padded_text;
  }

  static void SetUpTestCase() {
    MakeEachLetterABlock(sample_text_without_spaces, &sample_text);
    MakeEachLetterABlock(search_string_without_spaces, &search_string);
    MakeEachLetterABlock(search_string_altered_without_spaces,
                         &search_string_altered);
    MakeEachLetterABlock(search_to_end_without_spaces, &search_to_end_string);
    MakeEachLetterABlock(search_to_beginning_without_spaces,
                         &search_to_beginning_string);
    MakeEachLetterABlock(sample_text_many_matches_without_spaces,
                         &sample_text_many_matches);
    MakeEachLetterABlock(search_string_many_matches_without_spaces,
                         &search_string_many_matches);
    MakeEachLetterABlock("y", &test_string_y);
    MakeEachLetterABlock("e", &test_string_e);
    char* new_test_string_unaligned_e = new char[kBlockSize];
    memset(new_test_string_unaligned_e, ' ', kBlockSize);
    new_test_string_unaligned_e[kBlockSize - 2] = 'e';
    test_string_unaligned_e = new_test_string_unaligned_e;
    char* new_test_string_all_Qs = new char[kBlockSize];
    memset(new_test_string_all_Qs, 'Q', kBlockSize);
    test_string_all_Qs = new_test_string_all_Qs;
    hashed_y = RollingHash<kBlockSize>::Hash(test_string_y);
    hashed_e = RollingHash<kBlockSize>::Hash(test_string_e);
    hashed_f =
        RollingHash<kBlockSize>::Hash(&search_string[index_of_f_in_fearsome]);
    hashed_unaligned_e = RollingHash<kBlockSize>::Hash(test_string_unaligned_e);
    hashed_all_Qs = RollingHash<kBlockSize>::Hash(test_string_all_Qs);
  }

  static void TearDownTestCase() {
    delete[] sample_text;
    delete[] search_string;
    delete[] search_string_altered;
    delete[] search_to_end_string;
    delete[] search_to_beginning_string;
    delete[] sample_text_many_matches;
    delete[] search_string_many_matches;
    delete[] test_string_y;
    delete[] test_string_e;
    delete[] test_string_unaligned_e;
    delete[] test_string_all_Qs;
  }

  // Each block in the sample text and search string is kBlockSize bytes long,
  // and consists of (kBlockSize - 1) space characters
  // followed by a single letter of text.

  // Block numbers of certain characters within the sample text:
  // All six occurrences of "e", in order.
  static const int block_of_first_e = 2;
  static const int block_of_second_e = 16;
  static const int block_of_third_e = 21;
  static const int block_of_fourth_e = 27;
  static const int block_of_fifth_e = 35;
  static const int block_of_sixth_e = 42;

  static const int block_of_y_in_only = 7;
  // The block number is multiplied by kBlockSize to arrive at the
  // index, which points to the (kBlockSize - 1) space characters before
  // the letter specified.
  // Indices of certain characters within the sample text.
  static const int index_of_first_e = block_of_first_e * kBlockSize;
  static const int index_of_fourth_e = block_of_fourth_e * kBlockSize;
  static const int index_of_sixth_e = block_of_sixth_e * kBlockSize;
  static const int index_of_y_in_only = block_of_y_in_only * kBlockSize;
  static const int index_of_space_before_fear_is_fear = 25 * kBlockSize;
  static const int index_of_longest_match_ear_is_fear = 27 * kBlockSize;
  static const int index_of_i_in_fear_is_fear = 31 * kBlockSize;
  static const int index_of_space_before_fear_itself = 33 * kBlockSize;
  static const int index_of_space_before_itself = 38 * kBlockSize;
  static const int index_of_ababc = 4 * kBlockSize;

  // Indices of certain characters within the search strings.
  static const int index_of_second_w_in_what_we = 5 * kBlockSize;
  static const int index_of_second_e_in_what_we_hear = 9 * kBlockSize;
  static const int index_of_f_in_fearsome = 16 * kBlockSize;
  static const int index_of_space_in_eat_itself =  12 * kBlockSize;
  static const int index_of_i_in_itself = 13 * kBlockSize;
  static const int index_of_t_in_use_the = 4 * kBlockSize;
  static const int index_of_o_in_online = 8 * kBlockSize;

  static const char sample_text_without_spaces[];
  static const char search_string_without_spaces[];
  static const char search_string_altered_without_spaces[];
  static const char search_to_end_without_spaces[];
  static const char search_to_beginning_without_spaces[];
  static const char sample_text_many_matches_without_spaces[];
  static const char search_string_many_matches_without_spaces[];

  static const char* sample_text;
  static const char* search_string;
  static const char* search_string_altered;
  static const char* search_to_end_string;
  static const char* search_to_beginning_string;
  static const char* sample_text_many_matches;
  static const char* search_string_many_matches;

  static const char* test_string_y;
  static const char* test_string_e;
  static const char* test_string_all_Qs;
  static const char* test_string_unaligned_e;

  static uint32_t hashed_y;
  static uint32_t hashed_e;
  static uint32_t hashed_f;
  static uint32_t hashed_unaligned_e;
  static uint32_t hashed_all_Qs;

  // Boost scoped_ptr, if available, could be used instead of std::auto_ptr.
  std::auto_ptr<const BlockHash> dh_;  // hash table is populated at startup
  std::auto_ptr<BlockHash> th_;  // hash table not populated;
                                // used to test incremental adds

  BlockHash::Match best_match_;
  char* compare_buffer_1_;
  char* compare_buffer_2_;
  int prime_result_;
};

#ifdef GTEST_HAS_DEATH_TEST
typedef BlockHashTest BlockHashDeathTest;
#endif  // GTEST_HAS_DEATH_TEST

// The C++ standard requires a separate definition of these static const values,
// even though their initializers are given within the class definition.
const int BlockHashTest::block_of_first_e;
const int BlockHashTest::block_of_second_e;
const int BlockHashTest::block_of_third_e;
const int BlockHashTest::block_of_fourth_e;
const int BlockHashTest::block_of_fifth_e;
const int BlockHashTest::block_of_sixth_e;
const int BlockHashTest::block_of_y_in_only;
const int BlockHashTest::index_of_first_e;
const int BlockHashTest::index_of_fourth_e;
const int BlockHashTest::index_of_sixth_e;
const int BlockHashTest::index_of_y_in_only;
const int BlockHashTest::index_of_space_before_fear_is_fear;
const int BlockHashTest::index_of_longest_match_ear_is_fear;
const int BlockHashTest::index_of_i_in_fear_is_fear;
const int BlockHashTest::index_of_space_before_fear_itself;
const int BlockHashTest::index_of_space_before_itself;
const int BlockHashTest::index_of_ababc;
const int BlockHashTest::index_of_second_w_in_what_we;
const int BlockHashTest::index_of_second_e_in_what_we_hear;
const int BlockHashTest::index_of_f_in_fearsome;
const int BlockHashTest::index_of_space_in_eat_itself;
const int BlockHashTest::index_of_i_in_itself;
const int BlockHashTest::index_of_t_in_use_the;
const int BlockHashTest::index_of_o_in_online;

const char BlockHashTest::sample_text_without_spaces[] =
    "The only thing we have to fear is fear itself";

const char BlockHashTest::search_string_without_spaces[] =
    "What we hear is fearsome";

const char BlockHashTest::search_string_altered_without_spaces[] =
    "Vhat ve hear is fearsomm";

const char BlockHashTest::search_to_end_without_spaces[] =
    "Pop will eat itself, eventually";

const char BlockHashTest::search_to_beginning_without_spaces[] =
    "Use The online dictionary";

const char BlockHashTest::sample_text_many_matches_without_spaces[] =
    "ababababcab";

const char BlockHashTest::search_string_many_matches_without_spaces[] =
    "ababc";

const char* BlockHashTest::sample_text = NULL;
const char* BlockHashTest::search_string = NULL;
const char* BlockHashTest::search_string_altered = NULL;
const char* BlockHashTest::search_to_end_string = NULL;
const char* BlockHashTest::search_to_beginning_string = NULL;
const char* BlockHashTest::sample_text_many_matches = NULL;
const char* BlockHashTest::search_string_many_matches = NULL;

const char* BlockHashTest::test_string_y = NULL;
const char* BlockHashTest::test_string_e = NULL;
const char* BlockHashTest::test_string_unaligned_e = NULL;
const char* BlockHashTest::test_string_all_Qs = NULL;

uint32_t BlockHashTest::hashed_y = 0;
uint32_t BlockHashTest::hashed_e = 0;
uint32_t BlockHashTest::hashed_f = 0;
uint32_t BlockHashTest::hashed_unaligned_e = 0;
uint32_t BlockHashTest::hashed_all_Qs = 0;

// The two strings passed to BlockHash::MatchingBytesToLeft do have matching
// characters -- in fact, they're the same string -- but since max_bytes is zero
// or negative, BlockHash::MatchingBytesToLeft should not read from the strings
// and should return 0.
TEST_F(BlockHashTest, MaxBytesZeroDoesNothing) {
  EXPECT_EQ(0, MatchingBytesToLeft(
      &search_string[index_of_f_in_fearsome],
      &search_string[index_of_f_in_fearsome],
      0));
  EXPECT_EQ(0, MatchingBytesToRight(
      &search_string[index_of_f_in_fearsome],
      &search_string[index_of_f_in_fearsome],
      0));
}

TEST_F(BlockHashTest, MaxBytesNegativeDoesNothing) {
  EXPECT_EQ(0, MatchingBytesToLeft(
      &search_string[index_of_f_in_fearsome],
      &search_string[index_of_f_in_fearsome],
      -1));
  EXPECT_EQ(0, MatchingBytesToLeft(
      &search_string[index_of_f_in_fearsome],
      &search_string[index_of_f_in_fearsome],
      INT_MIN));
  EXPECT_EQ(0, MatchingBytesToRight(
      &search_string[index_of_f_in_fearsome],
      &search_string[index_of_f_in_fearsome],
      -1));
  EXPECT_EQ(0, MatchingBytesToRight(
      &search_string[index_of_f_in_fearsome],
      &search_string[index_of_f_in_fearsome],
      INT_MIN));
}

TEST_F(BlockHashTest, MaxBytesOneMatch) {
  EXPECT_EQ(1, MatchingBytesToLeft(
      &search_string[index_of_f_in_fearsome],
      &search_string[index_of_f_in_fearsome],
      1));
  EXPECT_EQ(1, MatchingBytesToRight(
      &search_string[index_of_f_in_fearsome],
      &search_string[index_of_f_in_fearsome],
      1));
}

TEST_F(BlockHashTest, MaxBytesOneNoMatch) {
  EXPECT_EQ(0, MatchingBytesToLeft(
      &search_string[index_of_f_in_fearsome],
      &search_string[index_of_second_e_in_what_we_hear],
      1));
  EXPECT_EQ(0, MatchingBytesToRight(
      &search_string[index_of_f_in_fearsome],
      &search_string[index_of_second_e_in_what_we_hear - 1],
      1));
}

TEST_F(BlockHashTest, LeftLimitedByMaxBytes) {
  // The number of bytes that match between the original "we hear is fearsome"
  // and the altered "ve hear is fearsome".
  const int expected_length = kBlockSize * StringLengthAsInt("e hear is ");
  const int max_bytes = expected_length - 1;
  EXPECT_EQ(max_bytes, MatchingBytesToLeft(
      &search_string[index_of_f_in_fearsome],
      &search_string_altered[index_of_f_in_fearsome],
      max_bytes));
}

TEST_F(BlockHashTest, LeftNotLimited) {
  // The number of bytes that match between the original "we hear is fearsome"
  // and the altered "ve hear is fearsome".
  const int expected_length = kBlockSize * StringLengthAsInt("e hear is ");
  const int max_bytes = expected_length + 1;
  EXPECT_EQ(expected_length, MatchingBytesToLeft(
      &search_string[index_of_f_in_fearsome],
      &search_string_altered[index_of_f_in_fearsome],
      max_bytes));
  EXPECT_EQ(expected_length, MatchingBytesToLeft(
      &search_string[index_of_f_in_fearsome],
      &search_string_altered[index_of_f_in_fearsome],
      INT_MAX));
}

TEST_F(BlockHashTest, RightLimitedByMaxBytes) {
  // The number of bytes that match between the original "fearsome"
  // and the altered "fearsomm".
  const int expected_length = (kBlockSize * StringLengthAsInt("fearsom"))
                              + (kBlockSize - 1);  // spacing between letters
  const int max_bytes = expected_length - 1;
  EXPECT_EQ(max_bytes, MatchingBytesToRight(
      &search_string[index_of_f_in_fearsome],
      &search_string_altered[index_of_f_in_fearsome],
      max_bytes));
}

TEST_F(BlockHashTest, RightNotLimited) {
  // The number of bytes that match between the original "we hear is fearsome"
  // and the altered "ve hear is fearsome".
  const int expected_length = (kBlockSize * StringLengthAsInt("fearsom"))
                              + (kBlockSize - 1);  // spacing between letters
  const int max_bytes = expected_length + 1;
  EXPECT_EQ(expected_length, MatchingBytesToRight(
      &search_string[index_of_f_in_fearsome],
      &search_string_altered[index_of_f_in_fearsome],
      max_bytes));
  EXPECT_EQ(expected_length, MatchingBytesToRight(
      &search_string[index_of_f_in_fearsome],
      &search_string_altered[index_of_f_in_fearsome],
      INT_MAX));
}

TEST_F(BlockHashTest, BlockContentsMatchIsFasterThanMemcmp) {
  compare_buffer_1_ = new char[kTimingTestSize];
  compare_buffer_2_ = new char[kTimingTestSize];

  // The value 0xBE is arbitrarily chosen.  First test with identical contents
  // in the buffers, so that the comparison functions cannot short-circuit
  // and will return true.
  memset(compare_buffer_1_, 0xBE, kTimingTestSize);
  memset(compare_buffer_2_, 0xBE, kTimingTestSize);
  LOG(INFO) << "Comparing " << kTimingTestSize << " identical values:"
            << LOG_ENDL;
  TestAndPrintTimesForCompareFunctions(true);

  // Now change one value in the middle of one buffer, so that the contents
  // are no longer the same.
  compare_buffer_1_[kTimingTestSize / 2] = 0x00;
  LOG(INFO) << "Comparing " << (kTimingTestSize - 1) << " identical values"
            << " and one mismatch:" << LOG_ENDL;
  TestAndPrintTimesForCompareFunctions(false);

  // Set one of the bytes of each block to differ so that
  // none of the compare operations will return true, and run timing tests.
  // In practice, BlockHash::BlockContentsMatch will only be called
  // for two blocks whose hash values match, and the two most important
  // cases are: (1) the blocks are identical, or (2) none of their bytes match.
  TimingTestForBlocksThatDifferAtByte(0);
  TimingTestForBlocksThatDifferAtByte(1);
  TimingTestForBlocksThatDifferAtByte(kBlockSize / 2);
  TimingTestForBlocksThatDifferAtByte(kBlockSize - 1);

  delete[] compare_buffer_1_;
  delete[] compare_buffer_2_;
}

TEST_F(BlockHashTest, FindFailsBeforeHashing) {
  EXPECT_EQ(-1, FirstMatchingBlock(*th_, hashed_y, test_string_y));
}

TEST_F(BlockHashTest, HashOneFindOne) {
  for (int i = 0; i <= index_of_y_in_only; ++i) {
    th_->AddOneIndexHash(i, RollingHash<kBlockSize>::Hash(&sample_text[i]));
  }
  EXPECT_EQ(block_of_y_in_only, FirstMatchingBlock(*th_, hashed_y,
                                                   test_string_y));
  EXPECT_EQ(-1, NextMatchingBlock(*th_, block_of_y_in_only, test_string_y));
}

TEST_F(BlockHashTest, HashAllFindOne) {
  EXPECT_EQ(block_of_y_in_only, FirstMatchingBlock(*dh_, hashed_y,
                                                   test_string_y));
  EXPECT_EQ(-1, NextMatchingBlock(*dh_, block_of_y_in_only, test_string_y));
}

TEST_F(BlockHashTest, NonMatchingTextNotFound) {
  EXPECT_EQ(-1, FirstMatchingBlock(*dh_, hashed_all_Qs, test_string_all_Qs));
}

// Search for unaligned text.  The test string is contained in the
// sample text (unlike the non-matching string in NonMatchingTextNotFound,
// above), but it is not aligned on a block boundary.  FindMatchingBlock
// will only work if the test string is aligned on a block boundary.
//
//    "   T   h   e       o   n   l   y"
//              ^^^^ Here is the test string
//
TEST_F(BlockHashTest, UnalignedTextNotFound) {
  EXPECT_EQ(-1, FirstMatchingBlock(*dh_, hashed_unaligned_e,
                                   test_string_unaligned_e));
}

TEST_F(BlockHashTest, FindSixMatches) {
  EXPECT_EQ(block_of_first_e, FirstMatchingBlock(*dh_, hashed_e,
                                                 test_string_e));
  EXPECT_EQ(block_of_second_e, NextMatchingBlock(*dh_, block_of_first_e,
                                                 test_string_e));
  EXPECT_EQ(block_of_third_e, NextMatchingBlock(*dh_, block_of_second_e,
                                                test_string_e));
  EXPECT_EQ(block_of_fourth_e, NextMatchingBlock(*dh_, block_of_third_e,
                                                 test_string_e));
  EXPECT_EQ(block_of_fifth_e, NextMatchingBlock(*dh_, block_of_fourth_e,
                                                test_string_e));
  EXPECT_EQ(block_of_sixth_e, NextMatchingBlock(*dh_, block_of_fifth_e,
                                                test_string_e));
  EXPECT_EQ(-1, NextMatchingBlock(*dh_, block_of_sixth_e, test_string_e));

  // Starting over gives same result
  EXPECT_EQ(block_of_first_e, FirstMatchingBlock(*dh_, hashed_e,
                                                 test_string_e));
}

TEST_F(BlockHashTest, AddRangeFindThreeMatches) {
  // Add hash values only for those characters before the fourth instance
  // of "e" in the sample text.  Tests that the ending index
  // of AddAllBlocksThroughIndex() is not inclusive: only three matches
  // for "e" should be found.
  th_->AddAllBlocksThroughIndex(index_of_fourth_e);
  EXPECT_EQ(block_of_first_e, FirstMatchingBlock(*th_, hashed_e,
                                                 test_string_e));
  EXPECT_EQ(block_of_second_e, NextMatchingBlock(*th_, block_of_first_e,
                                                 test_string_e));
  EXPECT_EQ(block_of_third_e, NextMatchingBlock(*th_, block_of_second_e,
                                                test_string_e));
  EXPECT_EQ(-1, NextMatchingBlock(*th_, block_of_third_e, test_string_e));

  // Starting over gives same result
  EXPECT_EQ(block_of_first_e, FirstMatchingBlock(*th_, hashed_e,
                                                 test_string_e));
}

// Try indices that are not even multiples of the block size.
// Add three ranges and verify the results after each
// call to AddAllBlocksThroughIndex().
TEST_F(BlockHashTest, AddRangeWithUnalignedIndices) {
  th_->AddAllBlocksThroughIndex(index_of_first_e + 1);
  EXPECT_EQ(block_of_first_e, FirstMatchingBlock(*th_, hashed_e,
                                                 test_string_e));
  EXPECT_EQ(-1, NextMatchingBlock(*th_, block_of_first_e, test_string_e));

  // Starting over gives same result
  EXPECT_EQ(block_of_first_e, FirstMatchingBlock(*th_, hashed_e,
                                                 test_string_e));

  // Add the second range to expand the result set
  th_->AddAllBlocksThroughIndex(index_of_fourth_e - 3);
  EXPECT_EQ(block_of_first_e, FirstMatchingBlock(*th_, hashed_e,
                                                 test_string_e));
  EXPECT_EQ(block_of_second_e, NextMatchingBlock(*th_, block_of_first_e,
                                                 test_string_e));
  EXPECT_EQ(block_of_third_e, NextMatchingBlock(*th_, block_of_second_e,
                                                test_string_e));
  EXPECT_EQ(-1, NextMatchingBlock(*th_, block_of_third_e, test_string_e));

  // Starting over gives same result
  EXPECT_EQ(block_of_first_e, FirstMatchingBlock(*th_, hashed_e,
                                                 test_string_e));

  // Add the third range to expand the result set
  th_->AddAllBlocksThroughIndex(index_of_fourth_e + 1);

  EXPECT_EQ(block_of_first_e, FirstMatchingBlock(*th_, hashed_e,
                                                 test_string_e));
  EXPECT_EQ(block_of_second_e, NextMatchingBlock(*th_, block_of_first_e,
                                                 test_string_e));
  EXPECT_EQ(block_of_third_e, NextMatchingBlock(*th_, block_of_second_e,
                                                test_string_e));
  EXPECT_EQ(block_of_fourth_e, NextMatchingBlock(*th_, block_of_third_e,
                                                 test_string_e));
  EXPECT_EQ(-1, NextMatchingBlock(*th_, block_of_fourth_e, test_string_e));

  // Starting over gives same result
  EXPECT_EQ(block_of_first_e, FirstMatchingBlock(*th_, hashed_e,
                                                 test_string_e));
}

#ifdef GTEST_HAS_DEATH_TEST
TEST_F(BlockHashDeathTest, AddingRangesInDescendingOrderNoEffect) {
  th_->AddAllBlocksThroughIndex(index_of_fourth_e + 1);

  EXPECT_EQ(block_of_first_e, FirstMatchingBlock(*th_, hashed_e,
                                                 test_string_e));
  EXPECT_EQ(block_of_second_e, NextMatchingBlock(*th_, block_of_first_e,
                                                 test_string_e));
  EXPECT_EQ(block_of_third_e, NextMatchingBlock(*th_, block_of_second_e,
                                                test_string_e));
  EXPECT_EQ(block_of_fourth_e, NextMatchingBlock(*th_, block_of_third_e,
                                                 test_string_e));
  EXPECT_EQ(-1, NextMatchingBlock(*th_, block_of_fourth_e, test_string_e));

  // Starting over gives same result
  EXPECT_EQ(block_of_first_e, FirstMatchingBlock(*th_, hashed_e,
                                                 test_string_e));

  // These calls will produce DFATAL error messages, and will do nothing,
  // since the ranges have already been added.
  EXPECT_DEBUG_DEATH(th_->AddAllBlocksThroughIndex(index_of_fourth_e - 3),
                     "<");
  EXPECT_DEBUG_DEATH(th_->AddAllBlocksThroughIndex(index_of_first_e + 1),
                     "<");

  EXPECT_EQ(block_of_first_e, FirstMatchingBlock(*th_, hashed_e,
                                                 test_string_e));
  EXPECT_EQ(block_of_second_e, NextMatchingBlock(*th_, block_of_first_e,
                                                 test_string_e));
  EXPECT_EQ(block_of_third_e, NextMatchingBlock(*th_, block_of_second_e,
                                                test_string_e));
  EXPECT_EQ(block_of_fourth_e, NextMatchingBlock(*th_, block_of_third_e,
                                                 test_string_e));
  EXPECT_EQ(-1, NextMatchingBlock(*th_, block_of_fourth_e, test_string_e));

  // Starting over gives same result
  EXPECT_EQ(block_of_first_e, FirstMatchingBlock(*th_, hashed_e,
                                                 test_string_e));
}
#endif  // GTEST_HAS_DEATH_TEST

TEST_F(BlockHashTest, AddEntireRangeFindSixMatches) {
  th_->AddAllBlocksThroughIndex(StringLengthAsInt(sample_text));
  EXPECT_EQ(block_of_first_e, FirstMatchingBlock(*th_, hashed_e,
                                                 test_string_e));
  EXPECT_EQ(block_of_second_e, NextMatchingBlock(*th_, block_of_first_e,
                                                 test_string_e));
  EXPECT_EQ(block_of_third_e, NextMatchingBlock(*th_, block_of_second_e,
                                                test_string_e));
  EXPECT_EQ(block_of_fourth_e, NextMatchingBlock(*th_, block_of_third_e,
                                                 test_string_e));
  EXPECT_EQ(block_of_fifth_e, NextMatchingBlock(*th_, block_of_fourth_e,
                                                test_string_e));
  EXPECT_EQ(block_of_sixth_e, NextMatchingBlock(*th_, block_of_fifth_e,
                                                test_string_e));
  EXPECT_EQ(-1, NextMatchingBlock(*th_, block_of_sixth_e, test_string_e));

  // Starting over gives same result
  EXPECT_EQ(block_of_first_e, FirstMatchingBlock(*th_, hashed_e,
                                                 test_string_e));
}

TEST_F(BlockHashTest, ZeroSizeSourceAccepted) {
  BlockHash zero_sized_hash(sample_text, 0, 0);
  EXPECT_EQ(true, zero_sized_hash.Init(true));
  EXPECT_EQ(-1, FirstMatchingBlock(*th_, hashed_y, test_string_y));
}

#ifdef GTEST_HAS_DEATH_TEST
TEST_F(BlockHashDeathTest, BadNextMatchingBlockReturnsNoMatch) {
  EXPECT_DEBUG_DEATH(EXPECT_EQ(-1, NextMatchingBlock(*dh_, 0xFFFFFFFE, "    ")),
                     "invalid");
}
#endif  // GTEST_HAS_DEATH_TEST

TEST_F(BlockHashTest, UnknownFingerprintReturnsNoMatch) {
  EXPECT_EQ(-1, FirstMatchingBlock(*dh_, 0xFAFAFAFA, "FAFA"));
}

TEST_F(BlockHashTest, FindBestMatch) {
  dh_->FindBestMatch(hashed_f,
                     &search_string[index_of_f_in_fearsome],
                     search_string,
                     strlen(search_string),
                     &best_match_);
  EXPECT_EQ(index_of_longest_match_ear_is_fear, best_match_.source_offset());
  EXPECT_EQ(index_of_second_e_in_what_we_hear, best_match_.target_offset());
  // The match includes the spaces after the final character,
  // which is why (kBlockSize - 1) is added to the expected best size.
  EXPECT_EQ((strlen("ear is fear") * kBlockSize) + (kBlockSize - 1),
            best_match_.size());
}

TEST_F(BlockHashTest, FindBestMatchWithStartingOffset) {
  BlockHash th2(sample_text, strlen(sample_text), 0x10000);
  th2.Init(true);  // hash all blocks
  th2.FindBestMatch(hashed_f,
                    &search_string[index_of_f_in_fearsome],
                    search_string,
                    strlen(search_string),
                    &best_match_);
  // Offset should begin with dictionary_size
  EXPECT_EQ(0x10000 + (index_of_longest_match_ear_is_fear),
            best_match_.source_offset());
  EXPECT_EQ(index_of_second_e_in_what_we_hear, best_match_.target_offset());
  // The match includes the spaces after the final character,
  // which is why (kBlockSize - 1) is added to the expected best size.
  EXPECT_EQ((strlen("ear is fear") * kBlockSize) + (kBlockSize - 1),
            best_match_.size());
}

TEST_F(BlockHashTest, BestMatchReachesEndOfDictionary) {
  // Hash the "i" in "fear itself"
  uint32_t hash_value = RollingHash<kBlockSize>::Hash(
      &search_to_end_string[index_of_i_in_itself]);
  dh_->FindBestMatch(hash_value,
                     &search_to_end_string[index_of_i_in_itself],
                     search_to_end_string,
                     strlen(search_to_end_string),
                     &best_match_);
  EXPECT_EQ(index_of_space_before_itself, best_match_.source_offset());
  EXPECT_EQ(index_of_space_in_eat_itself, best_match_.target_offset());
  EXPECT_EQ(strlen(" itself") * kBlockSize, best_match_.size());
}

TEST_F(BlockHashTest, BestMatchReachesStartOfDictionary) {
  // Hash the "i" in "fear itself"
  uint32_t hash_value = RollingHash<kBlockSize>::Hash(
      &search_to_beginning_string[index_of_o_in_online]);
  dh_->FindBestMatch(hash_value,
                     &search_to_beginning_string[index_of_o_in_online],
                     search_to_beginning_string,
                     strlen(search_to_beginning_string),
                     &best_match_);
  EXPECT_EQ(0, best_match_.source_offset());  // beginning of dictionary
  EXPECT_EQ(index_of_t_in_use_the, best_match_.target_offset());
  // The match includes the spaces after the final character,
  // which is why (kBlockSize - 1) is added to the expected best size.
  EXPECT_EQ((strlen("The onl") * kBlockSize) + (kBlockSize - 1),
            best_match_.size());
}

TEST_F(BlockHashTest, BestMatchWithManyMatches) {
  BlockHash many_matches_hash(sample_text_many_matches,
                              strlen(sample_text_many_matches),
                              0);
  EXPECT_TRUE(many_matches_hash.Init(true));
  // Hash the "   a" at the beginning of the search string "ababc"
  uint32_t hash_value =
      RollingHash<kBlockSize>::Hash(search_string_many_matches);
  many_matches_hash.FindBestMatch(hash_value,
                                  search_string_many_matches,
                                  search_string_many_matches,
                                  strlen(search_string_many_matches),
                                  &best_match_);
  EXPECT_EQ(index_of_ababc, best_match_.source_offset());
  EXPECT_EQ(0, best_match_.target_offset());
  EXPECT_EQ(strlen(search_string_many_matches), best_match_.size());
}

TEST_F(BlockHashTest, HashCollisionFindsNoMatch) {
  char* collision_search_string = new char[strlen(search_string) + 1];
  memcpy(collision_search_string, search_string, strlen(search_string) + 1);
  char* fearsome_location = &collision_search_string[index_of_f_in_fearsome];

  // Tweak the collision string so that it has the same hash value
  // but different text.  The last four characters of the search string
  // should be "   f", and the bytes given below have the same hash value
  // as those characters.
  CHECK_GE(kBlockSize, 4);
  fearsome_location[kBlockSize - 4] = 0x84;
  fearsome_location[kBlockSize - 3] = 0xF1;
  fearsome_location[kBlockSize - 2] = 0x51;
  fearsome_location[kBlockSize - 1] = 0x00;
  EXPECT_EQ(hashed_f, RollingHash<kBlockSize>::Hash(fearsome_location));
  EXPECT_NE(0, memcmp(&search_string[index_of_f_in_fearsome],
                      fearsome_location,
                      kBlockSize));
  // No match should be found this time.
  dh_->FindBestMatch(hashed_f,
      fearsome_location,
      collision_search_string,
      strlen(search_string),  // since collision_search_string has embedded \0
      &best_match_);
  EXPECT_EQ(-1, best_match_.source_offset());
  EXPECT_EQ(-1, best_match_.target_offset());
  EXPECT_EQ(0U, best_match_.size());
  delete[] collision_search_string;
}

// If the footprint passed to FindBestMatch does not actually match
// the search string, it should not find any matches.
TEST_F(BlockHashTest, WrongFootprintFindsNoMatch) {
  dh_->FindBestMatch(hashed_e,  // Using hashed value of "e" instead of "f"!
                     &search_string[index_of_f_in_fearsome],
                     search_string,
                     strlen(search_string),
                     &best_match_);
  EXPECT_EQ(-1, best_match_.source_offset());
  EXPECT_EQ(-1, best_match_.target_offset());
  EXPECT_EQ(0U, best_match_.size());
}

// Use a dictionary containing 1M copies of the letter 'Q',
// and target data that also contains 1M Qs.  If FindBestMatch
// is not throttled to find a maximum number of matches, this
// will take a very long time -- several seconds at least.
// If this test appears to hang, it is because the throttling code
// (see BlockHash::kMaxMatchesToCheck for details) is not working.
TEST_F(BlockHashTest, SearchStringFindsTooManyMatches) {
  const int kTestSize = 1 << 20;  // 1M
  char* huge_dictionary = new char[kTestSize];
  memset(huge_dictionary, 'Q', kTestSize);
  BlockHash huge_bh(huge_dictionary, kTestSize, 0);
  EXPECT_TRUE(huge_bh.Init(/* populate_hash_table = */ true));
  char* huge_target = new char[kTestSize];
  memset(huge_target, 'Q', kTestSize);
  CycleTimer timer;
  timer.Start();
  huge_bh.FindBestMatch(hashed_all_Qs,
                        huge_target + (kTestSize / 2),  // middle of target
                        huge_target,
                        kTestSize,
                        &best_match_);
  timer.Stop();
  double elapsed_time_in_us = static_cast<double>(timer.GetInUsec());
  LOG(INFO) << "Time to search for best match with 1M matches: "
            << elapsed_time_in_us << " us" << LOG_ENDL;
  // All blocks match the candidate block.  FindBestMatch should have checked
  // a certain number of matches before giving up.  The best match
  // should include at least half the source and target, since the candidate
  // block was in the middle of the target data.
  EXPECT_GT((kTestSize / 2), best_match_.source_offset());
  EXPECT_GT((kTestSize / 2), best_match_.target_offset());
  EXPECT_LT(static_cast<size_t>(kTestSize / 2), best_match_.size());
  EXPECT_GT(1000000, elapsed_time_in_us);  // < 1 second
  delete[] huge_target;
  delete[] huge_dictionary;
}

#ifdef GTEST_HAS_DEATH_TEST
TEST_F(BlockHashDeathTest, AddTooManyBlocks) {
  for (int i = 0; i < StringLengthAsInt(sample_text_without_spaces); ++i) {
    th_->AddOneIndexHash(i * kBlockSize, hashed_e);
  }
  // Didn't expect another block to be added
  EXPECT_DEBUG_DEATH(th_->AddOneIndexHash(StringLengthAsInt(sample_text),
                                          hashed_e),
                     "AddBlock");
}
#endif  // GTEST_HAS_DEATH_TEST

}  //  namespace open_vcdiff
