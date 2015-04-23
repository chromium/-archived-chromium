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

// This version stores the size of the buffer so that the creator of the object
// doesn't have to keep track of that value.
// NOTE: This doesn't mean that we want to stop sending the size as an explictit
// argument to IO functions. Please keep using IOBuffer* for API declarations.
class IOBufferWithSize : public IOBuffer {
 public:
  explicit IOBufferWithSize(int size) : IOBuffer(size), size_(size) {}
  ~IOBufferWithSize() {}

  int size() const { return size_; }

 private:
  int size_;
};

// This version allows the caller to do multiple IO operations reusing a given
// IOBuffer. We don't own data_, we simply make it point to the buffer of the
// passed in IOBuffer, plus the desired offset.
class ReusedIOBuffer : public IOBuffer {
 public:
  ReusedIOBuffer(IOBuffer* base, int size)
      : IOBuffer(base->data()), base_(base), size_(size) {}
  ~ReusedIOBuffer() {
    // We don't really own a buffer.
    data_ = NULL;
  }

  int size() const { return size_; }
  void SetOffset(int offset);

 private:
  scoped_refptr<IOBuffer> base_;
  int size_;
};

// This class allows the creation of a temporary IOBuffer that doesn't really
// own the underlying buffer. Please use this class only as a last resort.
// A good example is the buffer for a synchronous operation, where we can be
// sure that nobody is keeping an extra reference to this object so the lifetime
// of the buffer can be completely managed by its intended owner.
class WrappedIOBuffer : public IOBuffer {
 public:
  explicit WrappedIOBuffer(const char* data)
      : IOBuffer(const_cast<char*>(data)) {}
  ~WrappedIOBuffer() {
    data_ = NULL;
  }
};

}  // namespace net

#endif  // NET_BASE_IO_BUFFER_H_
