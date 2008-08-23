// Copyright 2007 Google Inc.
// Authors: Jeff Dean, Lincoln Smith
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
#include "rolling_hash.h"
#include <stdint.h>  // uint32_t
#include <cstdlib>  // rand, srand
#include <vector>
#include "testing.h"

namespace open_vcdiff {
namespace {

static const uint32_t kBase = RollingHashUtil::kBase;

class RollingHashSimpleTest : public testing::Test {
 protected:
  RollingHashSimpleTest() { }
  virtual ~RollingHashSimpleTest() { }

  void TestModBase(uint32_t operand) {
    EXPECT_EQ(operand % kBase, RollingHashUtil::ModBase(operand));
    EXPECT_EQ(static_cast<uint32_t>((-static_cast<int32_t>(operand)) % kBase),
              RollingHashUtil::FindModBaseInverse(operand));
    EXPECT_EQ(0U, RollingHashUtil::ModBase(
        operand + RollingHashUtil::FindModBaseInverse(operand)));
  }

  void TestHashFirstTwoBytes(char first_value, char second_value) {
    char buf[2];
    buf[0] = first_value;
    buf[1] = second_value;
    EXPECT_EQ(RollingHashUtil::HashFirstTwoBytes(buf),
              RollingHashUtil::HashStep(RollingHashUtil::HashStep(0,
                                                                  first_value),
                                        second_value));
    EXPECT_EQ(RollingHashUtil::HashFirstTwoBytes(buf),
              RollingHashUtil::HashStep(static_cast<unsigned char>(first_value),
                                        second_value));
  }
};

#ifdef GTEST_HAS_DEATH_TEST
typedef RollingHashSimpleTest RollingHashDeathTest;
#endif  // GTEST_HAS_DEATH_TEST

TEST_F(RollingHashSimpleTest, KBaseIsAPowerOfTwo) {
  EXPECT_EQ(0U, kBase & (kBase - 1));
}

TEST_F(RollingHashSimpleTest, TestModBaseForValues) {
  TestModBase(0);
  TestModBase(10);
  TestModBase(static_cast<uint32_t>(-10));
  TestModBase(kBase - 1);
  TestModBase(kBase);
  TestModBase(kBase + 1);
  TestModBase(0x7FFFFFFF);
  TestModBase(0x80000000);
  TestModBase(0xFFFFFFFE);
  TestModBase(0xFFFFFFFF);
}

TEST_F(RollingHashSimpleTest, VerifyHashFirstTwoBytes) {
  TestHashFirstTwoBytes(0x00, 0x00);
  TestHashFirstTwoBytes(0x00, 0xFF);
  TestHashFirstTwoBytes(0xFF, 0x00);
  TestHashFirstTwoBytes(0xFF, 0xFF);
  TestHashFirstTwoBytes(0x00, 0x80);
  TestHashFirstTwoBytes(0x7F, 0xFF);
  TestHashFirstTwoBytes(0x7F, 0x80);
  TestHashFirstTwoBytes(0x01, 0x8F);
}

#ifdef GTEST_HAS_DEATH_TEST
TEST_F(RollingHashDeathTest, InstantiateBlockHashWithoutCallingInit) {
  EXPECT_DEBUG_DEATH(RollingHash<16> bad_hash, "Init");
}
#endif  // GTEST_HAS_DEATH_TEST

class RollingHashTest : public testing::Test {
 public:
  static const int kUpdateHashBlocks = 1000;
  static const int kLargestBlockSize = 128;

  static void MakeRandomBuffer(char* buffer, int buffer_size) {
    for (int i = 0; i < buffer_size; ++i) {
      buffer[i] = PortableRandomInRange<unsigned char>(0xFF);
    }
  }

  template<int kBlockSize> static void BM_DefaultHash(int iterations,
                                                      const char *buffer) {
    RollingHash<kBlockSize> hasher;
    static uint32_t result_array[kUpdateHashBlocks];
    for (int iter = 0; iter < iterations; ++iter) {
      for (int i = 0; i < kUpdateHashBlocks; ++i) {
        result_array[i] = hasher.Hash(&buffer[i]);
      }
    }
  }

  template<int kBlockSize> static void BM_UpdateHash(int iterations,
                                                     const char *buffer) {
    RollingHash<kBlockSize> hasher;
    static uint32_t result_array[kUpdateHashBlocks];
    for (int iter = 0; iter < iterations; ++iter) {
      uint32_t running_hash = hasher.Hash(buffer);
      for (int i = 0; i < kUpdateHashBlocks; ++i) {
        running_hash = hasher.UpdateHash(running_hash,
                                         buffer[i],
                                         buffer[i + kBlockSize]);
        result_array[i] = running_hash;
      }
    }
  }

