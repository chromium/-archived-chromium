// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// SeekableBuffer to support backward and forward seeking in a buffer for
// reading a media data source.
//
// In order to support backward and forward seeking, this class buffers data in
// both backward and forward directions, the current read position can be reset
// to anywhere in the buffered data.
//
// The amount of data buffered is regulated by two variables at construction,
// |backward_capacity| and |forward_capacity|.
//
// In the case of reading and seeking forward, the current read position
// advances and there will be more data in the backward direction. If backward
// bytes exceeds |backward_capacity|, the exceeding bytes are evicted and thus
// backward_bytes() will always be less than or equal to |backward_capacity|.
// The eviction will be caused by Read() and Seek() in the forward direction and
// is done internally when the mentioned criteria is fulfilled.
//
// In the case of appending data to the buffer, there is an advisory limit of
// how many bytes can be kept in the forward direction, regulated by
// |forward_capacity|. The append operation (by calling Append()) that caused
// forward bytes to exceed |forward_capacity| will have a return value that
// advises a halt of append operation, further append operations are allowed but
// are not advised. Since this class is used as a backend buffer for caching
// media files downloaded from network we cannot afford losing data, we can
// only advise a halt of further writing to this buffer.
// This class is not inherently thread-safe. Concurrent access must be
// externally serialized.

#ifndef MEDIA_BASE_SEEKABLE_BUFFER_H_
#define MEDIA_BASE_SEEKABLE_BUFFER_H_

#include <list>

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/scoped_ptr.h"

namespace media {

class SeekableBuffer {
 public:
  // Constructs an instance with |forward_capacity| and |backward_capacity|.
  // The values are in bytes.
  SeekableBuffer(size_t backward_capacity, size_t forward_capacity);

  ~SeekableBuffer();

  // Reads a maximum of |size| bytes into |buffer| from the current read
  // position. Returns the number of bytes read.
  // The current read position will advance by the amount of bytes read. If
  // reading caused backward_bytes() to exceed backward_capacity(), an eviction
  // of the backward buffer will be done internally.
  size_t Read(size_t size, uint8* buffer);

  // Appends |data| with |size| bytes to this buffer. If this buffer becomes
  // full or is already full then returns false, otherwise returns true.
  // Append operations are always successful. A return value of false only means
  // that forward_bytes() is greater than or equals to forward_capacity(). Data
  // appended is still in this buffer but user is advised not to write any more.
  bool Append(size_t size, const uint8* data);

  // Moves the read position by |offset| bytes. If |offset| is positive, the
  // current read position is moved forward. If negative, the current read
  // position is moved backward. A zero |offset| value will keep the current
  // read position stationary.
  // If |offset| exceeds bytes buffered in either direction, reported by
  // forward_bytes() when seeking forward and backward_bytes() when seeking
  // backward, the seek operation will fail and return value will be false.
  // If the seek operation fails, the current read position will not be updated.
  // If a forward seeking caused backward_bytes() to exceed backward_capacity(),
  // this method call will cause an eviction of the backward buffer.
  bool Seek(int32 offset);

  // Returns the number of bytes buffered beyond the current read position.
  size_t forward_bytes() const { return forward_bytes_; }

  // Returns the number of bytes buffered that precedes the current read
  // position.
  size_t backward_bytes() const { return backward_bytes_; }

  // Returns the maximum number of bytes that should be kept in the forward
  // direction.
  size_t forward_capacity() const { return forward_capacity_; }

  // Returns the maximum number of bytes that should be kept in the backward
  // direction.
  size_t backward_capacity() const { return backward_capacity_; }

 private:
  // A structure that contains a block of data.
  struct Buffer {
    explicit Buffer(size_t len) : data(new uint8[len]), size(len) {}
    // Pointer to data.
    scoped_array<uint8> data;
    // Size of this block.
    size_t size;
  };

  // Definition of the buffer queue.
  typedef std::list<Buffer*> BufferQueue;

  // A helper method to evict buffers in the backward direction until backward
  // bytes is within the backward capacity.
  void EvictBackwardBuffers();

  // An internal method shared by Read() and SeekForward() that actually does
  // reading. It reads a maximum of |size| bytes into |data|. Returns the number
  // of bytes read. The current read position will be moved forward by the
  // number of bytes read. If |data| is NULL, only the current read position
  // will advance but no data will be copied.
  size_t InternalRead(size_t size, uint8* data);

  // A helper method that moves the current read position forward by |size|
  // bytes.
  // If the return value is true, the operation completed successfully.
  // If the return value is false, |size| is greater than forward_bytes() and
  // the seek operation failed. The current read position is not updated.
  bool SeekForward(size_t size);

  // A helper method that moves the current read position backward by |size|
  // bytes.
  // If the return value is true, the operation completed successfully.
  // If the return value is false, |size| is greater than backward_bytes() and
  // the seek operation failed. The current read position is not updated.
  bool SeekBackward(size_t size);

  BufferQueue::iterator current_buffer_;
  BufferQueue buffers_;
  size_t current_buffer_offset_;

  size_t backward_capacity_;
  size_t backward_bytes_;

  size_t forward_capacity_;
  size_t forward_bytes_;
};

}  // namespace media

#endif  // MEDIA_BASE_SEEKABLE_BUFFER_H_
