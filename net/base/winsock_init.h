// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Winsock initialization must happen before any Winsock calls are made.  This
// class provides a wrapper for WSAStartup and WSACleanup. There are 3 ways to
// use it: either allocate a new WinsockInit object at startup and delete when
// shutting down, manually call Init and Cleanup, or use the EnsureWinsockInit
// method, which may be called multiple times.  In the second case, Cleanup
// should only be called if Init was successful.

#ifndef NET_BASE_WINSOCK_INIT_H_
#define NET_BASE_WINSOCK_INIT_H_

namespace net {

class WinsockInit {
 public:
  WinsockInit();
  ~WinsockInit();

  static bool Init();
  static void Cleanup();

 private:
  bool did_init_;
};

// Force there to be a global WinsockInit object that gets created once and
// destroyed at application exit.  This may be called multiple times.
void EnsureWinsockInit();

}  // namespace net

#endif  // NET_BASE_WINSOCK_INIT_H_