 protected:
  static const int kUpdateHashTestIterations = 400;
  static const int kTimingTestSize = 1 << 14;  // 16K iterations

  RollingHashTest() { }
  virtual ~RollingHashTest() { }

  template<int kBlockSize> void UpdateHashMatchesHashForBlockSize() {
    RollingHash<kBlockSize>::Init();
    RollingHash<kBlockSize> hasher;
    for (int x = 0; x < kUpdateHashTestIterations; ++x) {
      int random_buffer_size =
          PortableRandomInRange(kUpdateHashBlocks - 1) + kBlockSize;
      MakeRandomBuffer(buffer_, random_buffer_size);
      uint32_t running_hash = hasher.Hash(buffer_);
      for (int i = kBlockSize; i < random_buffer_size; ++i) {
        // UpdateHash() calculates the hash value incrementally.
        running_hash = hasher.UpdateHash(running_hash,
                                         buffer_[i - kBlockSize],
                                         buffer_[i]);
        // Hash() calculates the hash value from scratch.  Verify that both
        // methods return the same hash value.
        EXPECT_EQ(running_hash, hasher.Hash(&buffer_[i + 1 - kBlockSize]));
      }
    }
  }

  template<int kBlockSize> double DefaultHashTimingTest() {
    // Execution time is expected to be O(kBlockSize) per hash operation,
    // so scale the number of iterations accordingly
    const int kTimingTestIterations = kTimingTestSize / kBlockSize;
    CycleTimer timer;
    timer.Start();
    BM_DefaultHash<kBlockSize>(kTimingTestIterations, buffer_);
    timer.Stop();
    return static_cast<double>(timer.GetInUsec())
        / (kTimingTestIterations * kUpdateHashBlocks);
  }

  template<int kBlockSize> double RollingTimingTest() {
    // Execution time is expected to be O(1) per hash operation,
    // so leave the number of iterations constant
    const int kTimingTestIterations = kTimingTestSize;
    CycleTimer timer;
    timer.Start();
    BM_UpdateHash<kBlockSize>(kTimingTestIterations, buffer_);
    timer.Stop();
    return static_cast<double>(timer.GetInUsec())
        / (kTimingTestIterations * kUpdateHashBlocks);
  }

  double FindPercentage(double original, double modified) {
    if (original < 0.0001) {
      return 0.0;
    } else {
      return ((modified - original) / original) * 100.0;
    }
  }

  template<int kBlockSize> void RunTimingTestForBlockSize() {
    RollingHash<kBlockSize>::Init();
    MakeRandomBuffer(buffer_, sizeof(buffer_));
    const double time_for_default_hash = DefaultHashTimingTest<kBlockSize>();
    const double time_for_rolling_hash = RollingTimingTest<kBlockSize>();
    printf("%d\t%f\t%f (%f%%)\n",
           kBlockSize,
           time_for_default_hash,
           time_for_rolling_hash,
           FindPercentage(time_for_default_hash, time_for_rolling_hash));
    CHECK_GT(time_for_default_hash, 0.0);
    CHECK_GT(time_for_rolling_hash, 0.0);
    if (kBlockSize > 16) {
      EXPECT_GT(time_for_default_hash, time_for_rolling_hash);
    }
  }

  char buffer_[kUpdateHashBlocks + kLargestBlockSize];
};

TEST_F(RollingHashTest, UpdateHashMatchesHashFromScratch) {
  srand(1);  // test should be deterministic, including calls to rand()
  UpdateHashMatchesHashForBlockSize<4>();
  UpdateHashMatchesHashForBlockSize<8>();
  UpdateHashMatchesHashForBlockSize<16>();
  UpdateHashMatchesHashForBlockSize<32>();
  UpdateHashMatchesHashForBlockSize<64>();
  UpdateHashMatchesHashForBlockSize<128>();
}

TEST_F(RollingHashTest, TimingTests) {
  srand(1);  // test should be deterministic, including calls to rand()
  printf("BlkSize\tHash (us)\tUpdateHash (us)\n");
  RunTimingTestForBlockSize<4>();
  RunTimingTestForBlockSize<8>();
  RunTimingTestForBlockSize<16>();
  RunTimingTestForBlockSize<32>();
  RunTimingTestForBlockSize<64>();
  RunTimingTestForBlockSize<128>();
}

}  // anonymous namespace
}  // namespace open_vcdiff
