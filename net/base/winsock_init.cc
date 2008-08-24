// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <winsock2.h>

#include "net/base/winsock_init.h"

#include "base/singleton.h"

namespace net {

WinsockInit::WinsockInit() : did_init_(false) {
  did_init_ = Init();
}

bool WinsockInit::Init() {
  WORD winsock_ver = MAKEWORD(2,2);
  WSAData wsa_data;
  return (WSAStartup(winsock_ver, &wsa_data) == 0);
}

void WinsockInit::Cleanup() {
  WSACleanup();
}

WinsockInit::~WinsockInit() {
  if (did_init_)
    Cleanup();
}

void EnsureWinsockInit() {
  Singleton<WinsockInit>::get();
}

}  // namespace net

