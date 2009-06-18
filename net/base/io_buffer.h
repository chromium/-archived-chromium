// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_IO_BUFFER_H_
#define NET_BASE_IO_BUFFER_H_

#include "base/ref_counted.h"

namespace net {

// This is a simple wrapper around a buffer that provides ref counting for
// easier asynchronous IO handling.
class IOBuffer : public base::RefCountedThreadSafe<IOBuffer> {
 public:
  IOBuffer() : data_(NULL) {}
  explicit IOBuffer(int buffer_size);
  virtual ~IOBuffer() {
    delete[] data_;
  }

  char* data() { return data_; }

 protected:
  // Only allow derived classes to specify data_.
  // In all other cases, we own data_, and must delete it at destruction time.
  explicit IOBuffer(char* data) : data_(data) {}
  char* data_;
};

// This class allows the creation of a temporary IOBuffer that doesn't really
// own the underlying buffer. Please use this class only as a last resort.
// A good example is the buffer for a synchronous operation, where we can be
// sure that nobody is keeping an extra reference to this object so the lifetime
// of the buffer can be completely managed by its intended owner.
class WrappedIOBuffer : public net::IOBuffer {
 public:
  explicit WrappedIOBuffer(const char* data)
      : net::IOBuffer(const_cast<char*>(data)) {}
  ~WrappedIOBuffer() {
    data_ = NULL;
  }
};

}  // namespace net

#endif  // NET_BASE_IO_BUFFER_H_
