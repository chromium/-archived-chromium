// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef NET_BASE_FILTER_UNITTEST_H_
#define NET_BASE_FILTER_UNITTEST_H_

#include <string>

#include "googleurl/src/gurl.h"
#include "net/base/filter.h"

//------------------------------------------------------------------------------
class MockFilterContext : public FilterContext {
 public:
  explicit MockFilterContext(int buffer_size)
    : buffer_size_(buffer_size),
      is_cached_content_(false),
      is_download_(false),
      is_sdch_response_(false),
      response_code_(-1) {
  }

  void SetBufferSize(int buffer_size) { buffer_size_ = buffer_size; }
  void SetMimeType(const std::string& mime_type) { mime_type_ = mime_type; }
  void SetURL(const GURL& gurl) { gurl_ = gurl; }
  void SetRequestTime(const base::Time time) { request_time_ = time; }
  void SetCached(bool is_cached) { is_cached_content_ = is_cached; }
  void SetDownload(bool is_download) { is_download_ = is_download; }
  void SetResponseCode(int response_code) { response_code_ = response_code; }
  void SetSdchResponse(bool is_sdch_response) {
    is_sdch_response_ = is_sdch_response;
  }

  virtual bool GetMimeType(std::string* mime_type) const {
    *mime_type = mime_type_;
    return true;
  }

  // What URL was used to access this data?
  // Return false if gurl is not present.
  virtual bool GetURL(GURL* gurl) const {
    *gurl = gurl_;
    return true;
  }

  // What was this data requested from a server?
  virtual base::Time GetRequestTime() const {
    return request_time_;
  }

  // Is data supplied from cache, or fresh across the net?
  virtual bool IsCachedContent() const { return is_cached_content_; }

  // Is this a download?
  virtual bool IsDownload() const { return is_download_; }

  // Was this data flagged as a response to a request with an SDCH dictionary?
  virtual bool IsSdchResponse() const { return is_sdch_response_; }

  // How many bytes were fed to filter(s) so far?
  virtual int64 GetByteReadCount() const { return 0; }

  virtual int GetResponseCode() const { return response_code_; }

  // What is the desirable input buffer size for these filters?
  virtual int GetInputStreamBufferSize() const { return buffer_size_; }

  virtual void RecordPacketStats(StatisticSelector statistic) const {}

 private:
  int buffer_size_;
  std::string mime_type_;
  GURL gurl_;
  base::Time request_time_;
  bool is_cached_content_;
  bool is_download_;
  bool is_sdch_response_;
  int response_code_;

  DISALLOW_COPY_AND_ASSIGN(MockFilterContext);
};

#endif  // NET_BASE_FILTER_UNITTEST_H_
