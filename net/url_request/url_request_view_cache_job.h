// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_VIEW_CACHE_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_VIEW_CACHE_JOB_H_

#include "net/url_request/url_request.h"
#include "net/url_request/url_request_simple_job.h"

namespace disk_cache {
class Backend;
}

// A job subclass that implements the view-cache: protocol, which simply
// provides a debug view of the cache or of a particular cache entry.
class URLRequestViewCacheJob : public URLRequestSimpleJob {
 public:
  URLRequestViewCacheJob(URLRequest* request) : URLRequestSimpleJob(request) {}

  static URLRequest::ProtocolFactory Factory;

  // override from URLRequestSimpleJob
  virtual bool GetData(std::string* mime_type,
                       std::string* charset,
                       std::string* data) const;

 private:
  disk_cache::Backend* GetDiskCache() const;
};

#endif  // NET_URL_REQUEST_URL_REQUEST_VIEW_CACHE_JOB_H_
