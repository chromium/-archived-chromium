// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple implementation of WritableBufferInterface that takes ownership of
// the given data pointer.
//
// DataBuffer assumes that memory was allocated with new char[].

#ifndef MEDIA_BASE_DATA_BUFFER_H_
#define MEDIA_BASE_DATA_BUFFER_H_

#include "media/base/buffers.h"

namespace media {

class DataBuffer : public WritableBufferInterface {
 public:
  DataBuffer(char* data, size_t buffer_size, size_t data_size,
             int64 timestamp, int64 duration);

  // StreamSampleInterface
  virtual int64 GetTimestamp() const;
  virtual void SetTimestamp(int64 timestamp);
  virtual int64 GetDuration() const;
  virtual void SetDuration(int64 duration);

  // BufferInterface
  virtual const char* GetData() const;
  virtual size_t GetDataSize() const;

  // WritableBufferInterface
  virtual char* GetWritableData();
  virtual size_t GetBufferSize() const;
  virtual void SetDataSize(size_t data_size);

 protected:
  virtual ~DataBuffer();

 private:
  char* data_;
  size_t buffer_size_;
  size_t data_size_;
  int64 timestamp_;
  int64 duration_;
};

}  // namespace media

#endif  // MEDIA_BASE_DATA_BUFFER_H_
