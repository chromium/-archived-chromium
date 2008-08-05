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

#include "base/logging.h"
#include "base/word_iterator.h"
#include "unicode/ubrk.h"

const int WordIterator::npos = -1;

WordIterator::WordIterator(const std::wstring& str, BreakType break_type)
    : iter_(NULL),
      string_(str),
      break_type_(break_type),
      prev_(npos),
      pos_(0) {
}

WordIterator::~WordIterator() {
  if (iter_)
    ubrk_close(iter_);
}

bool WordIterator::Init() {
  UErrorCode status = U_ZERO_ERROR;
  UBreakIteratorType break_type;
  switch (break_type_) {
    case BREAK_WORD:
      break_type = UBRK_WORD;
      break;
    case BREAK_LINE:
      break_type = UBRK_LINE;
      break;
    default:
      NOTREACHED();
      break_type = UBRK_LINE;
  }
#ifdef U_WCHAR_IS_UTF16
  iter_ = ubrk_open(break_type, NULL,
                    string_.data(), static_cast<int32_t>(string_.size()),
                    &status);
#else  // U_WCHAR_IS_UTF16
  // When wchar_t is wider than UChar (16 bits), transform |string_| into a
  // UChar* string.  Size the UChar* buffer to be large enough to hold twice
  // as many UTF-16 code points as there are UCS-4 characters, in case each
  // character translates to a UTF-16 surrogate pair, and leave room for a NUL
  // terminator.
  // TODO(avi): avoid this alloc
  chars_.resize(wide.length() * sizeof(UChar) + 1);

  UErrorCode error = U_ZERO_ERROR;
  int32_t destLength;
  u_strFromWCS(&chars_[0], chars_.size(), &destLength, string_.data(),
               string_.length(), &error);
  
  iter_ = ubrk_open(break_type, NULL, chars_, destLength, &status);
#endif
  if (U_FAILURE(status)) {
    NOTREACHED() << "ubrk_open failed";
    return false;
  }
  ubrk_first(iter_);  // Move the iterator to the beginning of the string.
  return true;
}

bool WordIterator::Advance() {
  prev_ = pos_;
  const int32_t pos = ubrk_next(iter_);
  if (pos == UBRK_DONE) {
    pos_ = npos;
    return false;
  } else {
    pos_ = static_cast<int>(pos);
    return true;
  }
}

bool WordIterator::IsWord() const {
  return (ubrk_getRuleStatus(iter_) != UBRK_WORD_NONE);
}