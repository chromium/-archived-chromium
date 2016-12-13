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


// This file contains the definition of class FileTextReader. The
// tests are in string_reader_test.cc.

#include "utils/cross/file_text_reader.h"

#include <stdio.h>
#include <sys/stat.h>
#include "base/logging.h"
#include "base/scoped_ptr.h"

namespace o3d {

FileTextReader::FileTextReader(FILE* input)
    : TextReader(), input_(input) {
  if (input_ == NULL) {
    LOG(FATAL) << "Invalid NULL file pointer";
  }
  if (::ferror(input_) != 0) {
    DLOG(FATAL) << "Invalid file pointer.";
  }
}

FileTextReader::~FileTextReader() {
}

size_t FileTextReader::position() const {
  if (input_) {
    return ::ftell(input_);
  } else {
    return 0;
  }
}

bool FileTextReader::IsAtEnd() const {
  size_t position = ::ftell(input_);
  if (::feof(input_) != 0) {
    return true;
  }
  return GetFileSize() == position;
}

std::string FileTextReader::PeekString(std::string::size_type count) const {
  if (IsAtEnd()) {
    return std::string("");
  }

  // Find out where we are in the stream.
  size_t original_pos = ::ftell(input_);

  // We're just re-using code here -- the stream will not be affected
  // because we will seek back to where we started.
  std::string result = const_cast<FileTextReader*>(this)->ReadString(count);

  // Go back to where we started in the stream.
  ::fseek(input_, original_pos, SEEK_SET);
  return result;
}

char FileTextReader::ReadChar() {
  if (!IsAtEnd()) {
    return ::fgetc(input_);
  } else {
    return 0;
  }
}

std::string FileTextReader::ReadString(std::string::size_type count) {
  if (count <= 0 || IsAtEnd()) {
    return std::string("");
  }

  // Allocate a string to hold the data.
  scoped_array<char> buffer(new char[count + 1]);

  // Read it.
  size_t count_read = ::fread(reinterpret_cast<void*>(buffer.get()),
                              sizeof(char),
                              count,
                              input_);

  // Make sure it's null terminated.
  (buffer.get())[count_read] = 0;
  return std::string(buffer.get());
}

std::string FileTextReader::ReadLine() {
  // Find out where we are in the stream.
  int original_pos = ::ftell(input_);

  // Find the occurance of the next eol character, regardless of what
  // kind of terminator it is.
  static const char linefeed = '\n';
  static const char carriage_return = '\r';
  int eol_pos = -1;
  int eol_len = 1;
  while (!IsAtEnd()) {
    char eol = ::fgetc(input_);
    if (eol == linefeed) {
      eol_pos = ::ftell(input_);
      break;
    }
    if (eol == carriage_return) {
      eol_pos = ::ftell(input_);
      if (IsAtEnd()) {
        break;
      }
      eol = ::fgetc(input_);
      if (eol == linefeed) {
        eol_len = 2;
      }
      break;
    }
  }

  // Go back to where we started in the stream.
  ::fseek(input_, original_pos, SEEK_SET);

  if (eol_pos > 0) {
    int count = eol_pos - original_pos - 1;
    std::string result = ReadString(count);
    // Strip off one EOL marker (of the appropriate length).
    ReadString(eol_len);
    return result;
  } else {
    // If there are no end of line markers in this file, just send
    // back the whole file.
    return ReadToEnd();
  }
}

std::string FileTextReader::ReadToEnd() {
  size_t remaining_size = GetRemainingSize();
  if (remaining_size > 0) {
    return ReadString(remaining_size);
  } else {
    return std::string("");
  }
}

size_t FileTextReader::GetFileSize() const {
  struct stat file_info;
#if defined(OS_WIN)
  int file_number = ::_fileno(input_);
#else
  int file_number = ::fileno(input_);
#endif
  ::fstat(file_number, &file_info);

  return file_info.st_size;
}

size_t FileTextReader::GetRemainingSize() const {
  // Find out where we are in the stream.
  int original_pos = ::ftell(input_);

  return GetFileSize() - original_pos;
}
}  // end namespace o3d
