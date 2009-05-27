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


// This file contains the declaration of class FileTextWriter.

#ifndef O3D_UTILS_CROSS_FILE_TEXT_WRITER_H_
#define O3D_UTILS_CROSS_FILE_TEXT_WRITER_H_

#include <stdio.h>

#include <string>

#include "utils/cross/text_writer.h"

namespace o3d {

// A FileTextWriter writes a sequence of characters to a file.
class FileTextWriter : public TextWriter {
 public:
  // TODO: decide how we're going to use Unicode: wstrings of wide
  // chars or strings encoded as UTF8? Only support ASCII for now (although it
  // doesn't check that all chars are in the ASCII range).
  enum Encoding {
    ASCII,
  };

  // Take ownership of the given file. Prepare to write characters to
  // it using the specified character encoding and new line sequence.
  FileTextWriter(FILE* file, Encoding encoding, NewLine new_line);
  virtual ~FileTextWriter();
  virtual void WriteChar(char c);
  virtual void WriteString(const std::string& s);
  virtual void Close();
 private:
  FILE* file_;
  Encoding encoding_;
  DISALLOW_COPY_AND_ASSIGN(FileTextWriter);
};
}  // namespace o3d

#endif  // O3D_UTILS_CROSS_FILE_TEXT_WRITER_H_
