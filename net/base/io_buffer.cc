// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/io_buffer.h"

#include "base/logging.h"

namespace net {

IOBuffer::IOBuffer(int buffer_size) {
  DCHECK(buffer_size);
  data_ = new char[buffer_size];
}

}  // namespace net
