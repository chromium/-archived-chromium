// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/render_sandbox_host_linux.h"

#include <stdint.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/poll.h>

#include "base/eintr_wrapper.h"
#include "base/process_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/pickle.h"
#include "base/string_util.h"
#include "base/unix_domain_socket_posix.h"
#include "chrome/common/sandbox_methods_linux.h"
#include "webkit/api/public/gtk/WebFontInfo.h"
#include "webkit/api/public/WebData.h"
#include "webkit/api/public/WebKit.h"
#include "webkit/api/public/WebKitClient.h"

#include "SkFontHost_fontconfig_direct.h"
#include "SkFontHost_fontconfig_ipc.h"

using WebKit::WebClipboard;
using WebKit::WebData;
using WebKit::WebFontInfo;
using WebKit::WebKitClient;
using WebKit::WebMimeRegistry;
using WebKit::WebPluginInfo;
using WebKit::WebPluginListBuilder;
using WebKit::WebSandboxSupport;
using WebKit::WebString;
using WebKit::WebThemeEngine;
using WebKit::WebUChar;
using WebKit::WebURL;
using WebKit::WebURLLoader;

// http://code.google.com/p/chromium/wiki/LinuxSandboxIPC

// BEWARE: code in this file run across *processes* (not just threads).

// This code runs in a child process
class SandboxIPCProcess : public WebKitClient {
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
    WebKit::initialize(this);

    base::InjectiveMultimap multimap;
    multimap.push_back(base::InjectionArc(0, lifeline_fd, false));
    multimap.push_back(base::InjectionArc(0, browser_socket, false));

