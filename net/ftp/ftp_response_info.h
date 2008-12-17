// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef NET_FTP_FTP_RESPONSE_INFO_H_
#define NET_FTP_FTP_RESPONSE_INFO_H_

#include "net/base/auth.h"

namespace net {

class FtpResponseInfo {
 public:
  // Non-null when authentication is required.
  scoped_refptr<AuthChallengeInfo> auth_challenge;

  // The length of the response.  -1 means unspecified.
  int64 content_length;

  // True if the response data is of a directory listing.
  bool is_directory_listing;

  FtpResponseInfo() : content_length(-1), is_directory_listing(false) {
  }
};

}  // namespace net

#endif  // NET_FTP_FTP_RESPONSE_INFO_H_
