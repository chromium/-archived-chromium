// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// DnsQueue is implemented as an almost FIFO circular buffer for text
// strings that don't have embedded nulls ('\0').  The "almost" element is that
// some duplicate strings may be removed (i.e., the string won't really be
// pushed *if* the class happens to notice that a duplicate is already in the
// queue).
// The buffers internal format is null terminated character strings
// (a.k.a., c_strings).
// It is written to be as fast as possible during push() operations, so
// that there will be minimal performance impact on a supplier thread.
// The push() operation will not block, and no memory allocation is involved
// (internally) during the push operations.
// The one caveat is that if there is insufficient space in the buffer to
// accept additional string via a push(), then the push() will fail, and
// the buffer will be unmodified.

// This class was designed for use in DNS prefetch operations.  During
// rendering, the supplier is the renderer (typically), and the consumer
// is a thread that sends messages to an async DNS resolver.

#ifndef CHROME_RENDERER_NET_RENDER_DNS_QUEUE_H__
#define CHROME_RENDERER_NET_RENDER_DNS_QUEUE_H__

#include <string>

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/scoped_ptr.h"

class DnsQueue {
 public:
  // BufferSize is a signed type used for indexing into a buffer.
  typedef int32 BufferSize;

  enum PushResult { SUCCESSFUL_PUSH, OVERFLOW_PUSH, REDUNDANT_PUSH };

  // The size specified in the constructor creates a buffer large enough
  // to hold at most one string of that length, or "many"
  // strings of considerably shorter length.  Note that strings
  // are padded internally with a terminal '\0" while stored,
  // so if you are trying to be precise and get N strings of
  // length K to fit, you should actually construct a buffer with
  // an internal size of N*(K+1).
  explicit DnsQueue(BufferSize size);
  ~DnsQueue(void);

  size_t Size() const { return size_; }
  void Clear() {
    size_ = 0;
    readable_ = writeable_;
    Validate();
  }

  // Push takes an unterminated string of the given length
  // and inserts it into the queue for later
  // extraction by read.  For each successful push(), there
  // can later be a corresponding read() to extracted the text.
  // The string must not contain an embedded null terminator
  // Exactly length chars are written, or the push fails (where
  // "fails" means nothing is written).
  // Returns true for success, false for failure (nothing written).
  PushResult Push(const char* source, const size_t length);

  PushResult Push(std::string source) {
    return Push(source.c_str(), source.length());
  }

  // Extract the next available string from the buffer.
  // If the buffer is empty, then return false.
  bool Pop(std::string* out_string);

 private:
  bool Validate();  // Checks that all internal data is valid.

  const scoped_array<char> buffer_;  // Circular buffer, plus extra char ('\0').
  const BufferSize buffer_size_;  // Size one smaller than allocated space.
  const BufferSize buffer_sentinel_;  // Index of extra '\0' at end of buffer_.

  // If writable_ == readable_, then the buffer is empty.
  BufferSize readable_;  // Next readable char in buffer_.
  BufferSize writeable_;  // The next space in buffer_ to push.

  // Number of queued strings
  size_t size_;

  DISALLOW_EVIL_CONSTRUCTORS(DnsQueue);
};  // class DnsQueue

#endif  // CHROME_RENDERER_NET_RENDER_DNS_QUEUE_H__
