// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_UPLOAD_DATA_STREAM_H_
#define NET_BASE_UPLOAD_DATA_STREAM_H_

#include "net/base/file_stream.h"
#include "net/base/upload_data.h"

namespace net {

class IOBuffer;

class UploadDataStream {
 public:
  explicit UploadDataStream(const UploadData* data);
  ~UploadDataStream();

  // Returns the stream's buffer and buffer length.
  IOBuffer* buf() const { return buf_; }
  size_t buf_len() const { return buf_len_; }

  // Call to indicate that a portion of the stream's buffer was consumed.  This
  // call modifies the stream's buffer so that it contains the next segment of
  // the upload data to be consumed.
  void DidConsume(size_t num_bytes);

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
  scoped_refptr<IOBuffer> buf_;
  size_t buf_len_;

  // Iterator to the upload element to be written to the send buffer next.
  std::vector<UploadData::Element>::const_iterator next_element_;

  // The byte offset into next_element_'s data buffer if the next element is
  // a TYPE_BYTES element.
  size_t next_element_offset_;

  // A stream to the currently open file, for next_element_ if the next element
  // is a TYPE_FILE element.
  FileStream next_element_stream_;

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
