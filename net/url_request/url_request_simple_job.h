// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_SIMPLE_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_SIMPLE_JOB_H_

#include "net/url_request/url_request_job.h"

class URLRequest;

class URLRequestSimpleJob : public URLRequestJob {
 public:
  URLRequestSimpleJob(URLRequest* request);

  virtual void Start();
  virtual bool ReadRawData(net::IOBuffer* buf, int buf_size, int *bytes_read);
  virtual bool GetMimeType(std::string* mime_type);
  virtual bool GetCharset(std::string* charset);

 protected:
  // subclasses must override the way response data is determined.
  virtual bool GetData(std::string* mime_type,
                       std::string* charset,
                       std::string* data) const = 0;

 private:
  void StartAsync();

  std::string mime_type_;
  std::string charset_;
  std::string data_;
  int data_offset_;
};

#endif  // NET_URL_REQUEST_URL_REQUEST_DATA_JOB_H_
