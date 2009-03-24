// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_UPLOAD_DATA_H_
#define NET_BASE_UPLOAD_DATA_H_

#include <string>
#include <vector>

#include "base/ref_counted.h"

namespace net {

class UploadData : public base::RefCounted<UploadData> {
 public:
  UploadData() : identifier_(0) {}

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

  void swap_elements(std::vector<Element>* elements) {
    elements_.swap(*elements);
  }

  // Identifies a particular upload instance, which is used by the cache to
  // formulate a cache key.  This value should be unique across browser
  // sessions.  A value of 0 is used to indicate an unspecified identifier.
  void set_identifier(int64 id) { identifier_ = id; }
  int64 identifier() const { return identifier_; }

 private:
  std::vector<Element> elements_;
  int64 identifier_;
};

}  // namespace net

#endif  // NET_BASE_UPLOAD_DATA_H_
