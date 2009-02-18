// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/client_socket.h"

#include "base/logging.h"
#include "net/base/net_errors.h"

namespace net {

#if defined(OS_LINUX)
// Identical to posix system call getpeername().
// Needed by ssl_client_socket_nss.
int ClientSocket::GetPeerName(struct sockaddr *name, socklen_t *namelen) {
  // Default implementation just permits some unit tests to link.
  NOTREACHED();
  return ERR_UNEXPECTED;
}
#endif

}  // namespace net
