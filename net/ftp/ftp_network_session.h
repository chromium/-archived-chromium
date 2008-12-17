// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef NET_FTP_FTP_NETWORK_SESSION_H_
#define NET_FTP_FTP_NETWORK_SESSION_H_

#include "base/ref_counted.h"
#include "net/base/auth_cache.h"

namespace net {

// This class holds session objects used by FtpNetworkTransaction objects.
class FtpNetworkSession : public base::RefCounted<FtpNetworkSession> {
 public:
  FtpNetworkSession() {}

  AuthCache* auth_cache() { return &auth_cache_; }

 private:
  AuthCache auth_cache_;
};

}  // namespace net

#endif  // NET_FTP_FTP_NETWORK_SESSION_H_
