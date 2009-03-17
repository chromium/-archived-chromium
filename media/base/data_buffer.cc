// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "media/base/data_buffer.h"

namespace media {

DataBuffer::DataBuffer()
    : data_(NULL),
      buffer_size_(0),
      data_size_(0) {
}

DataBuffer::~DataBuffer() {
  delete [] data_;
}

const uint8* DataBuffer::GetData() const {
  return data_;
}

size_t DataBuffer::GetDataSize() const {
  return data_size_;
}

uint8* DataBuffer::GetWritableData(size_t buffer_size) {
  if (buffer_size > buffer_size_) {
    delete [] data_;
    data_ = new uint8[buffer_size];
    if (!data_) {
      NOTREACHED();
      buffer_size = 0;
    }
    buffer_size_ = buffer_size;
  }
  data_size_ = buffer_size;
  return data_;
}


void DataBuffer::SetDataSize(size_t data_size) {
  DCHECK(data_size <= buffer_size_);
  data_size_ = data_size;
}

}  // namespace media
