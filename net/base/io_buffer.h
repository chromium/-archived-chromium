// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_IO_BUFFER_H_
#define NET_BASE_IO_BUFFER_H_

#include "base/logging.h"
#include "base/ref_counted.h"

namespace net {

// This is a simple wrapper around a buffer that provides ref counting for
// easier asynchronous IO handling.
class IOBuffer : public base::RefCountedThreadSafe<IOBuffer> {
 public:
  IOBuffer() : data_(NULL) {}
  explicit IOBuffer(int buffer_size) {
    DCHECK(buffer_size);
    data_ = new char[buffer_size];
  }
  explicit IOBuffer(char* buffer) : data_(buffer) {}
  virtual ~IOBuffer() {
    delete[] data_;
  }

  char* data() { return data_; }

 protected:
  char* data_;
};

}  // namespace net

#endif  // NET_BASE_IO_BUFFER_H_
