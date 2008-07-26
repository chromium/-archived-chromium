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
// Class for parsing lists of integers into ranges.
//
// The anti-phishing and anti-malware protocol sends ASCII strings of numbers
// and ranges of numbers corresponding to chunks of whitelists and blacklists.
// Clients of this protocol need to be able to convert back and forth between
// this representation, and individual integer chunk numbers. The ChunkRange
// class is a simple and compact mechanism for storing a continuous list of
// chunk numbers.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHUNK_RANGE_H__
#define CHROME_BROWSER_SAFE_BROWSING_CHUNK_RANGE_H__

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


#endif  // CHROME_BROWSER_SAFE_BROWSING_CHUNK_RANGE_H__