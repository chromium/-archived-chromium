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

#ifndef NET_BASE_UPLOAD_DATA_H__
#define NET_BASE_UPLOAD_DATA_H__

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"

namespace net {

class UploadData : public base::RefCounted<UploadData> {
 public:
  UploadData() {}

  enum Type {
    TYPE_BYTES,
    TYPE_FILE
  };

  class Element {
   public:
    Element() : type_(TYPE_BYTES), file_range_offset_(0),
                file_range_length_(kuint64max) {
    }

    Type type() const { return type_; }
    const std::vector<char>& bytes() const { return bytes_; }
    const std::wstring& file_path() const { return file_path_; }
    uint64 file_range_offset() const { return file_range_offset_; }
    uint64 file_range_length() const { return file_range_length_; }

    void SetToBytes(const char* bytes, int bytes_len) {
      type_ = TYPE_BYTES;
      bytes_.assign(bytes, bytes + bytes_len);
    }

    void SetToFilePath(const std::wstring& path) {
      SetToFilePathRange(path, 0, kuint64max);
    }

    void SetToFilePathRange(const std::wstring& path,
                            uint64 offset, uint64 length) {
      type_ = TYPE_FILE;
      file_path_ = path;
      file_range_offset_ = offset;
      file_range_length_ = length;
    }

    // Returns the byte-length of the element.  For files that do not exist, 0
    // is returned.  This is done for consistency with Mozilla.
    uint64 GetContentLength() const;

   private:
    Type type_;
    std::vector<char> bytes_;
    std::wstring file_path_;
    uint64 file_range_offset_;
    uint64 file_range_length_;
  };

  void AppendBytes(const char* bytes, int bytes_len) {
    if (bytes_len > 0) {
      elements_.push_back(Element());
      elements_.back().SetToBytes(bytes, bytes_len);
    }
  }

  void AppendFile(const std::wstring& file_path) {
    elements_.push_back(Element());
    elements_.back().SetToFilePath(file_path);
  }

  void AppendFileRange(const std::wstring& file_path,
                       uint64 offset, uint64 length) {
    elements_.push_back(Element());
    elements_.back().SetToFilePathRange(file_path, offset, length);
  }

  // Returns the total size in bytes of the data to upload.
  uint64 GetContentLength() const;

  const std::vector<Element>& elements() const {
    return elements_;
  }

  void set_elements(const std::vector<Element>& elements) {
    elements_ = elements;
  }

 private:
   std::vector<Element> elements_;
};

}  // namespace net

#endif  // NET_BASE_UPLOAD_DATA_H__
