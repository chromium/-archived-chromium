// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of ChunkRange class.

#include "chrome/browser/safe_browsing/chunk_range.h"

#include "base/logging.h"
#include "base/string_util.h"

ChunkRange::ChunkRange(int start) : start_(start), stop_(start) {
}

ChunkRange::ChunkRange(int start, int stop) : start_(start), stop_(stop) {
}

ChunkRange::ChunkRange(const ChunkRange& rhs)
    : start_(rhs.start()), stop_(rhs.stop()) {
}

// Helper functions -----------------------------------------------------------

// Traverse the chunks vector looking for contiguous integers.
void ChunksToRanges(const std::vector<int>& chunks,
                    std::vector<ChunkRange>* ranges) {
  DCHECK(ranges);
  for (size_t i = 0; i < chunks.size(); ++i) {
    int start = static_cast<int>(i);
    int next = start + 1;
    while (next < static_cast<int>(chunks.size()) &&
           (chunks[start] == chunks[next] - 1 ||
            chunks[start] == chunks[next])) {
      ++start;
      ++next;
    }
    ranges->push_back(ChunkRange(chunks[i], chunks[start]));
    if (next >= static_cast<int>(chunks.size()))
      break;
    i = start;
  }
}

void RangesToChunks(const std::vector<ChunkRange>& ranges,
                    std::vector<int>* chunks) {
  DCHECK(chunks);
  for (size_t i = 0; i < ranges.size(); ++i) {
    const ChunkRange& range = ranges[i];
    for (int chunk = range.start(); chunk <= range.stop(); ++chunk) {
      chunks->push_back(chunk);
    }
  }
}

void RangesToString(const std::vector<ChunkRange>& ranges,
                    std::string* result) {
  DCHECK(result);
  result->clear();
  std::vector<ChunkRange>::const_iterator it = ranges.begin();
  for (; it != ranges.end(); ++it) {
    if (!result->empty())
      result->append(",");
    if (it->start() == it->stop()) {
      std::string num_buf = IntToString(it->start());
      result->append(num_buf);
    } else {
      result->append(StringPrintf("%d-%d", it->start(), it->stop()));
    }
  }
}

bool StringToRanges(const std::string& input,
                    std::vector<ChunkRange>* ranges) {
  DCHECK(ranges);

  // Crack the string into chunk parts, then crack each part looking for a
  // range.
  std::vector<std::string> chunk_parts;
  SplitString(input, ',', &chunk_parts);

  for (size_t i = 0; i < chunk_parts.size(); ++i) {
    std::vector<std::string> chunk_ranges;
    SplitString(chunk_parts[i], '-', &chunk_ranges);
    int start = atoi(chunk_ranges[0].c_str());
    int stop = start;
    if (chunk_ranges.size() == 2)
      stop = atoi(chunk_ranges[1].c_str());
    if (start == 0 || stop == 0) {
      // atoi error, since chunk numbers are guaranteed to never be 0.
      ranges->clear();
      return false;
    }
    ranges->push_back(ChunkRange(start, stop));
  }

  return true;
}

// Binary search over a series of ChunkRanges.
bool IsChunkInRange(int chunk_number, const std::vector<ChunkRange>& ranges) {
  if (ranges.empty())
    return false;

  int low = 0;
  int high = ranges.size() - 1;

  while (low <= high) {
    // http://googleresearch.blogspot.com/2006/06/extra-extra-read-all-about-it-nearly.html
    int mid = ((unsigned int)low + (unsigned int)high) >> 1;
    const ChunkRange& chunk = ranges[mid];
    if ((chunk.stop() >= chunk_number) && (chunk.start() <= chunk_number))
      return true;  // chunk_number is in range.

    // Adjust our mid point.
    if (chunk.stop() < chunk_number)
      low = mid + 1;
    else
      high = mid - 1;
  }

  return false;
}
