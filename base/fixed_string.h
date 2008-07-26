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

#ifndef BASE_FIXED_STRING_H__
#define BASE_FIXED_STRING_H__

#include <string.h>

#include "base/string_util.h"

// This class manages a fixed-size, null-terminated string buffer.  It is meant
// to be allocated on the stack, and it makes no use of the heap internally.  In
// most cases you'll just want to use a std::(w)string, but when you need to
// avoid the heap, you can use this class instead.
//
// Methods are provided to read the null-terminated buffer and to append data
// to the buffer, and once the buffer fills-up, it simply discards any extra
// append calls.
//
// Since this object clips if the internal fixed buffer is exceeded, it is
// appropriate for exception handlers where the heap may be corrupted. Fixed
// buffers that overflow onto the heap are provided by Stack[W]String.
// (see stack_container.h).
template <class CharT, int MaxSize>
class FixedString {
 public:
  typedef CharTraits<CharT> char_traits;

  FixedString() : index_(0), truncated_(false) {
    buf_[0] = CharT(0);
  }

  // Returns true if the Append ever failed.
  bool was_truncated() const { return truncated_; }

  // Returns the number of characters in the string, excluding the null
  // terminator.
  size_t size() const { return index_; }

  // Returns the null-terminated string.
  const CharT* get() const { return buf_; }
  CharT* get() { return buf_; }

  // Append an array of characters.  The operation is bounds checked, and if
  // there is insufficient room, then the was_truncated() flag is set to true.
  void Append(const CharT* s, size_t n) {
    if (char_traits::copy_num(buf_ + index_, arraysize(buf_) - index_, s, n)) {
      index_ += n;
    } else {
      truncated_ = true;
    }
  }

  // Append a null-terminated string.
  void Append(const CharT* s) {
    Append(s, char_traits::length(s));
  }

  // Append a single character.
  void Append(CharT c) {
    Append(&c, 1);
  }

 private:
  CharT buf_[MaxSize];
  size_t index_;
  bool truncated_;
};

#endif  // BASE_FIXED_STRING_H__
