// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_REQUEST_INFO_H__
#define NET_HTTP_HTTP_REQUEST_INFO_H__

#include "base/ref_counted.h"
#include "googleurl/src/gurl.h"
#include "net/base/upload_data.h"

namespace net {

class HttpRequestInfo {
 public:
  // The requested URL.
  GURL url;

  // The referring URL (if any).
  GURL referrer;

  // The method to use (GET, POST, etc.).
  std::string method;

  // The user agent string to use.  TODO(darin): we should just add this to
  // extra_headers
  std::string user_agent;

  // Any extra request headers (\r\n-delimited).
  std::string extra_headers;

  // Any upload data.
  scoped_refptr<UploadData> upload_data;

  // Any load flags (see load_flags.h).
  int load_flags;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_REQUEST_INFO_H__
