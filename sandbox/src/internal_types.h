// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
