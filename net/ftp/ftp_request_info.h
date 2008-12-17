// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef NET_FTP_FTP_REQUEST_INFO_H_
#define NET_FTP_FTP_REQUEST_INFO_H_

class FtpRequestInfo {
 public:
  // The requested URL. 
  GURL url;

  // Any upload data.
  scoped_refptr<UploadData> upload_data;

  // Any load flags (see load_flags.h).
  int load_flags;
};

#endif  // NET_FTP_FTP_REQUEST_INFO_H_
