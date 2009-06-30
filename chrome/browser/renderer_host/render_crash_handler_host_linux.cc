// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_crash_handler_host_linux.h"

#include <stdint.h>

#include <unistd.h>
#include <sys/uio.h>
#include <sys/socket.h>

#include "base/eintr_wrapper.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "breakpad/linux/exception_handler.h"
#include "breakpad/linux/linux_dumper.h"
#include "breakpad/linux/minidump_writer.h"
#include "chrome/app/breakpad_linux.h"
#include "chrome/browser/chrome_thread.h"

// Since RenderCrashHandlerHostLinux is a singleton, it's only destroyed at the
// end of the processes lifetime, which is greater in span then the lifetime of
// the IO message loop.
template<> struct RunnableMethodTraits<RenderCrashHandlerHostLinux> {
  static void RetainCallee(RenderCrashHandlerHostLinux*) { }
  static void ReleaseCallee(RenderCrashHandlerHostLinux*) { }
};

RenderCrashHandlerHostLinux::RenderCrashHandlerHostLinux()
    : renderer_socket_(-1),
      browser_socket_(-1) {
  int fds[2];
  CHECK(socketpair(AF_UNIX, SOCK_DGRAM, 0, fds) == 0);
  static const int on = 1;

  // Enable passcred on the server end of the socket
  CHECK(setsockopt(fds[1], SOL_SOCKET, SO_PASSCRED, &on, sizeof(on)) == 0);

  renderer_socket_ = fds[0];
  browser_socket_ = fds[1];

  MessageLoop* ml = ChromeThread::GetMessageLoop(ChromeThread::IO);
  ml->PostTask(FROM_HERE, NewRunnableMethod(
        this, &RenderCrashHandlerHostLinux::Init));
}

RenderCrashHandlerHostLinux::~RenderCrashHandlerHostLinux() {
  HANDLE_EINTR(close(renderer_socket_));
  HANDLE_EINTR(close(browser_socket_));
}

void RenderCrashHandlerHostLinux::Init() {
  MessageLoopForIO* ml = MessageLoopForIO::current();
  CHECK(ml->WatchFileDescriptor(
      browser_socket_, true /* persistent */,
      MessageLoopForIO::WATCH_READ,
      &file_descriptor_watcher_, this));
  ml->AddDestructionObserver(this);
}

void RenderCrashHandlerHostLinux::OnFileCanWriteWithoutBlocking(int fd) {
  DCHECK(false);
}

void RenderCrashHandlerHostLinux::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(fd, browser_socket_);

  // A renderer process has crashed and has signaled us by writing a datagram
  // to the death signal socket. The datagram contains the crash context needed
  // for writing the minidump as well as a file descriptor and a credentials
  // block so that they can't lie about their pid.

  // The length of the control message:
  static const unsigned kControlMsgSize =
      CMSG_SPACE(sizeof(int)) + CMSG_SPACE(sizeof(struct ucred));
  // The length of the regular payload:
  static const unsigned kCrashContextSize =
      sizeof(google_breakpad::ExceptionHandler::CrashContext);
  static const unsigned kMaxActiveURLSize = 1024;
  static const unsigned kGuidSize = 32;  // 128 bits = 32 chars in hex.

  struct msghdr msg = {0};
  struct iovec iov;
  char context[kCrashContextSize + kMaxActiveURLSize + kGuidSize];
  char control[kControlMsgSize];
  iov.iov_base = context;
  iov.iov_len = sizeof(context);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = control;
  msg.msg_controllen = kControlMsgSize;

  const ssize_t n = HANDLE_EINTR(recvmsg(browser_socket_, &msg, 0));
  if (n < 1) {
    LOG(ERROR) << "Error reading from death signal socket. Crash dumping"
               << " is disabled."
               << " n:" << n
               << " errno:" << errno;
    file_descriptor_watcher_.StopWatchingFileDescriptor();
    return;
  }

  if (n < static_cast<ssize_t>(kCrashContextSize) ||
      msg.msg_controllen != kControlMsgSize ||
      msg.msg_flags & ~MSG_TRUNC) {
    LOG(ERROR) << "Received death signal message with the wrong size;"
               << " n:" << n
               << " msg.msg_controllen:" << msg.msg_controllen
               << " msg.msg_flags:" << msg.msg_flags
               << " kCrashContextSize:" << kCrashContextSize
               << " kControlMsgSize:" << kControlMsgSize;
    return;
  }

  // After the message contents we have the guid.
  const char* const guid = &context[kCrashContextSize];

  // Anything in the guid after the crash context is the crashing URL.
  const char* const crash_url = &context[kCrashContextSize + kGuidSize];
  const unsigned crash_url_len = n - kCrashContextSize - kGuidSize;

  // Walk the control payload an extract the file descriptor and validated pid.
  pid_t crashing_pid = -1;
  int signal_fd = -1;
  for (struct cmsghdr *hdr = CMSG_FIRSTHDR(&msg); hdr;
       hdr = CMSG_NXTHDR(&msg, hdr)) {
    if (hdr->cmsg_level != SOL_SOCKET)
      continue;
    if (hdr->cmsg_type == SCM_RIGHTS) {
      const unsigned len = hdr->cmsg_len -
          (((uint8_t*)CMSG_DATA(hdr)) - (uint8_t*)hdr);
      DCHECK(len % sizeof(int) == 0);
      const unsigned num_fds = len / sizeof(int);
      if (num_fds > 1 || num_fds == 0) {
        // A nasty renderer could try and send us too many descriptors and
        // force a leak.
        LOG(ERROR) << "Death signal contained too many descriptors;"
                   << " num_fds:" << num_fds;
        for (unsigned i = 0; i < num_fds; ++i)
          HANDLE_EINTR(close(reinterpret_cast<int*>(CMSG_DATA(hdr))[i]));
        return;
      } else {
        signal_fd = reinterpret_cast<int*>(CMSG_DATA(hdr))[0];
      }
    } else if (hdr->cmsg_type == SCM_CREDENTIALS) {
      const struct ucred *cred =
          reinterpret_cast<struct ucred*>(CMSG_DATA(hdr));
      crashing_pid = cred->pid;
    }
  }

  if (crashing_pid == -1 || signal_fd == -1) {
    LOG(ERROR) << "Death signal message didn't contain all expected control"
               << " messages";
    if (signal_fd)
      HANDLE_EINTR(close(signal_fd));
    return;
  }

  const uint64 rand = base::RandUint64();
  const std::string minidump_filename =
      StringPrintf("/tmp/chromium-renderer-minidump-%016" PRIx64 ".dmp", rand);
  if (!google_breakpad::WriteMinidump(minidump_filename.c_str(),
                                      crashing_pid, context,
                                      kCrashContextSize)) {
    LOG(ERROR) << "Failed to write crash dump for pid " << crashing_pid;
    HANDLE_EINTR(close(signal_fd));
  }

  // Send the done signal to the renderer: it can exit now.
  memset(&msg, 0, sizeof(msg));
  iov.iov_base = const_cast<char*>("\x42");
  iov.iov_len = 1;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  HANDLE_EINTR(sendmsg(signal_fd, &msg, MSG_DONTWAIT | MSG_NOSIGNAL));
  HANDLE_EINTR(close(signal_fd));

  UploadCrashDump(minidump_filename.c_str(),
                  "renderer", 8,
                  crash_url, crash_url_len,
                  guid, kGuidSize);
}

void RenderCrashHandlerHostLinux::WillDestroyCurrentMessageLoop() {
  file_descriptor_watcher_.StopWatchingFileDescriptor();
}
