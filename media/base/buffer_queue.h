// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// BufferQueue is a simple Buffer manager that handles requests for data
// while hiding Buffer boundaries, treating its internal queue of Buffers
// as a contiguous region.
//
// This class is not threadsafe and requires external locking.

#ifndef MEDIA_BASE_BUFFER_QUEUE_H_
#define MEDIA_BASE_BUFFER_QUEUE_H_

#include <deque>

#include "base/ref_counted.h"

namespace media {

class Buffer;

class BufferQueue {
 public:
  BufferQueue();
  ~BufferQueue();

  // Clears |queue_|.
  void Clear();

  // Advances front pointer |bytes_to_be_consumed| bytes and discards
  // "consumed" buffers.
  void Consume(size_t bytes_to_be_consumed);

  // Tries to copy |bytes| bytes from our data to |dest|. Returns the number
  // of bytes successfully copied.
  size_t Copy(uint8* dest, size_t bytes);

  // Enqueues |buffer_in| and adds a reference.
  void Enqueue(Buffer* buffer_in);

  // Returns true if the |queue_| is empty.
  bool IsEmpty();

  // Returns the number of bytes in the |queue_|.
  size_t SizeInBytes();

 private:
  // Queued audio data.
  std::deque< scoped_refptr<Buffer> > queue_;

  // Remembers the amount of remaining audio data in the front buffer.
  size_t data_offset_;

  // Keeps track of the |queue_| size in bytes.
  size_t size_in_bytes_;

  DISALLOW_COPY_AND_ASSIGN(BufferQueue);
};

}  // namespace media

#endif  // MEDIA_BASE_BUFFER_QUEUE_H_
