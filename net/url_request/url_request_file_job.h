// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_FILE_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_FILE_JOB_H_

#include <string>

#include "base/file_path.h"
#include "net/base/completion_callback.h"
#include "net/base/file_stream.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

namespace file_util {
struct FileInfo;
}

// A request job that handles reading file URLs
class URLRequestFileJob : public URLRequestJob {
 public:
  URLRequestFileJob(URLRequest* request, const FilePath& file_path);
  virtual ~URLRequestFileJob();

  virtual void Start();
  virtual void Kill();
  virtual bool ReadRawData(net::IOBuffer* buf, int buf_size, int* bytes_read);
  virtual bool IsRedirectResponse(GURL* location, int* http_status_code);
  virtual bool GetContentEncodings(
      std::vector<Filter::FilterType>* encoding_type);
  virtual bool GetMimeType(std::string* mime_type) const;
  virtual void SetExtraRequestHeaders(const std::string& headers);

  static URLRequest::ProtocolFactory Factory;

 protected:
  // The OS-specific full path name of the file
  FilePath file_path_;

 private:
  void DidResolve(bool exists, const file_util::FileInfo& file_info);
  void DidRead(int result);

  net::CompletionCallbackImpl<URLRequestFileJob> io_callback_;
  net::FileStream stream_;
  bool is_directory_;

  net::HttpByteRange byte_range_;
  int64 remaining_bytes_;

#if defined(OS_WIN)
  class AsyncResolver;
  friend class AsyncResolver;
  scoped_refptr<AsyncResolver> async_resolver_;
#endif

  DISALLOW_COPY_AND_ASSIGN(URLRequestFileJob);
};

#endif  // NET_URL_REQUEST_URL_REQUEST_FILE_JOB_H_
