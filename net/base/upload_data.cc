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

#include "net/base/upload_data.h"

#include <windows.h>

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

  // NOTE: wininet is unable to upload files larger than 4GB, but we'll let the
  // http layer worry about that.
  // TODO(darin): This size calculation could be out of sync with the state of
  // the file when we get around to reading it.  We should probably find a way
  // to lock the file or somehow protect against this error condition.

  WIN32_FILE_ATTRIBUTE_DATA info;
  if (!GetFileAttributesEx(file_path_.c_str(), GetFileExInfoStandard, &info)) {
    DLOG(WARNING) << "GetFileAttributesEx failed: " << GetLastError();
    return 0;
  }

  uint64 length = static_cast<uint64>(info.nFileSizeHigh) << 32 |
                  info.nFileSizeLow;
  if (file_range_offset_ >= length)
    return 0;  // range is beyond eof

  // compensate for the offset and clip file_range_length_ to eof
  return std::min(length - file_range_offset_, file_range_length_);
}

}  // namespace net
