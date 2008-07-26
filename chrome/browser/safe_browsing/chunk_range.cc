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
      char num_buf[11];  // One 32 bit positive integer + NULL.
      _itoa_s(it->start(), num_buf, sizeof(num_buf), 10);
      result->append(num_buf);
    } else {
      result->append(StringPrintf("%d-%d", it->start(), it->stop()));
    }
  }
}

bool StringToRanges(const std::string& input,
                    std::vector<ChunkRange>* ranges) {
  DCHECK(ranges);

  // Crack the string into chunk parts, then crack each part looking for a range.
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