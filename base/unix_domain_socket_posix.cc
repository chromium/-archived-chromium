// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/unix_domain_socket_posix.h"

#include <unistd.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "base/eintr_wrapper.h"
#include "base/logging.h"

namespace base {

bool SendMsg(int fd, const void* buf, size_t length, std::vector<int>& fds) {
  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));
  struct iovec iov = {const_cast<void*>(buf), length};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  char* control_buffer = NULL;
  if (fds.size()) {
    const unsigned control_len = CMSG_SPACE(sizeof(int) * fds.size());
    control_buffer = new char[control_len];
    if (!control_buffer)
      return false;

    struct cmsghdr *cmsg;

    msg.msg_control = control_buffer;
    msg.msg_controllen = control_len;
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int) * fds.size());
    memcpy(CMSG_DATA(cmsg), &fds[0], sizeof(int) * fds.size());
    msg.msg_controllen = cmsg->cmsg_len;
  }

  const ssize_t r = HANDLE_EINTR(sendmsg(fd, &msg, 0));
  const bool ret = static_cast<ssize_t>(length) == r;
  delete[] control_buffer;
  return ret;
}

ssize_t RecvMsg(int fd, void* buf, size_t length, std::vector<int>* fds) {
  static const unsigned kMaxDescriptors = 16;

  fds->clear();

  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));
  struct iovec iov = {buf, length};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  char control_buffer[CMSG_SPACE(sizeof(int) * kMaxDescriptors)];
  msg.msg_control = control_buffer;
  msg.msg_controllen = sizeof(control_buffer);

  const ssize_t r = HANDLE_EINTR(recvmsg(fd, &msg, 0));
  if (r == -1)
    return -1;

  int* wire_fds;
  unsigned wire_fds_len = 0;

  if (msg.msg_controllen > 0) {
    struct cmsghdr* cmsg;
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
      if (cmsg->cmsg_level == SOL_SOCKET &&
          cmsg->cmsg_type == SCM_RIGHTS) {
        const unsigned payload_len = cmsg->cmsg_len - CMSG_LEN(0);
        DCHECK(payload_len % sizeof(int) == 0);
        wire_fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
        wire_fds_len = payload_len / sizeof(int);
        break;
      }
    }
  }

  if (msg.msg_flags & MSG_TRUNC || msg.msg_flags & MSG_CTRUNC) {
    for (unsigned i = 0; i < wire_fds_len; ++i)
      close(wire_fds[i]);
    errno = EMSGSIZE;
    return -1;
  }

  fds->resize(wire_fds_len);
  memcpy(&(*fds)[0], wire_fds, sizeof(int) * wire_fds_len);

  return r;
}

}  // namespace base
