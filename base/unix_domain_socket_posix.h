// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_UNIX_DOMAIN_SOCKET_POSIX_H_
#define BASE_UNIX_DOMAIN_SOCKET_POSIX_H_

#include <sys/types.h>
#include <vector>

namespace base {

// Use sendmsg to write the given msg and include a vector
// of file descriptors. Returns true iff successful.
bool SendMsg(int fd, const void* msg, size_t length,
             std::vector<int>& fds);
// Use recvmsg to read a message and an array of file descriptors. Returns
// -1 on failure. Note: will read, at most, 16 descriptors.
ssize_t RecvMsg(int fd, void* msg, size_t length, std::vector<int>* fds);

}  // namespace base

#endif  // BASE_UNIX_DOMAIN_SOCKET_POSIX_H_
