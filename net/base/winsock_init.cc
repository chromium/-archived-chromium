// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <winsock2.h>

#include "net/base/winsock_init.h"

#include "base/logging.h"
#include "base/singleton.h"

namespace {

class WinsockInitSingleton {
 public:
  WinsockInitSingleton() : did_init_(false) {
    WORD winsock_ver = MAKEWORD(2, 2);
    WSAData wsa_data;
    did_init_ = (WSAStartup(winsock_ver, &wsa_data) == 0);
    if (did_init_) {
      DCHECK(wsa_data.wVersion == winsock_ver);

      // The first time WSAGetLastError is called, the delay load helper will
      // resolve the address with GetProcAddress and fixup the import.  If a
      // third party application hooks system functions without correctly
      // restoring the error code, it is possible that the error code will be
      // overwritten during delay load resolution.  The result of the first
      // call may be incorrect, so make sure the function is bound and future
      // results will be correct.
      WSAGetLastError();
    }
  }

  ~WinsockInitSingleton() {
    if (did_init_)
      WSACleanup();
  }

 private:
  bool did_init_;
};

}  // namespace

namespace net {

void EnsureWinsockInit() {
  Singleton<WinsockInitSingleton>::get();
}

}  // namespace net