    base::CloseSuperfluousFds(multimap);
  }

  void Run() {
    struct pollfd pfds[2];
    pfds[0].fd = lifeline_fd_;
    pfds[0].events = POLLIN;
    pfds[1].fd = browser_socket_;
    pfds[1].events = POLLIN;

    bool failed_polls = 0;
    for (;;) {
      const int r = HANDLE_EINTR(poll(pfds, 2, -1));
      if (r < 1) {
        LOG(WARNING) << "poll errno:" << errno;
        if (failed_polls++ == 3) {
          LOG(FATAL) << "poll failing. Sandbox host aborting.";
          return;
        }
        continue;
      }

      failed_polls = 0;

      if (pfds[0].revents) {
        // our parent died so we should too.
        _exit(0);
      }

      if (pfds[1].revents) {
        HandleRequestFromRenderer(browser_socket_);
      }
    }
  }

  // ---------------------------------------------------------------------------
  // WebKitClient impl...

  virtual WebClipboard* clipboard() { return NULL; }
  virtual WebMimeRegistry* mimeRegistry() { return NULL; }
  virtual WebSandboxSupport* sandboxSupport() { return NULL; }
  virtual WebThemeEngine* themeEngine() { return NULL; }

  virtual unsigned long long visitedLinkHash(const char*, size_t) { return 0; }
  virtual bool isLinkVisited(unsigned long long) { return false; }

  virtual void setCookies(const WebURL&, const WebURL&, const WebString&) { }
  virtual WebString cookies(const WebURL&, const WebURL&) { return WebString(); }

  virtual void prefetchHostName(const WebString&) { }

  virtual bool getFileSize(const WebString& path, long long& result) {
    return false;
  }

  virtual WebURLLoader* createURLLoader() { return NULL; }

  virtual void getPluginList(bool refresh, WebPluginListBuilder*) { }

  virtual void decrementStatsCounter(const char*) { }
  virtual void incrementStatsCounter(const char*) { }

  virtual void traceEventBegin(const char* name, void*, const char*) { }
  virtual void traceEventEnd(const char* name, void*, const char*) { }

  virtual WebData loadResource(const char*) { return WebData(); }

  virtual void suddenTerminationChanged(bool) { }

  virtual WebString defaultLocale() { return WebString(); }

  virtual double currentTime() { return 0; }

  virtual void setSharedTimerFiredFunction(void (*)()) { }
  virtual void setSharedTimerFireTime(double) { }
  virtual void stopSharedTimer() { }

  virtual void callOnMainThread(void (*)()) { }

 private:
  // ---------------------------------------------------------------------------
  // Requests from the renderer...

  void HandleRequestFromRenderer(int fd) {
    std::vector<int> fds;
    static const unsigned kMaxMessageLength = 2048;
    char buf[kMaxMessageLength];
    const ssize_t len = base::RecvMsg(fd, buf, sizeof(buf), &fds);
    if (len == -1)
      return;
    if (fds.size() == 0)
      return;

    Pickle pickle(buf, len);
    void* iter = NULL;

    int kind;
    if (!pickle.ReadInt(&iter, &kind))
      goto error;

    if (kind == FontConfigIPC::METHOD_MATCH) {
      HandleFontMatchRequest(fd, pickle, iter, fds);
    } else if (kind == FontConfigIPC::METHOD_OPEN) {
      HandleFontOpenRequest(fd, pickle, iter, fds);
    } else if (kind == LinuxSandbox::METHOD_GET_FONT_FAMILY_FOR_CHARS) {
      HandleGetFontFamilyForChars(fd, pickle, iter, fds);
    }

  error:
    for (std::vector<int>::const_iterator
         i = fds.begin(); i != fds.end(); ++i) {
      close(*i);
    }
  }

  void HandleFontMatchRequest(int fd, Pickle& pickle, void* iter,
                              std::vector<int>& fds) {
    bool fileid_valid;
    uint32_t fileid;
    bool is_bold, is_italic;
    std::string family;

    if (!pickle.ReadBool(&iter, &fileid_valid))
      return;
    if (fileid_valid) {
      if (!pickle.ReadUInt32(&iter, &fileid))
        return;
    }
    if (!pickle.ReadBool(&iter, &is_bold) ||
        !pickle.ReadBool(&iter, &is_italic) ||
        !pickle.ReadString(&iter, &family)) {
      return;
    }

    std::string result_family;
    unsigned result_fileid;

    const bool r = font_config_->Match(
        &result_family, &result_fileid, fileid_valid, fileid, family, &is_bold,
        &is_italic);

    Pickle reply;
    if (!r) {
      reply.WriteBool(false);
    } else {
      reply.WriteBool(true);
      reply.WriteUInt32(result_fileid);
      reply.WriteString(result_family);
      reply.WriteBool(is_bold);
      reply.WriteBool(is_italic);
    }
    SendRendererReply(fds, reply, -1);
  }

  void HandleFontOpenRequest(int fd, Pickle& pickle, void* iter,
                             std::vector<int>& fds) {
    uint32_t fileid;
    if (!pickle.ReadUInt32(&iter, &fileid))
      return;
    const int result_fd = font_config_->Open(fileid);

    Pickle reply;
    if (result_fd == -1) {
      reply.WriteBool(false);
    } else {
      reply.WriteBool(true);
    }

    SendRendererReply(fds, reply, result_fd);
  }

  void HandleGetFontFamilyForChars(int fd, Pickle& pickle, void* iter,
                                   std::vector<int>& fds) {
    // The other side of this call is
    // chrome/renderer/renderer_sandbox_support_linux.cc

    int num_chars;
    if (!pickle.ReadInt(&iter, &num_chars))
      return;

    // We don't want a corrupt renderer asking too much of us, it might
    // overflow later in the code.
    static const int kMaxChars = 4096;
    if (num_chars < 1 || num_chars > kMaxChars) {
      LOG(WARNING) << "HandleGetFontFamilyForChars: too many chars: "
                   << num_chars;
      return;
    }

    scoped_array<WebUChar> chars(new WebUChar[num_chars]);

    for (int i = 0; i < num_chars; ++i) {
      uint32_t c;
      if (!pickle.ReadUInt32(&iter, &c)) {
        return;
      }

      chars[i] = c;
    }

    const WebString family = WebFontInfo::familyForChars(chars.get(), num_chars);
    const std::string family_utf8 = UTF16ToUTF8(family);

    Pickle reply;
    reply.WriteString(family_utf8);
    SendRendererReply(fds, reply, -1);
  }

  void SendRendererReply(const std::vector<int>& fds, const Pickle& reply,
                         int reply_fd) {
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    struct iovec iov = {const_cast<void*>(reply.data()), reply.size()};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    char control_buffer[CMSG_SPACE(sizeof(int))];

    if (reply_fd != -1) {
      struct cmsghdr *cmsg;

      msg.msg_control = control_buffer;
      msg.msg_controllen = sizeof(control_buffer);
      cmsg = CMSG_FIRSTHDR(&msg);
      cmsg->cmsg_level = SOL_SOCKET;
      cmsg->cmsg_type = SCM_RIGHTS;
      cmsg->cmsg_len = CMSG_LEN(sizeof(int));
      memcpy(CMSG_DATA(cmsg), &reply_fd, sizeof(int));
      msg.msg_controllen = cmsg->cmsg_len;
    }

    HANDLE_EINTR(sendmsg(fds[0], &msg, MSG_DONTWAIT));
  }

  // ---------------------------------------------------------------------------

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
