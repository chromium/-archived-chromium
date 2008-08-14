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

#ifndef BASE_WORD_ITERATOR_H__
#define BASE_WORD_ITERATOR_H__

#include <string>
#include <vector>

#include "unicode/uchar.h"

#include "base/basictypes.h"

// The WordIterator class iterates through the words and word breaks
// in a string.  (In the string " foo bar! ", the word breaks are at the
// periods in ". .foo. .bar.!. .".)
//
// To extract the words from a string, move a WordIterator through the
// string and test whether IsWord() is true.  E.g.,
//   WordIterator iter(str, WordIterator::BREAK_WORD);
//   if (!iter.Init()) return false;
//   while (iter.Advance()) {
//     if (iter.IsWord()) {
//       // region [iter.prev(),iter.pos()) contains a word.
//       LOG(INFO) << "word: " << iter.GetWord();
//     }
//   }


class WordIterator {
 public:
  enum BreakType {
    BREAK_WORD,
    BREAK_LINE
  };

  // Requires |str| to live as long as the WordIterator does.
  WordIterator(const std::wstring& str, BreakType break_type);
  ~WordIterator();

  // Init() must be called before any of the iterators are valid.
  // Returns false if ICU failed to initialize.
  bool Init();

  // Return the current break position within the string,
  // or WordIterator::npos when done.
  int pos() const { return pos_; }
  // Return the value of pos() returned before Advance() was last called.
  int prev() const { return prev_; }

  // A special position value indicating "end of string".
  static const int npos;

  // Advance to the next break.  Returns false if we've run past the end of
  // the string.  (Note that the very last "word break" is after the final
  // character in the string, and when we advance to that position it's the
  // last time Advance() returns true.)
  bool Advance();

  // Returns true if the break we just hit is the end of a word.
  // (Otherwise, the break iterator just skipped over e.g. whitespace
  // or punctuation.)
  bool IsWord() const;

  // Return the word between prev() and pos().
  // Advance() must have been called successfully at least once
  // for pos() to have advanced to somewhere useful.
  std::wstring GetWord() const;

 private:
  // ICU iterator.
  void* iter_;
#if !defined(WCHAR_T_IS_UTF16)
  std::vector<UChar> chars_;
#endif

  // The string we're iterating over.
  const std::wstring& string_;

  // The breaking style (word/line).
  BreakType break_type_;

  // Previous and current iterator positions.
  int prev_, pos_;

  DISALLOW_EVIL_CONSTRUCTORS(WordIterator);
};

#endif  // BASE_WORD_ITERATOR_H__
