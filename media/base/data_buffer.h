// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple implementation of WritableBuffer that takes ownership of
// the given data pointer.
//
// DataBuffer assumes that memory was allocated with new char[].

#ifndef MEDIA_BASE_DATA_BUFFER_H_
#define MEDIA_BASE_DATA_BUFFER_H_

#include "media/base/buffers.h"

namespace media {

class DataBuffer : public WritableBuffer {
 public:
  DataBuffer(char* data, size_t buffer_size, size_t data_size,
             const base::TimeDelta& timestamp, const base::TimeDelta& duration);

  // Buffer implementation.
  virtual const char* GetData() const;
  virtual size_t GetDataSize() const;

  // WritableBuffer implementation.
  virtual char* GetWritableData();
  virtual size_t GetBufferSize() const;
  virtual void SetDataSize(size_t data_size);

 protected:
  virtual ~DataBuffer();

 private:
  char* data_;
  size_t buffer_size_;
  size_t data_size_;
};

}  // namespace media

#endif  // MEDIA_BASE_DATA_BUFFER_H_
