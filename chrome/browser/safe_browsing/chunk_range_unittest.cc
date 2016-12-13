// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Test program to convert lists of integers into ranges, and vice versa.

#include "chunk_range.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test formatting chunks into a string representation.
TEST(SafeBrowsingChunkRangeTest, TestRangesToString) {
  std::vector<ChunkRange> ranges;
  ranges.push_back(ChunkRange(1, 10));
  ranges.push_back(ChunkRange(15, 17));
  ranges.push_back(ChunkRange(21, 410));
  ranges.push_back(ChunkRange(991, 1000));

  std::string range_string;
  RangesToString(ranges, &range_string);
  EXPECT_EQ(range_string, "1-10,15-17,21-410,991-1000");
  ranges.clear();

  ranges.push_back(ChunkRange(4, 4));
  RangesToString(ranges, &range_string);
  EXPECT_EQ(range_string, "4");

  ranges.push_back(ChunkRange(7));
  ranges.push_back(ChunkRange(9));
  RangesToString(ranges, &range_string);
  EXPECT_EQ(range_string, "4,7,9");

  ranges.push_back(ChunkRange(42, 99));
  RangesToString(ranges, &range_string);
  EXPECT_EQ(range_string, "4,7,9,42-99");
}


// Test various configurations of chunk numbers.
TEST(SafeBrowsingChunkRangeTest, TestChunksToRanges) {
  std::vector<int> chunks;
  std::vector<ChunkRange> ranges;

  // Test one chunk range and one single value.
  chunks.push_back(1);
  chunks.push_back(2);
  chunks.push_back(3);
  chunks.push_back(4);
  chunks.push_back(7);
  ChunksToRanges(chunks, &ranges);
  EXPECT_EQ(ranges.size(), static_cast<size_t>(2));
  EXPECT_EQ(ranges[0].start(), 1);
  EXPECT_EQ(ranges[0].stop(),  4);
  EXPECT_EQ(ranges[1].start(), 7);
  EXPECT_EQ(ranges[1].stop(),  7);

  chunks.clear();
  ranges.clear();

  // Test all chunk numbers in one range.
  chunks.push_back(3);
  chunks.push_back(4);
  chunks.push_back(5);
  chunks.push_back(6);
  chunks.push_back(7);
  chunks.push_back(8);
  chunks.push_back(9);
  chunks.push_back(10);
  ChunksToRanges(chunks, &ranges);
  EXPECT_EQ(ranges.size(), static_cast<size_t>(1));
  EXPECT_EQ(ranges[0].start(), 3);
  EXPECT_EQ(ranges[0].stop(),  10);

  chunks.clear();
  ranges.clear();

  // Test no chunk numbers in contiguous ranges.
  chunks.push_back(3);
  chunks.push_back(5);
  chunks.push_back(7);
  chunks.push_back(9);
  chunks.push_back(11);
  chunks.push_back(13);
  chunks.push_back(15);
  chunks.push_back(17);
  ChunksToRanges(chunks, &ranges);
  EXPECT_EQ(ranges.size(), static_cast<size_t>(8));

  chunks.clear();
  ranges.clear();

  // Test a single chunk number.
  chunks.push_back(17);
  ChunksToRanges(chunks, &ranges);
  EXPECT_EQ(ranges.size(), static_cast<size_t>(1));
  EXPECT_EQ(ranges[0].start(), 17);
  EXPECT_EQ(ranges[0].stop(),  17);

  chunks.clear();
  ranges.clear();

  // Test duplicates.
  chunks.push_back(1);
  chunks.push_back(2);
  chunks.push_back(2);
  chunks.push_back(2);
  chunks.push_back(3);
  chunks.push_back(7);
  chunks.push_back(7);
  chunks.push_back(7);
  chunks.push_back(7);
  ChunksToRanges(chunks, &ranges);
  EXPECT_EQ(ranges.size(), static_cast<size_t>(2));
  EXPECT_EQ(ranges[0].start(), 1);
  EXPECT_EQ(ranges[0].stop(), 3);
  EXPECT_EQ(ranges[1].start(), 7);
  EXPECT_EQ(ranges[1].stop(), 7);
}


TEST(SafeBrowsingChunkRangeTest, TestStringToRanges) {
  std::vector<ChunkRange> ranges;

  std::string input = "1-100,398,415,1138-2001,2019";
  EXPECT_TRUE(StringToRanges(input, &ranges));
  EXPECT_EQ(ranges.size(), static_cast<size_t>(5));
  EXPECT_EQ(ranges[0].start(), 1);
  EXPECT_EQ(ranges[0].stop(),  100);
  EXPECT_EQ(ranges[1].start(), 398);
  EXPECT_EQ(ranges[1].stop(),  398);
  EXPECT_EQ(ranges[3].start(), 1138);
  EXPECT_EQ(ranges[3].stop(),  2001);

  ranges.clear();

  input = "1,2,3,4,5,6,7";
  EXPECT_TRUE(StringToRanges(input, &ranges));
  EXPECT_EQ(ranges.size(), static_cast<size_t>(7));

  ranges.clear();

  input = "300-3001";
  EXPECT_TRUE(StringToRanges(input, &ranges));
  EXPECT_EQ(ranges.size(), static_cast<size_t>(1));
  EXPECT_EQ(ranges[0].start(),  300);
  EXPECT_EQ(ranges[0].stop(),  3001);

  ranges.clear();

  input = "17";
  EXPECT_TRUE(StringToRanges(input, &ranges));
  EXPECT_EQ(ranges.size(), static_cast<size_t>(1));
  EXPECT_EQ(ranges[0].start(), 17);
  EXPECT_EQ(ranges[0].stop(),  17);

  ranges.clear();

  input = "x-y";
  EXPECT_FALSE(StringToRanges(input, &ranges));
}


TEST(SafeBrowsingChunkRangeTest, TestRangesToChunks) {
  std::vector<ChunkRange> ranges;
  ranges.push_back(ChunkRange(1, 4));
  ranges.push_back(ChunkRange(17));

  std::vector<int> chunks;
  RangesToChunks(ranges, &chunks);

  EXPECT_EQ(chunks.size(), static_cast<size_t>(5));
  EXPECT_EQ(chunks[0], 1);
  EXPECT_EQ(chunks[1], 2);
  EXPECT_EQ(chunks[2], 3);
  EXPECT_EQ(chunks[3], 4);
  EXPECT_EQ(chunks[4], 17);
}


TEST(SafeBrowsingChunkRangeTest, TestSearchChunkRanges) {
  std::string range_str("1-10,15-17,21-410,555,991-1000");
  std::vector<ChunkRange> ranges;
  StringToRanges(range_str, &ranges);

  EXPECT_TRUE(IsChunkInRange(7, ranges));
  EXPECT_TRUE(IsChunkInRange(300, ranges));
  EXPECT_TRUE(IsChunkInRange(555, ranges));
  EXPECT_TRUE(IsChunkInRange(1, ranges));
  EXPECT_TRUE(IsChunkInRange(1000, ranges));

  EXPECT_FALSE(IsChunkInRange(11, ranges));
  EXPECT_FALSE(IsChunkInRange(990, ranges));
  EXPECT_FALSE(IsChunkInRange(2000, ranges));
}
