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

#ifndef NET_BASE_UPLOAD_DATA_STREAM_H_
#define NET_BASE_UPLOAD_DATA_STREAM_H_

#include "net/base/upload_data.h"

namespace net {

class UploadDataStream {
 public:
  UploadDataStream(const UploadData* data);
  ~UploadDataStream();

  // Returns the stream's buffer and buffer length.
  const char* buf() const { return buf_; }
  size_t buf_len() const { return buf_len_; }

  // Call to indicate that a portion of the stream's buffer was consumed.  This
  // call modifies the stream's buffer so that it contains the next segment of
  // the upload data to be consumed.
  void DidConsume(size_t num_bytes);

  // Call to reset the stream position to the beginning.
  void Reset();

  // Returns the total size of the data stream and the current position.
  uint64 size() const { return total_size_; }
  uint64 position() const { return current_position_; }

 private:
  void FillBuf();

  const UploadData* data_;

  // This buffer is filled with data to be uploaded.  The data to be sent is
  // always at the front of the buffer.  If we cannot send all of the buffer at
  // once, then we memmove the remaining portion and back-fill the buffer for
  // the next "write" call.  buf_len_ indicates how much data is in the buffer.
  enum { kBufSize = 16384 };
  char buf_[kBufSize];
  size_t buf_len_;

  // Iterator to the upload element to be written to the send buffer next.
  std::vector<UploadData::Element>::const_iterator next_element_;

  // The byte offset into next_element_'s data buffer if the next element is
  // a TYPE_BYTES element.
  size_t next_element_offset_;

  // A handle to the currently open file (or INVALID_HANDLE_VALUE) for
  // next_element_ if the next element is a TYPE_FILE element.
  HANDLE next_element_handle_;

  // The number of bytes remaining to be read from the currently open file
  // if the next element is of TYPE_FILE.
  uint64 next_element_remaining_;

  // Size and current read position within the stream.
  uint64 total_size_;
  uint64 current_position_;

  DISALLOW_EVIL_CONSTRUCTORS(UploadDataStream);
};

}  // namespace net

#endif  // NET_BASE_UPLOAD_DATA_STREAM_H_
