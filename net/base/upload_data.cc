// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_data.h"

#include "base/file_util.h"
#include "base/logging.h"

namespace net {

uint64 UploadData::GetContentLength() const {
  uint64 len = 0;
  std::vector<Element>::const_iterator it = elements_.begin();
  for (; it != elements_.end(); ++it)
    len += (*it).GetContentLength();
  return len;
}

uint64 UploadData::Element::GetContentLength() const {
  if (type_ == TYPE_BYTES)
    return static_cast<uint64>(bytes_.size());

  DCHECK(type_ == TYPE_FILE);

  // TODO(darin): This size calculation could be out of sync with the state of
  // the file when we get around to reading it.  We should probably find a way
  // to lock the file or somehow protect against this error condition.

  int64 length = 0;
  if (!file_util::GetFileSize(file_path_, &length))
    return 0;

  if (file_range_offset_ >= static_cast<uint64>(length))
    return 0;  // range is beyond eof

  // compensate for the offset and clip file_range_length_ to eof
  return std::min(length - file_range_offset_, file_range_length_);
}

}  // namespace net
