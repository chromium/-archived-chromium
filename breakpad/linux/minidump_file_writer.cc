// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
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

// minidump_file_writer.cc: Minidump file writer implementation.
//
// See minidump_file_writer.h for documentation.

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "breakpad/linux/linux_syscall_support.h"
#include "breakpad/linux/linux_libc_support.h"
#include "client/minidump_file_writer-inl.h"
#include "common/string_conversion.h"

namespace google_breakpad {

const MDRVA MinidumpFileWriter::kInvalidMDRVA = static_cast<MDRVA>(-1);

MinidumpFileWriter::MinidumpFileWriter() : file_(-1), position_(0), size_(0) {
}

MinidumpFileWriter::~MinidumpFileWriter() {
  Close();
}

bool MinidumpFileWriter::Open(const char *path) {
  assert(file_ == -1);
  file_ = sys_open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);

  return file_ != -1;
}

bool MinidumpFileWriter::Close() {
  bool result = true;

  if (file_ != -1) {
    ftruncate(file_, position_);
    result = (sys_close(file_) == 0);
    file_ = -1;
  }

  return result;
}

bool MinidumpFileWriter::CopyStringToMDString(const wchar_t *str,
                                              unsigned int length,
                                              TypedMDRVA<MDString> *mdstring) {
  bool result = true;
  if (sizeof(wchar_t) == sizeof(u_int16_t)) {
    // Shortcut if wchar_t is the same size as MDString's buffer
    result = mdstring->Copy(str, mdstring->get()->length);
  } else {
    u_int16_t out[2];
    int out_idx = 0;
    
    // Copy the string character by character
    while (length && result) {
      UTF32ToUTF16Char(*str, out);
      if (!out[0])
        return false;
      
      // Process one character at a time
      --length;
      ++str;
      
      // Append the one or two UTF-16 characters.  The first one will be non-
      // zero, but the second one may be zero, depending on the conversion from
      // UTF-32.
      int out_count = out[1] ? 2 : 1;
      int out_size = sizeof(u_int16_t) * out_count;
      result = mdstring->CopyIndexAfterObject(out_idx, out, out_size);
      out_idx += out_count;
    }
  }
  return result;
}

bool MinidumpFileWriter::CopyStringToMDString(const char *str,
                                              unsigned int length,
                                              TypedMDRVA<MDString> *mdstring) {
  bool result = true;
  u_int16_t out[2];
  int out_idx = 0;

  // Copy the string character by character
  while (length && result) {
    int conversion_count = UTF8ToUTF16Char(str, length, out);
    if (!conversion_count)
      return false;
    
    // Move the pointer along based on the nubmer of converted characters
    length -= conversion_count;
    str += conversion_count;
    
    // Append the one or two UTF-16 characters
    int out_count = out[1] ? 2 : 1;
    int out_size = sizeof(u_int16_t) * out_count;
    result = mdstring->CopyIndexAfterObject(out_idx, out, out_size);
    out_idx += out_count;
  }
  return result;
}

template <typename CharType>
bool MinidumpFileWriter::WriteStringCore(const CharType *str,
                                         unsigned int length,
                                         MDLocationDescriptor *location) {
  assert(str);
  assert(location);
  // Calculate the mdstring length by either limiting to |length| as passed in
  // or by finding the location of the NULL character.
  unsigned int mdstring_length = 0;
  if (!length)
    length = INT_MAX;
  for (; mdstring_length < length && str[mdstring_length]; ++mdstring_length)
    ;

  // Allocate the string buffer
  TypedMDRVA<MDString> mdstring(this);
  if (!mdstring.AllocateObjectAndArray(mdstring_length + 1, sizeof(u_int16_t)))
    return false;

  // Set length excluding the NULL and copy the string
  mdstring.get()->length = mdstring_length * sizeof(u_int16_t);
  bool result = CopyStringToMDString(str, mdstring_length, &mdstring);

  // NULL terminate
  if (result) {
    u_int16_t ch = 0;
    result = mdstring.CopyIndexAfterObject(mdstring_length, &ch, sizeof(ch));

    if (result)
      *location = mdstring.location();
  }

  return result;
}

bool MinidumpFileWriter::WriteString(const wchar_t *str, unsigned int length,
                 MDLocationDescriptor *location) {
  return WriteStringCore(str, length, location);
}

bool MinidumpFileWriter::WriteString(const char *str, unsigned int length,
                 MDLocationDescriptor *location) {
  return WriteStringCore(str, length, location);
}

bool MinidumpFileWriter::WriteMemory(const void *src, size_t size,
                                     MDMemoryDescriptor *output) {
  assert(src);
  assert(output);
  UntypedMDRVA mem(this);

  if (!mem.Allocate(size))
    return false;
  if (!mem.Copy(src, mem.size()))
    return false;

  output->start_of_memory_range = reinterpret_cast<u_int64_t>(src);
  output->memory = mem.location();

  return true;
}

MDRVA MinidumpFileWriter::Allocate(size_t size) {
  assert(size);
  assert(file_ != -1);
  size_t aligned_size = (size + 7) & ~7;  // 64-bit alignment

  if (position_ + aligned_size > size_) {
    size_t growth = aligned_size;
    size_t minimal_growth = getpagesize();

    // Ensure that the file grows by at least the size of a memory page
    if (growth < minimal_growth)
      growth = minimal_growth;

    size_t new_size = size_ + growth;
    if (ftruncate(file_, new_size) != 0)
      return kInvalidMDRVA;

    size_ = new_size;
  }

  MDRVA current_position = position_;
  position_ += static_cast<MDRVA>(aligned_size);

  return current_position;
}

bool MinidumpFileWriter::Copy(MDRVA position, const void *src, ssize_t size) {
  assert(src);
  assert(size);
  assert(file_ != -1);

  // Ensure that the data will fit in the allocated space
  if (size + position > size_)
    return false;

  // Seek and write the data
  if (sys_lseek(file_, position, SEEK_SET) == static_cast<off_t>(position)) {
    if (sys_write(file_, src, size) == size)
      return true;
  }

  return false;
}

bool UntypedMDRVA::Allocate(size_t size) {
  assert(size_ == 0);
  size_ = size;
  position_ = writer_->Allocate(size_);
  return position_ != MinidumpFileWriter::kInvalidMDRVA;
}

bool UntypedMDRVA::Copy(MDRVA position, const void *src, size_t size) {
  assert(src);
  assert(size);
  assert(position + size <= position_ + size_);
  return writer_->Copy(position, src, size);
}

}  // namespace google_breakpad
