// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/buffer_queue.h"

#include "media/base/buffers.h"

namespace media {

BufferQueue::BufferQueue()
    : data_offset_(0),
      size_in_bytes_(0) {
}

BufferQueue::~BufferQueue() {
}

void BufferQueue::Consume(size_t bytes_to_be_consumed) {
  // Make sure user isn't trying to consume more than we have.
  DCHECK(size_in_bytes_ >= bytes_to_be_consumed);

  // As we have enough data to consume, adjust |size_in_bytes_|.
  size_in_bytes_ -= bytes_to_be_consumed;

  // Now consume them.
  while (bytes_to_be_consumed > 0) {
    // Calculate number of usable bytes in the front of the |queue_|.
    size_t front_remaining = queue_.front()->GetDataSize() - data_offset_;

    // If there is enough data in our first buffer to advance into it, do so.
    // Otherwise, advance into the queue.
    if (front_remaining > bytes_to_be_consumed) {
      data_offset_ += bytes_to_be_consumed;
      bytes_to_be_consumed = 0;
    } else {
      data_offset_ = 0;
      queue_.pop_front();
      bytes_to_be_consumed -= front_remaining;
    }
  }
}

size_t BufferQueue::Copy(uint8* dest, size_t bytes) {
  if (bytes == 0)
    return 0;

  DCHECK(!queue_.empty());

  size_t current_remaining = 0;
  const uint8* current = NULL;
  size_t copied = 0;

  for (size_t i = 0; i < queue_.size() && bytes > 0; ++i) {
    // Calculate number of usable bytes in the front of the |queue_|. Special
    // case for front due to |data_offset_|.
    if (i == 0) {
      current_remaining = queue_.front()->GetDataSize() - data_offset_;
      current = queue_.front()->GetData() + data_offset_;
    } else {
      current_remaining = queue_[i]->GetDataSize();
      current = queue_[i]->GetData();
    }

    // Prevent writing over the end of the buffer.
    if (current_remaining > bytes)
      current_remaining = bytes;

    memcpy(dest + copied, current, current_remaining);

    // Modify counts and pointers.
    copied += current_remaining;
    bytes -= current_remaining;
  }
  return copied;
}

void BufferQueue::Enqueue(Buffer* buffer_in) {
  queue_.push_back(buffer_in);
  size_in_bytes_ += buffer_in->GetDataSize();
}

void BufferQueue::Clear() {
  queue_.clear();
  size_in_bytes_ = 0;
  data_offset_ = 0;
}

bool BufferQueue::IsEmpty() {
  // Since we keep track of the number of bytes, this is easier than calling
  // into |queue_|.
  return size_in_bytes_ == 0;
}

size_t BufferQueue::SizeInBytes() {
  return size_in_bytes_;
}

}  // namespace media
