// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_sandbox_host_linux.h"

#include <stdint.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include "base/eintr_wrapper.h"
#include "base/process_util.h"
#include "base/logging.h"
#include "base/message_loop.h"

#include "SkFontHost_fontconfig_direct.h"
#include "SkFontHost_fontconfig_ipc.h"

// http://code.google.com/p/chromium/wiki/LinuxSandboxIPC

// BEWARE: code in this file run across *processes* (not just threads).

// This code runs in a child process
class SandboxIPCProcess {
 public:
  // lifeline_fd: this is the read end of a pipe which the browser process
  //   holds the other end of. If the browser process dies, it's descriptors are
  //   closed and we will noticed an EOF on the pipe. That's our signal to exit.
  // browser_socket: the 'browser's end of the sandbox IPC socketpair. From the
  //   point of view of the renderer's, it's talking to the browser but this
  //   object actually services the requests.
  SandboxIPCProcess(int lifeline_fd, int browser_socket)
      : lifeline_fd_(lifeline_fd),
        browser_socket_(browser_socket),
        font_config_(new FontConfigDirect()) {
    base::InjectiveMultimap multimap;
    multimap.push_back(base::InjectionArc(0, lifeline_fd, false));
    multimap.push_back(base::InjectionArc(0, browser_socket, false));

    base::CloseSuperfluousFds(multimap);
  }

  void Run() {
    const int epollfd = epoll_create(2);
    CHECK(epollfd >= 0);
    struct epoll_event ev;

    ev.events = EPOLLIN;
    ev.data.fd = lifeline_fd_;
    CHECK(0 == epoll_ctl(epollfd, EPOLL_CTL_ADD, lifeline_fd_, &ev));

    ev.events = EPOLLIN;
    ev.data.fd = browser_socket_;
    CHECK(0 == epoll_ctl(epollfd, EPOLL_CTL_ADD, browser_socket_, &ev));

    for (;;) {
      CHECK(1 == HANDLE_EINTR(epoll_wait(epollfd, &ev, 1, -1)));
      if (ev.data.fd == lifeline_fd_) {
        // our parent died so we should too.
        _exit(0);
      } else {
        CHECK(ev.data.fd == browser_socket_);
        HandleRequest(browser_socket_);
      }
    }
  }

  void HandleRequest(int fd) {
    struct msghdr msg = {0};
    struct iovec iov;
    uint8_t buf[1024];
    uint8_t control_buf[CMSG_SPACE(sizeof(int))];
    iov.iov_base = buf;
    iov.iov_len = sizeof(buf);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control_buf;
    msg.msg_controllen = sizeof(control_buf);

    const ssize_t n = HANDLE_EINTR(recvmsg(fd, &msg, 0));

    if (n < 1) {
      LOG(ERROR) << "Error reading from sandbox IPC socket. Sandbox IPC is"
                 << " disabled."
                 << " n:" << n
                 << " errno:" << errno;
      _exit(1);
      return;
    }

    if (msg.msg_controllen != sizeof(control_buf) ||
        n < static_cast<ssize_t>(sizeof(uint16_t)) ||
        msg.msg_flags) {
      LOG(ERROR) << "Sandbox IPC: missing control message or truncated message:"
                 << " n:" << n
                 << " msg.msg_controllen:" << msg.msg_controllen
                 << " msg.msg_flags:" << msg.msg_flags;
      return;
    }

    // Get the reply socket from the control message
    int reply_fd = -1;
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
      // The client cannot send us additional descriptors because the control
      // message buffer is only sized for a single int.
      reply_fd = *reinterpret_cast<int*>(CMSG_DATA(cmsg));
    } else {
      LOG(ERROR) << "Sandbox IPC: message without reply descriptor:"
                 << " n:" << n
                 << " msg.msg_controllen:" << msg.msg_controllen
                 << " cmsg->cmsg_level:" << cmsg->cmsg_level
                 << " cmsg->cmsg_type:" << cmsg->cmsg_type;
      return;
    }

