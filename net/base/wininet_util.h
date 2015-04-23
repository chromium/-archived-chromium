// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_WININET_UTIL_H__
#define NET_BASE_WININET_UTIL_H__

#include <windows.h>
#include <wininet.h>

#include <string>

namespace net {

// Global functions and variables for using WinInet.
class WinInetUtil {
 public:
  // Maps Windows error codes (returned by GetLastError()) to net::ERR_xxx
  // error codes.
  static int OSErrorToNetError(DWORD os_error);
};

}  // namespace net

#endif  // NET_BASE_WININET_UTIL_H__
