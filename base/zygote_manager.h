// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ZYGOTE_MANAGER_H_
#define BASE_ZYGOTE_MANAGER_H_

#include "build/build_config.h"

#if defined(OS_LINUX)

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/pickle.h"
#include "base/time.h"
#include "base/process_util.h"  // for file_handle_mapping_vector

namespace base {

class ZygoteManager {
 public:
  // The normal way to get a ZygoteManager is via this singleton factory.
  static ZygoteManager* Get();

  ZygoteManager() :
      server_fd_(-1), client_fd_(-1), canary_fd_(-1), lockfd_(-1) {
  }

  ~ZygoteManager();

  // Measure round trip time.  Return true on success.
  // Only used during testing.
  bool Ping(base::TimeDelta* delta);

  // Start the zygote manager.
  // Called only once, but returns many times.
  // Returns once in original process and once in each spawned child.
  // In original process, returns NULL.
  // In child processes, returns the argv to use for the child.
  // In Chromium, called from ChromeMain().
  std::vector<std::string>* Start();

  // Like longjmp() and base::LaunchApp().
  // Ask the fork server to spawn a new process with
  // the given commandline and the given file descriptors.
  // Returns process id of copy, or -1 on failure.
  // In Chromium, called from base::ForkApp(), which is
  // called from BrowserRenderProcessHost::Init().
  pid_t LongFork(const std::vector<std::string>& argv,
    const file_handle_mapping_vector& fds_to_remap);

  // Tell ZygoteManager that we expect the given process to
  // exit on its own soon.  If it doesn't die within a few
  // seconds, kill it.  Does not block (unless pipe to server full).
  // In Chromium, called from ProcessWatcher::EnsureProcessTerminated().
  void EnsureProcessTerminated(pid_t childpid);

  // Open a file, or retrieve a previously cached file descriptor
  // for this file.  The files are opened for readonly access.
  // Caution: do not seek file descriptors returned
  // by this API, as all children share the same file objects, so
  // a seek on one is a seek on all.
  // Works even if the file is unlinked after the first call
  // (e.g. when an app is updated by the linux system autoupdater).
  // Returns file descriptor, or -1 for error.
  // In Chromium, called from MemoryMappedFile::MapFileToMemory().
  // Only allows openeing files named .pak in reasonable looking locations.
  int OpenFile(const std::string& filename);

 private:
  int UnpickleHeader(const Pickle& reply, void** iter);

  // Returns false on EOF
  // Sets *newargv to a new commandline if the remote side requested a fork.
  bool ReadAndHandleMessage(std::vector<std::string>** newargv);

  void PingHandler(const Pickle& request, void* iter, Pickle* reply,
                   std::vector<std::string>** newargv);

  bool LongForkHandler(const Pickle& request, void* iter, Pickle* reply,
                   std::vector<std::string>** newargv,
                   const int wire_fds[], int num_wire_fds);

  void EnsureProcessTerminatedHandler(const Pickle& request, void* iter);

  bool OpenFileHandler(const Pickle& request, void* iter, Pickle* reply,
      ::msghdr* msg);

  // The fd used by the server to receive requests
  int server_fd_;

  // The fd used by the clients to send requests
  int client_fd_;

  // fd used only to notify server of destruction.
  int canary_fd_;

  // Temporary file used only for locking.
  // Each client must do its own open for locking to work;
  // inherited file descriptors can't lock each other out.
  // FIXME: locking is lame.
  std::string lockfile_;
  int lockfd_;

  enum message_kind_t { ZMPING, ZMPINGED,
                ZMFORK, ZMFORKED,
                ZMREAP,
                ZMOPEN, ZMOPENED,
                ZMBAD };

  // See common/reserved_file_descriptors.h for who uses the reserved
  // file descriptors.  kReservedFds is one plus the highest fd mentioned there.
  // TODO(dkegel): move kReservedFds to reserved_file_descriptors.h
  static const int kReservedFds = 5;

  static const int kMAX_MSG_LEN = 2000;
  static const int kMAX_CMSG_LEN = 100;

  static const char kZMagic[];

  char msg_buf_[kMAX_MSG_LEN];
  char cmsg_buf_[kMAX_CMSG_LEN];

  // Where we remember file descriptors for already-opened files.
  // Both client and server maintain this table.
  // Client should check the table before requesting the
  // server to open a file, as it might have been already
  // opened before this client was forked.
  std::map<std::string, int> cached_fds_;
};

}  // namespace base

#endif  // defined(OS_LINUX)

#endif  // BASE_ZYGOTE_MANAGER_H_
