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


// This file contains the declaration of class TextWriter.

#ifndef O3D_UTILS_CROSS_TEXT_WRITER_H_
#define O3D_UTILS_CROSS_TEXT_WRITER_H_

#include <string>

#include "base/basictypes.h"

namespace o3d {

// TextWriter is the abstract base class of classes
// that write a sequence of characters to an underlying
// medium.
class TextWriter {
 public:
  enum NewLine {
    LF,
    CR_LF,
    CR,
  };

  // Construct a new TextWriter using the given newline
  // sequence.
  explicit TextWriter(NewLine new_line);
  virtual ~TextWriter();

  // Write a single character.
  virtual void WriteChar(char c) = 0;

  // Write a string of characters.
  virtual void WriteString(const std::string& s);

  // Write "true" or "false".
  void WriteBool(bool value);

  // Write a signed integer.
  void WriteInt(int value);

  // Write an unsigned integer.
  void WriteUnsignedInt(unsigned int value);

  // Write a floating point number.
  void WriteFloat(float value);

  // Write a printf formatted string.
  void WriteFormatted(const char* format, ...);

  // Write a newline.
  void WriteNewLine();

  // Close the writer.
  virtual void Close();

  NewLine new_line() const { return new_line_; }

 private:
  NewLine new_line_;
  DISALLOW_COPY_AND_ASSIGN(TextWriter);
};
}  // namespace o3d

#endif  // O3D_UTILS_CROSS_TEXT_WRITER_H_
