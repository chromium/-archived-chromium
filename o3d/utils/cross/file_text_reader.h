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


// This file contains the declaration of class FileTextReader.

#ifndef O3D_UTILS_CROSS_FILE_TEXT_READER_H_
#define O3D_UTILS_CROSS_FILE_TEXT_READER_H_

#include <string>
#include "utils/cross/text_reader.h"

namespace o3d {

// A FileTextReader reads a sequence of characters from an
// in-memory string.
class FileTextReader : public TextReader {
 public:
  // Prepare to read from the given input string.
  explicit FileTextReader(FILE* input);
  virtual ~FileTextReader();

  virtual bool IsAtEnd() const;
  virtual std::string PeekString(std::string::size_type count) const;
  virtual char ReadChar();
  virtual std::string ReadString(std::string::size_type count);
  virtual std::string ReadLine();
  virtual std::string ReadToEnd();

  // Access to the original input file pointer, and the current
  // position in that buffer.
  const FILE* input() const { return input_; }
  size_t position() const;

 protected:
  size_t GetFileSize() const;
  size_t GetRemainingSize() const;

 private:
  FILE* input_;
  DISALLOW_COPY_AND_ASSIGN(FileTextReader);
};
}  // namespace o3d

#endif  // O3D_UTILS_CROSS_FILE_TEXT_READER_H_
