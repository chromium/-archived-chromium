// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_BUFFERS_H_
#define MEDIA_BASE_BUFFERS_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"

namespace media {

// NOTE: this isn't a true interface since RefCountedThreadSafe has non-virtual
// members, therefore implementors should NOT subclass RefCountedThreadSafe.
//
// If you do, AddRef/Release will have different outcomes depending on the
// current type of the pointer (StreamSampleInterface vs. SomeImplementation).
class StreamSampleInterface :
    public base::RefCountedThreadSafe<StreamSampleInterface> {
 public:
  virtual ~StreamSampleInterface() {}

  virtual int64 GetTimestamp() = 0;
  virtual int64 GetDuration() = 0;
  virtual void SetTimestamp(int64 timestamp) = 0;
  virtual void SetDuration(int64 duration) = 0;
};


class BufferInterface : public StreamSampleInterface {
 public:
  virtual ~BufferInterface() {}

  // Returns a read only pointer to the buffer data.
  virtual const char* GetData() = 0;

  // Returns the size of valid data in bytes.
  virtual size_t GetDataSize() = 0;
};


class WritableBufferInterface : public BufferInterface  {
 public:
  virtual ~WritableBufferInterface() {}

  // Returns a read-write pointer to the buffer data.
  virtual char* GetWritableData() = 0;

  // Updates the size of valid data in bytes, which must be less than or equal
  // to GetBufferSize.
  virtual void SetDataSize(size_t data_size) = 0;

  // Returns the maximum allocated size for this buffer.
  virtual size_t GetBufferSize() = 0;
};


struct VideoSurface {
  static const size_t kMaxPlanes = 3;

  // Surface formats roughly based on FOURCC labels, see:
  // http://www.fourcc.org/rgb.php
  // http://www.fourcc.org/yuv.php
  enum Format {
    RGB555,      // 16bpp RGB packed 5:5:5
    RGB565,      // 16bpp RGB packed 5:6:5
    RGB24,       // 24bpp RGB packed 8:8:8
    RGB32,       // 32bpp RGB packed with extra byte 8:8:8
    RGBA,        // 32bpp RGBA packed 8:8:8:8
    YV12,        // 12bpp YVU planar 1x1 Y, 2x2 VU samples
    YV16,        // 16bpp YVU planar 1x1 Y, 2x1 VU samples
  };

  // Surface format.
  Format format;

  // Width and height of surface.
  size_t width;
  size_t height;

  // Number of planes, typically 1 for packed RGB formats and 3 for planar
  // YUV formats.
  size_t planes;

  // Array of strides for each plane, typically greater or equal to the width
  // of the surface divided by the horizontal sampling period.
  size_t strides[kMaxPlanes];

  // Array of data pointers to each plane.
  char* data[kMaxPlanes];
};


class VideoFrameInterface : public StreamSampleInterface {
 public:
  virtual ~VideoFrameInterface() {}

  // Locks the underlying surface and fills out the given VideoSurface and
  // returns true if successful, false otherwise.
  virtual bool Lock(VideoSurface* surface) = 0;

  // Unlocks the underlying surface, any previous VideoSurfaces are no longer
  // guaranteed to be valid.
  virtual void Unlock() = 0;
};


template <class BufferType>
class AssignableInterface {
 public:
  virtual ~AssignableInterface() {}

  // Assigns a buffer to the owner.
  virtual void SetBuffer(BufferType* buffer) = 0;

  // Notifies the owner that an assignment has been completed.
  virtual void OnAssignment() = 0;
};


// Template for easily creating AssignableInterface buffers.  Pass in the
// pointer of the object to receive the OnAssignment callback.
template <class OwnerType, class BufferType>
class AssignableBuffer : public AssignableInterface<BufferType>,
    public base::RefCountedThreadSafe<AssignableBuffer<OwnerType, BufferType> > {
 public:
  explicit AssignableBuffer(OwnerType* owner)
    : owner_(owner),
      buffer_(NULL) {
    DCHECK(owner_);
  }
  virtual ~AssignableBuffer() {}

  // AssignableBufferInterface<BufferType>
  virtual void SetBuffer(BufferType* buffer) {
    buffer_ = buffer;
  }

  virtual void OnAssignment() {
    owner_->OnAssignment(buffer_.get());
  }

 private:
  OwnerType* owner_;
  scoped_refptr<BufferType> buffer_;

  DISALLOW_COPY_AND_ASSIGN(AssignableBuffer);
};

} // namespace media

#endif // MEDIA_BASE_BUFFERS_H_
