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

#ifndef SANDBOX_SRC_INTERNAL_TYPES_H__
#define SANDBOX_SRC_INTERNAL_TYPES_H__

namespace sandbox {

const wchar_t kNtdllName[] = L"ntdll.dll";
const wchar_t kKerneldllName[] = L"kernel32.dll";

// Defines the supported C++ types encoding to numeric id. Like a poor's man
// RTTI. Note that true C++ RTTI will not work because the types are not
// polymorphic anyway.
enum ArgType {
  INVALID_TYPE = 0,
  WCHAR_TYPE,
  ULONG_TYPE,
  UNISTR_TYPE,
  VOIDPTR_TYPE,
  INPTR_TYPE,
  INOUTPTR_TYPE,
  LAST_TYPE
};

// Encapsulates a pointer to a buffer and the size of the buffer.
class CountedBuffer {
 public:
  CountedBuffer(void* buffer, size_t size) : size_(size), buffer_(buffer) {}

  size_t Size() const {
    return size_;
  }

  void* Buffer() const {
    return buffer_;
  }

 private:
  size_t size_;
  void* buffer_;
};

}  // namespace sandbox

#endif  // SANDBOX_SRC_INTERNAL_TYPES_H__
