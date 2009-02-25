// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines various types of timestamped media buffers used for transporting
// data between filters.  Every buffer contains a timestamp in microseconds
// describing the relative position of the buffer within the media stream, and
// the duration in microseconds for the length of time the buffer will be
// rendered.
//
// Timestamps are derived directly from the encoded media file and are commonly
// known as the presentation timestamp (PTS).  Durations are a best-guess and
// are usually derived from the sample/frame rate of the media file.
//
// Due to encoding and transmission errors, it is not guaranteed that timestamps
// arrive in a monotonically increasing order nor that the next timestamp will
// be equal to the previous timestamp plus the duration.
//
// In the ideal scenario for a 25fps movie, buffers are timestamped as followed:
//
//               Buffer0      Buffer1      Buffer2      ...      BufferN
// Timestamp:        0us      40000us      80000us      ...   (N*40000)us
// Duration*:    40000us      40000us      40000us      ...       40000us
//
//  *25fps = 0.04s per frame = 40000us per frame

#ifndef MEDIA_BASE_BUFFERS_H_
#define MEDIA_BASE_BUFFERS_H_

#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/time.h"

namespace media {

class StreamSample : public base::RefCountedThreadSafe<StreamSample> {
 public:
  // Returns the timestamp of this buffer in microseconds.
  base::TimeDelta GetTimestamp() const {
    return timestamp_;
  }

  // Returns the duration of this buffer in microseconds.
  base::TimeDelta GetDuration() const {
    return duration_;
  }

  // Indicates that the sample is the last one in the stream.
  bool IsEndOfStream() const {
    return end_of_stream_;
  }

  // Indicates that this sample is discontinuous from the previous one, for
  // example, following a seek.
  bool IsDiscontinuous() const {
    return discontinuous_;
  }

  // Sets the timestamp of this buffer in microseconds.
  void SetTimestamp(const base::TimeDelta& timestamp) {
    timestamp_ = timestamp;
  }

  // Sets the duration of this buffer in microseconds.
  void SetDuration(const base::TimeDelta& duration) {
    duration_ = duration;
  }

  // Sets the value returned by IsEndOfStream().
  void SetEndOfStream(bool end_of_stream) {
    end_of_stream_ = end_of_stream;
  }

  // Sets the value returned by IsDiscontinuous().
  void SetDiscontinuous(bool discontinuous) {
    discontinuous_ = discontinuous;
  }

 protected:
  friend class base::RefCountedThreadSafe<StreamSample>;
  StreamSample()
      : end_of_stream_(false),
        discontinuous_(false) {
  }
  virtual ~StreamSample() {}

  base::TimeDelta timestamp_;
  base::TimeDelta duration_;
  bool end_of_stream_;
  bool discontinuous_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StreamSample);
};


class Buffer : public StreamSample {
 public:
  // Returns a read only pointer to the buffer data.
  virtual const char* GetData() const = 0;

  // Returns the size of valid data in bytes.
  virtual size_t GetDataSize() const = 0;
};


class WritableBuffer : public Buffer  {
 public:
  // Returns a read-write pointer to the buffer data.
  virtual char* GetWritableData() = 0;

  // Updates the size of valid data in bytes, which must be less than or equal
  // to GetBufferSize.
  virtual void SetDataSize(size_t data_size) = 0;

  // Returns the maximum allocated size for this buffer.
  virtual size_t GetBufferSize() const = 0;
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


class VideoFrame : public StreamSample {
 public:
  // Locks the underlying surface and fills out the given VideoSurface and
  // returns true if successful, false otherwise.  Any additional calls to Lock
  // will fail.
  virtual bool Lock(VideoSurface* surface) = 0;

  // Unlocks the underlying surface, the VideoSurface acquired from Lock is no
  // longer guaranteed to be valid.
  virtual void Unlock() = 0;
};


// An interface for receiving the results of an asynchronous read.  Downstream
// filters typically implement this interface or use AssignableBuffer and
// provide it to upstream filters as a read request.  When the upstream filter
// has completed the read, they call SetBuffer/OnAssignment to notify the
// downstream filter.
//
// TODO(scherkus): rethink the Assignable interface -- it's a bit kludgy.
template <class BufferType>
class Assignable
    : public base::RefCountedThreadSafe< Assignable<BufferType> > {
 public:
  // Assigns a buffer to the owner.
  virtual void SetBuffer(BufferType* buffer) = 0;

  // Notifies the owner that an assignment has been completed.
  virtual void OnAssignment() = 0;

  // TODO(scherkus): figure out a solution to friending a template.
  // See http://www.comeaucomputing.com/techtalk/templates/#friendclassT for
  // an explanation.
  // protected:
  // friend class base::RefCountedThreadSafe< Assignable<class T> >;
  virtual ~Assignable() {}
};


// Template for easily creating Assignable buffers.  Pass in the pointer of the
// object to receive the OnAssignment callback.
template <class OwnerType, class BufferType>
class AssignableBuffer : public Assignable<BufferType> {
 public:
  explicit AssignableBuffer(OwnerType* owner)
      : owner_(owner),
        buffer_(NULL) {
    DCHECK(owner_);
  }

  // AssignableBuffer<BufferType> implementation.
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

}  // namespace media

#endif  // MEDIA_BASE_BUFFERS_H_
