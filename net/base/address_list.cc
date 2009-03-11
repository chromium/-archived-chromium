// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_list.h"

#ifdef OS_WIN
#include <ws2tcpip.h>
#include <wspiapi.h>  // Needed for Win2k compat.
#else
#include <netdb.h>
#endif

namespace net {

void AddressList::Adopt(struct addrinfo* head) {
  data_ = new Data(head);
}

AddressList::Data::~Data() {
  freeaddrinfo(head);
}

}  // namespace net
