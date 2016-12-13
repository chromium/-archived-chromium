// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef NET_FTP_FTP_RESPONSE_INFO_H_
#define NET_FTP_FTP_RESPONSE_INFO_H_

#include "net/base/auth.h"

namespace net {

class FtpResponseInfo {
 public:
  FtpResponseInfo() : is_directory_listing(false) {
  }

  // Non-null when authentication is required.
  scoped_refptr<AuthChallengeInfo> auth_challenge;

  // The time at which the request was made that resulted in this response.
  // For cached responses, this time could be "far" in the past.
  base::Time request_time;

  // The time at which the response headers were received.  For cached
  // responses, this time could be "far" in the past.
  base::Time response_time;

  // True if the response data is of a directory listing.
  bool is_directory_listing;
};

}  // namespace net

#endif  // NET_FTP_FTP_RESPONSE_INFO_H_
