// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Class for parsing lists of integers into ranges.
//
// The anti-phishing and anti-malware protocol sends ASCII strings of numbers
// and ranges of numbers corresponding to chunks of whitelists and blacklists.
// Clients of this protocol need to be able to convert back and forth between
// this representation, and individual integer chunk numbers. The ChunkRange
// class is a simple and compact mechanism for storing a continuous list of
// chunk numbers.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHUNK_RANGE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHUNK_RANGE_H_

#include <string>
#include <vector>

// ChunkRange ------------------------------------------------------------------
// Each ChunkRange represents a continuous range of chunk numbers [start, stop].

class ChunkRange {
 public:
  ChunkRange(int start);
  ChunkRange(int start, int stop);
  ChunkRange(const ChunkRange& rhs);

  inline int start() const { return start_; }
  inline int stop() const { return stop_; }

  bool operator==(const ChunkRange& rhs) const {
    return start_ == rhs.start() && stop_ == rhs.stop();
  }

 private:
  int start_;
  int stop_;
};


// Helper functions ------------------------------------------------------------

// Convert a series of chunk numbers into a more compact range representation.
// The 'chunks' vector must be sorted in ascending order.
void ChunksToRanges(const std::vector<int>& chunks,
                    std::vector<ChunkRange>* ranges);

// Convert a set of ranges into individual chunk numbers.
void RangesToChunks(const std::vector<ChunkRange>& ranges,
                    std::vector<int>* chunks);

// Convert a series of chunk ranges into a string in protocol format.
void RangesToString(const std::vector<ChunkRange>& ranges,
                    std::string* result);

// Returns 'true' if the string was successfully converted to ChunkRanges,
// 'false' if the input was malformed.
// The string must be in the form: "1-100,398,415,1138-2001,2019".
bool StringToRanges(const std::string& input,
                    std::vector<ChunkRange>* ranges);

// Tests if a chunk number is contained a sorted vector of ChunkRanges.
bool IsChunkInRange(int chunk_number, const std::vector<ChunkRange>& ranges);

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHUNK_RANGE_H_
