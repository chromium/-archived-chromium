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

// This module computes snippets of queries based on hits in the documents
// for display in history search results.

#ifndef CHROME_BROWSER_HISTORY_SNIPPET_H__
#define CHROME_BROWSER_HISTORY_SNIPPET_H__

#include <vector>

class Snippet {
 public:
  // Each pair in MatchPositions is the [begin, end) positions of a match
  // within a string.
  typedef std::vector<std::pair<int, int> > MatchPositions;

  // Parses an offsets string as returned from a sqlite full text index. An
  // offsets string encodes information about why a row matched a text query.
  // The information is encoded in the string as a set of matches, where each
  // match consists of the column, term-number, location, and length of the
  // match. Each element of the match is separated by a space, as is each match
  // from other matches.
  //
  // This method adds the start and end of each match whose column is
  // column_num to match_positions. The pairs are ordered based on first,
  // with no overlapping elements.
  //
  // NOTE: the positions returned are in terms of UTF8 encoding. To convert the
  // offsets to wide, use ConvertMatchPositionsToWide.
  static void ExtractMatchPositions(const std::string& offsets_str,
                                    const std::string& column_num,
                                    MatchPositions* match_positions);

  // Converts match positions as returned from ExtractMatchPositions to be in
  // terms of a wide string.
  static void ConvertMatchPositionsToWide(
      const std::string& utf8_string,
      Snippet::MatchPositions* match_positions);

  // Given |matches|, the match positions within |document|, compute the snippet
  // for the document.
  // Note that |document| is UTF-8 and the offsets in |matches| are byte
  // offsets.
  void ComputeSnippet(const MatchPositions& matches,
                      const std::string& document);

  const std::wstring& text() const { return text_; }
  const MatchPositions& matches() const { return matches_; }

  // Efficiently swaps the contents of this snippet with the other.
  void Swap(Snippet* other) {
    text_.swap(other->text_);
    matches_.swap(other->matches_);
  }

 private:
  // The text of the snippet.
  std::wstring text_;

  // The matches within text_.
  MatchPositions matches_;
};

#endif  // CHROME_BROWSER_HISTORY_SNIPPET_H__