    const uint16_t request_type = *reinterpret_cast<uint16_t*>(buf);
    switch (request_type) {
      case FontConfigIPC::METHOD_MATCH:
        return FontConfigMatch(reply_fd, buf, n);
      case FontConfigIPC::METHOD_OPEN:
        return FontConfigOpen(reply_fd, buf, n);
      default:
        LOG(ERROR) << "Sandbox IPC: message with unknown type:"
                   << " request_type:" << request_type;
        HANDLE_EINTR(close(reply_fd));
    }
  }

  // Send a reply to a client
  //   reply_fd: the reply channel given to us by the client
  //   iov, iov_len: the contents of the reply message
  //   extra_fd: an fd to include in the reply, or -1
  //
  // Both reply_fd and extra_fd (if any) are closed.
  void SendReplyAndClose(int reply_fd, const struct iovec* iov,
                         unsigned iov_len, int extra_fd) {
    struct msghdr msg = {0};
    msg.msg_iov = const_cast<struct iovec*>(iov);
    msg.msg_iovlen = iov_len;

    uint8_t control_buf[CMSG_SPACE(sizeof(int))];

    if (extra_fd >= 0) {
      msg.msg_control = control_buf;
      msg.msg_controllen = sizeof(control_buf);

      struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
      cmsg->cmsg_level = SOL_SOCKET;
      cmsg->cmsg_type = SCM_RIGHTS;
      cmsg->cmsg_len = CMSG_LEN(sizeof(int));
      *reinterpret_cast<int*>(CMSG_DATA(cmsg)) = extra_fd;
    }

    HANDLE_EINTR(sendmsg(reply_fd, &msg, MSG_NOSIGNAL | MSG_DONTWAIT));
    HANDLE_EINTR(close(reply_fd));
    if (extra_fd >= 0)
      HANDLE_EINTR(close(extra_fd));
  }

  void FontConfigMatch(int reply_fd, const uint8_t* request_bytes,
                       unsigned request_len) {
    if (request_len < sizeof(FontConfigIPC::MatchRequest))
      return (void) HANDLE_EINTR(close(reply_fd));

    const FontConfigIPC::MatchRequest* request =
        reinterpret_cast<const FontConfigIPC::MatchRequest*>(request_bytes);

    if (request_len != sizeof(FontConfigIPC::MatchRequest) + request->family_len)
      return (void) HANDLE_EINTR(close(reply_fd));

    const std::string family(
        reinterpret_cast<const char*>(request_bytes + sizeof(*request)),
        request->family_len);
    std::string result_family;
    unsigned result_fileid;

    const bool r = font_config_->Match(
        &result_family, &result_fileid, request->fileid_valid, request->fileid,
        family, request->is_bold, request->is_italic);

    struct iovec iov[2];
    FontConfigIPC::MatchReply reply;
    memset(&reply, 0, sizeof(reply));

    iov[0].iov_base = &reply;
    iov[0].iov_len = sizeof(reply);

    if (r) {
      reply.result = 1;
      reply.result_fileid = result_fileid;
      reply.filename_len = result_family.size();

      iov[1].iov_base = const_cast<char*>(result_family.data());
      iov[1].iov_len = result_family.size();
    }

    SendReplyAndClose(reply_fd, iov, r ? 2 : 1, -1 /* no fd */);
  }

  void FontConfigOpen(int reply_fd, const uint8_t* request_bytes,
                      unsigned request_len) {
    if (request_len < sizeof(FontConfigIPC::OpenRequest))
      return (void) HANDLE_EINTR(close(reply_fd));

    const FontConfigIPC::OpenRequest* request =
        reinterpret_cast<const FontConfigIPC::OpenRequest*>(request_bytes);

    FontConfigDirect* fc = reinterpret_cast<FontConfigDirect*>(font_config_);

    const int result_fd = fc->Open(request->fileid);

    FontConfigIPC::OpenReply reply;
    reply.result = result_fd >= 0 ? 1 : 0;

    struct iovec iov;
    iov.iov_base = &reply;
    iov.iov_len = sizeof(reply);

    SendReplyAndClose(reply_fd, &iov, 1, result_fd);
  }

 private:
  const int lifeline_fd_;
  const int browser_socket_;
  FontConfigDirect* const font_config_;
};

// -----------------------------------------------------------------------------

// Runs on the main thread at startup.
RenderSandboxHostLinux::RenderSandboxHostLinux() {
  int fds[2];
  CHECK(socketpair(AF_UNIX, SOCK_DGRAM, 0, fds) == 0);

  renderer_socket_ = fds[0];
  const int browser_socket = fds[1];

  int pipefds[2];
  CHECK(0 == pipe(pipefds));
  const int child_lifeline_fd = pipefds[0];
  childs_lifeline_fd_ = pipefds[1];

  const pid_t child = fork();
  if (child == 0) {
    SandboxIPCProcess handler(child_lifeline_fd, browser_socket);
    handler.Run();
    _exit(0);
  }
}

RenderSandboxHostLinux::~RenderSandboxHostLinux() {
  HANDLE_EINTR(close(renderer_socket_));
  HANDLE_EINTR(close(childs_lifeline_fd_));
}
