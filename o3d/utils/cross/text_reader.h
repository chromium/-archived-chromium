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


// This file contains the declaration of class TextReader.

#ifndef O3D_UTILS_CROSS_TEXT_READER_H_
#define O3D_UTILS_CROSS_TEXT_READER_H_

#include <string>
#include "base/basictypes.h"

namespace o3d {

// TextReader is the abstract base class of classes
// that read a sequence of characters to an underlying
// medium.
class TextReader {
 public:
  // Construct a new TextReader.
  TextReader();
  virtual ~TextReader();

  // Returns true if at the end of the input.
  virtual bool IsAtEnd() const = 0;

  // Read ahead up to count characters and return them as a string,
  // but don't modify the input's current position.  Returns the empty
  // string if at the end of the input.
  virtual std::string PeekString(std::string::size_type count) const = 0;

  // Read a single character.
  virtual char ReadChar() = 0;

  // Read a number of characters as a string.  If the input isn't that
  // long, the string may be shorter than the given number of
  // characters.  Returns an empty string if it is already at the end
  // of the input or if there is an error.
  virtual std::string ReadString(std::string::size_type count) = 0;

  // Read a line of text, terminated by any kind of line terminator
  // (LF/CRLF/CR).  Returns the empty string if at end of input, or if
  // there is an error.
  virtual std::string ReadLine() = 0;

  // Read the remaining file into a string, linefeeds and all.
  // Returns the empty string if already at the end of the file or
  // there is an error.
  virtual std::string ReadToEnd() = 0;

 protected:
  // Returns length of EOL marker (one or two characters) if the given
  // character string starts with an end of line marker.  Returns zero
  // if there is no EOL marker at the BEGINNING of the string.
  static int TestForEndOfLine(const std::string& eol);

 private:

  DISALLOW_COPY_AND_ASSIGN(TextReader);
};
}  // namespace o3d

#endif  // O3D_UTILS_CROSS_TEXT_READER_H_
