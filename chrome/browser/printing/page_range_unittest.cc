// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "chrome/browser/printing/page_range.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PageRangeTest, RangeMerge) {
  printing::PageRanges ranges;
  printing::PageRange range;
  range.from = 1;
  range.to = 3;
  ranges.push_back(range);
  range.from = 10;
  range.to = 12;
  ranges.push_back(range);
  range.from = 2;
  range.to = 5;
  ranges.push_back(range);
  std::vector<int> pages(printing::PageRange::GetPages(ranges));
  ASSERT_EQ(8, pages.size());
  EXPECT_EQ(1, pages[0]);
  EXPECT_EQ(2, pages[1]);
  EXPECT_EQ(3, pages[2]);
  EXPECT_EQ(4, pages[3]);
  EXPECT_EQ(5, pages[4]);
  EXPECT_EQ(10, pages[5]);
  EXPECT_EQ(11, pages[6]);
  EXPECT_EQ(12, pages[7]);
}

TEST(PageRangeTest, Empty) {
  printing::PageRanges ranges;
  std::vector<int> pages(printing::PageRange::GetPages(ranges));
  EXPECT_EQ(0, pages.size());
}
