// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PAGE_RANGE_H_
#define PRINTING_PAGE_RANGE_H_

#include <vector>

namespace printing {

struct PageRange;

typedef std::vector<PageRange> PageRanges;

// Print range is inclusive. To select one page, set from == to.
struct PageRange {
  int from;
  int to;

  bool operator==(const PageRange& rhs) const {
    return from == rhs.from && to == rhs.to;
  }

  // Retrieves the sorted list of unique pages in the page ranges.
  static std::vector<int> GetPages(const PageRanges& ranges);

  // Gets the total number of pages.
  static int GetTotalPages(const PageRanges& ranges);
};

}  // namespace printing

#endif  // PRINTING_PAGE_RANGE_H_
