/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the definition of class StringReader. The
// tests are in string_reader_test.cc.

#include <stdio.h>
#include "utils/cross/string_reader.h"

namespace o3d {

StringReader::StringReader(const std::string& input)
    : TextReader(), input_(input), position_(0) {
}

StringReader::~StringReader() {
}

bool StringReader::IsAtEnd() const {
  return position_ == input_.size();
}

std::string StringReader::PeekString(
    std::string::size_type count) const {
  count = std::min(input_.size() - position_, count);
  if (count > 0) {
    return input_.substr(position_, count);
  } else {
    return "";
  }
}

char StringReader::ReadChar() {
  if (position_ <= input_.size() && input_.size() > 0) {
    position_++;
    return input_[position_ - 1];
  } else {
    return 0;
  }
}

std::string StringReader::ReadString(std::string::size_type count) {
  count = std::min(input_.size() - position_, count);
  std::string result;
  if (count > 0) {
    result = input_.substr(position_, count);
    position_ += count;
  }
  return result;
}

std::string StringReader::ReadLine() {
  std::string::size_type possible_eol = input_.find_first_of("\r\n", position_);
  if (possible_eol == std::string::npos) {
    // There wasn't an end of line marker anywhere, so just return the
    // whole string.
    return ReadToEnd();
  } else {
    if ((input_.size() - position_) > 1) {
      // See which end of line marker we have and strip it.
      int eol_len = TestForEndOfLine(input_.substr(possible_eol, 2));
      std::string result = input_.substr(position_, possible_eol - position_);
      position_ = possible_eol + eol_len;
      return result;
    } else {
      // If it's only one long, and it found a carriage return or a
      // linefeed, then we just eat that character and return an
      // empty string.
      position_++;
      return std::string("");
    }
  }
  return std::string("");
}

std::string StringReader::ReadToEnd() {
  std::string result = input_.substr(position_);
  position_ = input_.size();
  return result;
}
}  // end namespace o3d
