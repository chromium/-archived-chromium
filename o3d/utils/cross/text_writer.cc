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


// This file contains the definition of class TextWriter. The
// tests are in string_writer_test.cc.

#include <stdarg.h>

#include "utils/cross/text_writer.h"
#include "base/string_util.h"

using std::string;

namespace o3d {
TextWriter::TextWriter(NewLine new_line)
    : new_line_(new_line) {
}

TextWriter::~TextWriter() {
}

void TextWriter::WriteString(const string& s) {
  for (size_t i = 0; i != s.length(); ++i) {
    WriteChar(s[i]);
  }
}

void TextWriter::WriteBool(bool value) {
  if (value) {
    WriteString(string("true"));
  } else {
    WriteString(string("false"));
  }
}

void TextWriter::WriteInt(int value) {
  WriteString(StringPrintf("%d", value));
}

void TextWriter::WriteUnsignedInt(unsigned int value) {
  WriteString(StringPrintf("%u", value));
}

void TextWriter::WriteFloat(float value) {
  WriteString(StringPrintf("%g", value));
}

void TextWriter::WriteFormatted(const char* format, ...) {
  va_list args;
  va_start(args, format);
  string formatted;
  StringAppendV(&formatted, format, args);
  va_end(args);

  WriteString(formatted);
}

void TextWriter::WriteNewLine() {
  switch (new_line_) {
    case CR:
      WriteChar('\r');
      break;
    case CR_LF:
      WriteChar('\r');
      WriteChar('\n');
      break;
    case LF:
      WriteChar('\n');
      break;
  }
}

void TextWriter::Close() {
}
}  // namespace o3d
