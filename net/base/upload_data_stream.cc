// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_data_stream.h"

#include "base/logging.h"
#include "net/base/net_errors.h"

namespace net {

UploadDataStream::UploadDataStream(const UploadData* data)
    : data_(data),
      buf_len_(0),
      next_element_(data->elements().begin()),
      next_element_offset_(0),
      next_element_remaining_(0),
      total_size_(data->GetContentLength()),
      current_position_(0) {
  FillBuf();
}

UploadDataStream::~UploadDataStream() {
}

void UploadDataStream::DidConsume(size_t num_bytes) {
  DCHECK(num_bytes <= buf_len_);

  buf_len_ -= num_bytes;
  if (buf_len_)
    memmove(buf_, buf_ + num_bytes, buf_len_);

  FillBuf();

  current_position_ += num_bytes;
}

void UploadDataStream::FillBuf() {
  std::vector<UploadData::Element>::const_iterator end =
      data_->elements().end();

  while (buf_len_ < kBufSize && next_element_ != end) {
    bool advance_to_next_element = false;

    const UploadData::Element& element = *next_element_;

    size_t size_remaining = kBufSize - buf_len_;
    if (element.type() == UploadData::TYPE_BYTES) {
      const std::vector<char>& d = element.bytes();
      size_t count = d.size() - next_element_offset_;

      size_t bytes_copied = std::min(count, size_remaining);

      memcpy(buf_ + buf_len_, &d[next_element_offset_], bytes_copied);
      buf_len_ += bytes_copied;

      if (bytes_copied == count) {
        advance_to_next_element = true;
      } else {
        next_element_offset_ += bytes_copied;
      }
    } else {
      DCHECK(element.type() == UploadData::TYPE_FILE);

      if (!next_element_stream_.IsOpen()) {
        int flags = base::PLATFORM_FILE_OPEN |
                    base::PLATFORM_FILE_READ;
        int rv = next_element_stream_.Open(
            FilePath::FromWStringHack(element.file_path()), flags);
        // If the file does not exist, that's technically okay.. we'll just
        // upload an empty file.  This is for consistency with Mozilla.
        DLOG_IF(WARNING, rv != OK) << "Failed to open \"" <<
            element.file_path() << "\" for reading: " << rv;

        next_element_remaining_ = 0;  // Default to reading nothing.
        if (rv == OK) {
          uint64 offset = element.file_range_offset();
          if (offset && next_element_stream_.Seek(FROM_BEGIN, offset) < 0) {
            DLOG(WARNING) << "Failed to seek \"" << element.file_path() <<
                "\" to offset: " << offset;
          } else {
            next_element_remaining_ = element.file_range_length();
          }
        }
      }

      int rv = 0;
      int count = static_cast<int>(std::min(
          static_cast<uint64>(size_remaining), next_element_remaining_));
      if (count > 0 &&
          (rv = next_element_stream_.Read(buf_ + buf_len_, count, NULL)) > 0) {
        buf_len_ += rv;
        next_element_remaining_ -= rv;
      } else {
        advance_to_next_element = true;
      }
    }

    if (advance_to_next_element) {
      ++next_element_;
      next_element_offset_ = 0;
      next_element_stream_.Close();
    }
  }
}

}  // namespace net

