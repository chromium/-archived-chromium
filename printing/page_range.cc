// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/page_range.h"

#include "base/stl_util-inl.h"

namespace printing {

/* static */
std::vector<int> PageRange::GetPages(const PageRanges& ranges) {
  std::set<int> pages;
  for (unsigned i = 0; i < ranges.size(); ++i) {
    const PageRange& range = ranges[i];
    // Ranges are inclusive.
    for (int i = range.from; i <= range.to; ++i) {
      pages.insert(i);
    }
  }
  return SetToVector(pages);
}

/* static */
int PageRange::GetTotalPages(const PageRanges& ranges) {
  // Since ranges can overlap we need to merge them before counting
  std::vector<int> pages = PageRange::GetPages(ranges);
  return pages.size();
}

}  // namespace printing
