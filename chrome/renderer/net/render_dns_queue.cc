// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// See header file for description of DnsQueue class

#include "chrome/renderer/net/render_dns_queue.h"

#include "base/logging.h"
#include "base/stats_counters.h"

DnsQueue::DnsQueue(BufferSize size)
    : buffer_(new char[size + 2]),
      buffer_size_(size + 1),
      buffer_sentinel_(size + 1),
      size_(0) {
  CHECK(0 < static_cast<BufferSize>(size + 3));  // Avoid overflow worries.
  buffer_[buffer_sentinel_] = '\0';  // Guard byte to help reading data.
  readable_ = writeable_ = 0;  // Buffer starts empty.
}

DnsQueue::~DnsQueue(void) {
}

// Push takes an unterminated string plus its length.
// The string must not contain a null terminator.
// Exactly length chars are written, or nothing is written.
// Returns true for success, false there was no room to push.
DnsQueue::PushResult DnsQueue::Push(const char* source,
                                    const size_t unsigned_length) {
  BufferSize length = static_cast<BufferSize>(unsigned_length);
  if (0 > length+1)  // Avoid overflows in conversion to signed.
    return OVERFLOW_PUSH;

  // To save on sites with a LOT of links to the SAME domain, we have a
  // a compaction hack that removes duplicates when we try to push() a
  // match with the last push.
  if (0 < size_ && readable_ + length < buffer_sentinel_ &&
    0 == strncmp(source, &buffer_[readable_], unsigned_length) &&
    '\0' == buffer_[readable_ + unsigned_length]) {
    SIMPLE_STATS_COUNTER(L"DNS.PrefetchDnsRedundantPush");

    // We already wrote this name to the queue, so we'll skip this repeat.
    return REDUNDANT_PUSH;
  }

  // Calling convention precludes nulls.
  DCHECK(!length || '\0' != source[length - 1]);

  DCHECK(Validate());

  BufferSize available_space = readable_ - writeable_;

  if (0 >= available_space) {
    available_space += buffer_size_;
  }

  if (length + 1 >= available_space) {
    SIMPLE_STATS_COUNTER(L"DNS.PrefetchDnsQueueFull");
    return OVERFLOW_PUSH;  // Not enough space to push.
  }

  BufferSize dest = writeable_;
  BufferSize space_till_wrap = buffer_sentinel_ - dest;
  if (space_till_wrap < length + 1) {
    // Copy until we run out of room at end of buffer.
    std::memcpy(&buffer_[dest], source, space_till_wrap);
    // Ensure caller didn't have embedded '\0' and also
    // ensure trailing sentinel was in place.
    DCHECK(space_till_wrap == strlen(&buffer_[dest]));  // Relies on sentinel.

    length -= space_till_wrap;
    source += space_till_wrap;
    dest = 0;  // Continue writing at start of buffer.
  }

  // Copy any remaining portion of source.
  std::memcpy(&buffer_[dest], source, length);
  DCHECK(dest + length < buffer_sentinel_);
  buffer_[dest + length] = '\0';  // We need termination in our buffer.
  DCHECK(length == strlen(&buffer_[dest]));  // Preclude embedded '\0'.

  dest += length + 1;
  if (dest == buffer_sentinel_)
    dest = 0;

  writeable_ = dest;
  size_++;
  DCHECK(Validate());
  return SUCCESSFUL_PUSH;
}

// Extracts the next available string from the buffer.
// The returned string is null terminated, and hence has length
// that is exactly one greater than the written string.
// If the buffer is empty, then the Pop and returns false.
bool DnsQueue::Pop(std::string* out_string) {
  DCHECK(Validate());
  // Sentinel will preclude memory reads beyond buffer's end.
  DCHECK('\0' == buffer_[buffer_sentinel_]);

  if (readable_ == writeable_) {
    return false;  // buffer was empty
  }

  // Constructor *may* rely on sentinel for null termination.
  (*out_string) = &buffer_[readable_];
  // Our sentinel_ at end of buffer precludes an overflow in cast.
  BufferSize first_fragment_size = static_cast<BufferSize> (out_string->size());

  BufferSize terminal_null;
  if (readable_ + first_fragment_size >= buffer_sentinel_) {
    // Sentinel was used, so we need the portion after the wrap.
    out_string->append(&buffer_[0]);  // Fragment at start of buffer.
    // Sentinel precludes overflow in cast to signed type.
    terminal_null = static_cast<BufferSize>(out_string->size())
                    - first_fragment_size;
  } else {
    terminal_null = readable_ + first_fragment_size;
  }
  DCHECK('\0' == buffer_[terminal_null]);

  BufferSize new_readable = terminal_null + 1;
  if (buffer_sentinel_ == new_readable)
    new_readable = 0;

  readable_ = new_readable;
  size_--;
  if (readable_ == writeable_ || 0 == size_) {
    // Queue is empty, so reset to start of buffer to help with peeking.
    readable_ = writeable_ = 0;
  }
  DCHECK(Validate());
  return true;
}

bool DnsQueue::Validate() {
  return (readable_ >= 0) &&
          readable_ < buffer_sentinel_ &&
          writeable_ >= 0 &&
          writeable_ < buffer_sentinel_ &&
          '\0' == buffer_[buffer_sentinel_] &&
          ((0 == size_) == (readable_ == writeable_));
}
