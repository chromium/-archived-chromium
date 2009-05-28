// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/seekable_buffer.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stl_util-inl.h"

namespace media {

SeekableBuffer::SeekableBuffer(size_t backward_capacity,
                               size_t forward_capacity)
    : current_buffer_offset_(0),
      backward_capacity_(backward_capacity),
      backward_bytes_(0),
      forward_capacity_(forward_capacity),
      forward_bytes_(0) {
  current_buffer_ = buffers_.begin();
}

SeekableBuffer::~SeekableBuffer() {
  STLDeleteElements(&buffers_);
}

size_t SeekableBuffer::Read(size_t size, uint8* data) {
  DCHECK(data);
  return InternalRead(size, data);
}

bool SeekableBuffer::Append(size_t size, const uint8* data) {
  // Since the forward capacity is only used to check the criteria for buffer
  // full, we always append data to the buffer.
  Buffer* buffer = new Buffer(size);
  memcpy(buffer->data.get(), data, size);
  buffers_.push_back(buffer);

  // After we have written the first buffer, update |current_buffer_| to point
  // to it.
  if (current_buffer_ == buffers_.end()) {
    DCHECK_EQ(0u, forward_bytes_);
    current_buffer_ = buffers_.begin();
  }

  // Update the |forward_bytes_| counter since we have more bytes.
  forward_bytes_ += size;

  // Advise the user to stop append if the amount of forward bytes exceeds
  // the forward capacity. A false return value means the user should stop
  // appending more data to this buffer.
  if (forward_bytes_ >= forward_capacity_)
    return false;
  return true;
}

bool SeekableBuffer::Seek(int32 offset) {
  if (offset > 0)
    return SeekForward(offset);
  else if (offset < 0)
    return SeekBackward(-offset);
  return true;
}

bool SeekableBuffer::SeekForward(size_t size) {
  // Perform seeking forward only if we have enough bytes in the queue.
  if (size > forward_bytes_)
    return false;

  // Do a read of |size| bytes.
  size_t taken = InternalRead(size, NULL);
  DCHECK_EQ(taken, size);
  return true;
}

bool SeekableBuffer::SeekBackward(size_t size) {
  if (size > backward_bytes_)
    return false;
  // Record the number of bytes taken.
  size_t taken = 0;
  // Loop until we taken enough bytes and rewind by the desired |size|.
  while (taken < size) {
    // |current_buffer_| can never be invalid when we are in this loop. It can
    // only be invalid before any data is appended. The invalid case should be
    // handled by checks before we enter this loop.
    DCHECK(current_buffer_ != buffers_.end());

    // We try to consume at most |size| bytes in the backward direction. We also
    // have to account for the offset we are in the current buffer, so take the
    // minimum between the two to determine the amount of bytes to take from the
    // current buffer.
    size_t consumed = std::min(size - taken, current_buffer_offset_);

    // Decreases the offset in the current buffer since we are rewinding.
    current_buffer_offset_ -= consumed;

    // Increase the amount of bytes taken in the backward direction. This
    // determines when to stop the loop.
    taken += consumed;

    // Forward bytes increases and backward bytes decreases by the amount
    // consumed in the current buffer.
    forward_bytes_ += consumed;
    backward_bytes_ -= consumed;
    DCHECK_GE(backward_bytes_, 0u);

    // The current buffer pointed by current iterator has been consumed. Move
    // the iterator backward so it points to the previous buffer.
    if (current_buffer_offset_ == 0) {
      if (current_buffer_ == buffers_.begin())
        break;
      // Move the iterator backward.
      --current_buffer_;
      // Set the offset into the current buffer to be the buffer size as we
      // are preparing for rewind for next iteration.
      current_buffer_offset_ = (*current_buffer_)->size;
    }
  }
  DCHECK_EQ(taken, size);
  return true;
}

void SeekableBuffer::EvictBackwardBuffers() {
  // Advances the iterator until we hit the current pointer.
  while (backward_bytes_ > backward_capacity_) {
    BufferQueue::iterator i = buffers_.begin();
    if (i == current_buffer_)
      break;
    Buffer* buffer = *i;
    backward_bytes_ -= buffer->size;
    DCHECK_GE(backward_bytes_, 0u);

    delete buffer;
    buffers_.erase(i);
  }
}

size_t SeekableBuffer::InternalRead(size_t size, uint8* data) {
  // Counts how many bytes are actually read from the buffer queue.
  size_t taken = 0;

  while (taken < size) {
    // |current_buffer_| is valid since the first time this buffer is appended
    // with data.
    if (current_buffer_ == buffers_.end()) {
      DCHECK_EQ(0u, forward_bytes_);
      break;
    }
    Buffer* buffer = *current_buffer_;

    // Find the right amount to copy from the current buffer referenced by
    // |buffer|. We shall copy no more than |size| bytes in total and each
    // single step copied no more than the current buffer size.
    size_t copied = std::min(size - taken,
                             buffer->size - current_buffer_offset_);

    // |data| is NULL if we are seeking forward, so there's no need to copy.
    if (data)
      memcpy(data + taken, buffer->data.get() + current_buffer_offset_, copied);

    // Increase total number of bytes copied, which regulates when to end this
    // loop.
    taken += copied;

    // We have read |copied| bytes from the current buffer. Advances the offset.
    current_buffer_offset_ += copied;

    // We have less forward bytes and more backward bytes. Updates these
    // counters by |copied|.
    forward_bytes_ -= copied;
    backward_bytes_ += copied;
    DCHECK_GE(forward_bytes_, 0u);

    // The buffer has been consumed.
    if (current_buffer_offset_ == buffer->size) {
      BufferQueue::iterator next = current_buffer_;
      ++next;
      // If we are at the last buffer, don't advance.
      if (next == buffers_.end())
        break;

      // Advances the iterator.
      current_buffer_ = next;
      current_buffer_offset_ = 0;
    }
  }
  EvictBackwardBuffers();
  return taken;
}

}  // namespace media
