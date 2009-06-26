// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/prctl.h>

#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/global_descriptors_posix.h"
#include "base/pickle.h"
#include "base/unix_domain_socket_posix.h"

#include "chrome/browser/zygote_host_linux.h"
#include "chrome/common/chrome_descriptors.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/process_watcher.h"

#include "skia/ext/SkFontHost_fontconfig_control.h"

// http://code.google.com/p/chromium/wiki/LinuxZygote

// This is the object which implements the zygote. The ZygoteMain function,
// which is called from ChromeMain, at the the bottom and simple constructs one
// of these objects and runs it.
class Zygote {
 public:
  bool ProcessRequests() {
    // A SOCK_SEQPACKET socket is installed in fd 3. We get commands from the
    // browser on it.
    // A SOCK_DGRAM is installed in fd 4. This is the sandbox IPC channel.
    // See http://code.google.com/p/chromium/wiki/LinuxSandboxIPC

    // We need to accept SIGCHLD, even though our handler is a no-op because
    // otherwise we cannot wait on children. (According to POSIX 2001.)
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = SIGCHLDHandler;
    CHECK(sigaction(SIGCHLD, &action, NULL) == 0);

    for (;;) {
      if (HandleRequestFromBrowser(3))
        return true;
    }
  }

 private:
  // See comment below, where sigaction is called.
  static void SIGCHLDHandler(int signal) { }

  // ---------------------------------------------------------------------------
  // Requests from the browser...

  // Read and process a request from the browser. Returns true if we are in a
  // new process and thus need to unwind back into ChromeMain.
  bool HandleRequestFromBrowser(int fd) {
    std::vector<int> fds;
    static const unsigned kMaxMessageLength = 1024;
    char buf[kMaxMessageLength];
    const ssize_t len = base::RecvMsg(fd, buf, sizeof(buf), &fds);
    if (len == -1) {
      LOG(WARNING) << "Error reading message from browser: " << errno;
      return false;
    }

    if (len == 0) {
      // EOF from the browser. We should die.
      _exit(0);
      return false;
    }

    Pickle pickle(buf, len);
    void* iter = NULL;

    int kind;
    if (pickle.ReadInt(&iter, &kind)) {
      switch (kind) {
        case ZygoteHost::kCmdFork:
          return HandleForkRequest(fd, pickle, iter, fds);
        case ZygoteHost::kCmdReap:
          if (!fds.empty())
            break;
          return HandleReapRequest(fd, pickle, iter);
        case ZygoteHost::kCmdDidProcessCrash:
          if (!fds.empty())
            break;
          return HandleDidProcessCrash(fd, pickle, iter);
        default:
          NOTREACHED();
          break;
      }
    }

    LOG(WARNING) << "Error parsing message from browser";
    for (std::vector<int>::const_iterator
         i = fds.begin(); i != fds.end(); ++i)
      close(*i);
    return false;
  }

  bool HandleReapRequest(int fd, Pickle& pickle, void* iter) {
    pid_t child;

    if (!pickle.ReadInt(&iter, &child)) {
      LOG(WARNING) << "Error parsing reap request from browser";
      return false;
    }

    ProcessWatcher::EnsureProcessTerminated(child);

    return false;
  }

  bool HandleDidProcessCrash(int fd, Pickle& pickle, void* iter) {
    base::ProcessHandle child;

    if (!pickle.ReadInt(&iter, &child)) {
      LOG(WARNING) << "Error parsing DidProcessCrash request from browser";
      return false;
    }

    bool child_exited;
    bool did_crash = base::DidProcessCrash(&child_exited, child);

    Pickle write_pickle;
    write_pickle.WriteBool(did_crash);
    write_pickle.WriteBool(child_exited);
    HANDLE_EINTR(write(fd, write_pickle.data(), write_pickle.size()));

    return false;
  }

  // Handle a 'fork' request from the browser: this means that the browser
  // wishes to start a new renderer.
  bool HandleForkRequest(int fd, Pickle& pickle, void* iter,
                         std::vector<int>& fds) {
    std::vector<std::string> args;
    int argc, numfds;
    base::GlobalDescriptors::Mapping mapping;
    pid_t child;

    if (!pickle.ReadInt(&iter, &argc))
      goto error;

    for (int i = 0; i < argc; ++i) {
      std::string arg;
      if (!pickle.ReadString(&iter, &arg))
        goto error;
      args.push_back(arg);
    }

    if (!pickle.ReadInt(&iter, &numfds))
      goto error;
    if (numfds != static_cast<int>(fds.size()))
      goto error;

    for (int i = 0; i < numfds; ++i) {
      base::GlobalDescriptors::Key key;
      if (!pickle.ReadUInt32(&iter, &key))
        goto error;
      mapping.push_back(std::make_pair(key, fds[i]));
    }

    mapping.push_back(std::make_pair(
        static_cast<uint32_t>(kSandboxIPCChannel), 5));

    child = fork();

    if (!child) {
      close(3);  // our socket from the browser is in fd 3
      Singleton<base::GlobalDescriptors>()->Reset(mapping);
      CommandLine::Reset();
      CommandLine::Init(args);
      return true;
    }

    for (std::vector<int>::const_iterator
         i = fds.begin(); i != fds.end(); ++i)
      close(*i);

    HANDLE_EINTR(write(fd, &child, sizeof(child)));
    return false;

   error:
    LOG(WARNING) << "Error parsing fork request from browser";
    for (std::vector<int>::const_iterator
         i = fds.begin(); i != fds.end(); ++i)
      close(*i);
    return false;
  }
};

static bool MaybeEnterChroot() {
  const char* const sandbox_fd_string = getenv("SBX_D");
  if (sandbox_fd_string) {
    // The SUID sandbox sets this environment variable to a file descriptor
    // over which we can signal that we have completed our startup and can be
    // chrooted.

    char* endptr;
    const long fd_long = strtol(sandbox_fd_string, &endptr, 10);
    if (!*sandbox_fd_string || *endptr || fd_long < 0 || fd_long > INT_MAX)
      return false;
    const int fd = fd_long;

    static const char kChrootMe = 'C';
    static const char kChrootMeSuccess = 'O';

    if (HANDLE_EINTR(write(fd, &kChrootMe, 1)) != 1)
      return false;

    char reply;
    if (HANDLE_EINTR(read(fd, &reply, 1)) != 1)
      return false;
    if (reply != kChrootMeSuccess)
      return false;
    if (chdir("/") == -1)
      return false;

    static const int kMagicSandboxIPCDescriptor = 4;
    SkiaFontConfigUseIPCImplementation(kMagicSandboxIPCDescriptor);

    prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);
    if (prctl(PR_GET_DUMPABLE, 0, 0, 0, 0))
      return false;
  } else {
    SkiaFontConfigUseDirectImplementation();
  }

  return true;
}

bool ZygoteMain(const MainFunctionParams& params) {
  if (!MaybeEnterChroot()) {
    LOG(FATAL) << "Failed to enter sandbox. Fail safe abort. (errno: "
               << errno << ")";
    return false;
  }

  Zygote zygote;
  return zygote.ProcessRequests();
}
