// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include "base/eintr_wrapper.h"
#include "breakpad/linux/exception_handler.h"
#include "breakpad/linux/linux_libc_support.h"
#include "breakpad/linux/linux_syscall_support.h"

// This is defined in chrome/renderer/renderer_logging_linux.cc, it's the
// static string containing the current active URL. We send this in the crash
// report.
namespace renderer_logging {
extern std::string active_url;
}

static bool
CrashHandler(const void* crash_context, size_t crash_context_size,
             void* context) {
  const int fd = (int) context;
  int fds[2];
  pipe(fds);

  // The length of the control message:
  static const unsigned kControlMsgSize =
      CMSG_SPACE(sizeof(int)) + CMSG_SPACE(sizeof(struct ucred));

  union {
    struct kernel_msghdr msg;
    struct msghdr sys_msg;
  };
  my_memset(&msg, 0, sizeof(struct kernel_msghdr));
  struct kernel_iovec iov[2];
  iov[0].iov_base = const_cast<void*>(crash_context);
  iov[0].iov_len = crash_context_size;
  iov[1].iov_base = const_cast<char*>(renderer_logging::active_url.data());
  iov[1].iov_len = renderer_logging::active_url.size();

  msg.msg_iov = iov;
  msg.msg_iovlen = 2;
  char cmsg[kControlMsgSize];
  memset(cmsg, 0, kControlMsgSize);
  msg.msg_control = cmsg;
  msg.msg_controllen = sizeof(cmsg);

  struct cmsghdr *hdr = CMSG_FIRSTHDR(&msg);
  hdr->cmsg_level = SOL_SOCKET;
  hdr->cmsg_type = SCM_RIGHTS;
  hdr->cmsg_len = CMSG_LEN(sizeof(int));
  *((int*) CMSG_DATA(hdr)) = fds[1];
  hdr = CMSG_NXTHDR(&sys_msg, hdr);
  hdr->cmsg_level = SOL_SOCKET;
  hdr->cmsg_type = SCM_CREDENTIALS;
  hdr->cmsg_len = CMSG_LEN(sizeof(struct ucred));
  struct ucred *cred = reinterpret_cast<struct ucred*>(CMSG_DATA(hdr));
  cred->uid = getuid();
  cred->gid = getgid();
  cred->pid = getpid();

  HANDLE_EINTR(sys_sendmsg(fd, &msg, 0));
  sys_close(fds[1]);

  char b;
  HANDLE_EINTR(sys_read(fds[0], &b, 1));

  return true;
}

void EnableRendererCrashDumping() {
  // When the browser forks off our process, it installs the crash signal file
  // descriptor in this slot:
  static const int kMagicCrashSignalFd = 4;

  // We deliberately leak this object.
  google_breakpad::ExceptionHandler* handler = 
      new google_breakpad::ExceptionHandler("" /* unused */, NULL, NULL,
                                            (void*) kMagicCrashSignalFd, true);
  handler->set_crash_handler(CrashHandler);
}
