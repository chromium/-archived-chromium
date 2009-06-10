// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/zygote_manager.h"

#if defined(OS_LINUX)

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/file.h>     // for flock()
#include <sys/stat.h>
#include <sys/uio.h>      // for struct iovec
#include <sys/wait.h>
#include <unistd.h>       // for ssize_t

#include <string>

#include "base/eintr_wrapper.h"
#include "base/file_descriptor_shuffle.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/reserved_file_descriptors.h"
#include "base/singleton.h"

using file_util::Delete;

// See comment below, where sigaction is called.
static void SIGCHLDHandler(int signal) {
}

namespace base {

const char ZygoteManager::kZMagic[] = "zygo";

ZygoteManager::~ZygoteManager() {
  if (server_fd_ != -1) {
    close(server_fd_);
    server_fd_ = -1;
  }
  if (client_fd_ != -1) {
    close(client_fd_);
    client_fd_ = -1;
  }
  if (lockfd_ != -1) {
    close(lockfd_);
    lockfd_ = -1;
  }
  if (canary_fd_ != -1) {
    // Wake up the poll() in ReadAndHandleMessage()
    close(canary_fd_);
    canary_fd_ = -1;
  }
#ifndef OFFICIAL_BUILD
  // Closing the canary kills the server,
  // so after this it's ok for e.g. unit tests
  // to start a new zygote server.
  (void) unsetenv("ZYGOTE_MANAGER_STARTED");
#endif
}

// Runs in client process
ZygoteManager* ZygoteManager::Get() {
  static bool checked = false;
  static bool enabled = false;
  if (!checked) {
    enabled = (getenv("ENABLE_ZYGOTE_MANAGER") != NULL);
    checked = true;
  }
  if (!enabled)
    return NULL;
  return Singleton<ZygoteManager>::get();
}

// Runs in zygote manager process
int ZygoteManager::UnpickleHeader(const Pickle& reply, void** iter) {
  std::string magic;
  if (!reply.ReadString(iter, &magic) || magic != std::string(kZMagic)) {
    LOG(ERROR) << "reply didn't start with " << kZMagic;
    return ZMBAD;
  }
  pid_t clientpid = (pid_t) -1;
  if (!reply.ReadInt(iter, &clientpid)) {
    LOG(ERROR) << "Can't read client pid";
    return ZMBAD;
  }
  if (clientpid != getpid()) {
    LOG(ERROR) << "got client pid " << clientpid << ", expected " << getpid();
    DCHECK(clientpid == getpid());
    return ZMBAD;
  }
  int kind;
  if (!reply.ReadInt(iter, &kind)) {
    LOG(ERROR) << "can't read kind";
    return ZMBAD;
  }
  return kind;
}

// Runs in client process (only used in unit test)
bool ZygoteManager::Ping(base::TimeDelta* delta) {
  if (client_fd_ == -1)
    return false;

  Pickle pickle;
  pickle.WriteString(kZMagic);
  pickle.WriteInt(getpid());
  pickle.WriteInt(ZMPING);

  int bytes_sent;
  int bytes_read = -1;

  TimeTicks time_sent = TimeTicks::HighResNow();

  // Lock fork server, send the pickle, wait for the reply, unlock
  if (flock(lockfd_, LOCK_EX))
    LOG(ERROR) << "flock failed, errno " << errno;
  bytes_sent = HANDLE_EINTR(write(client_fd_,
      const_cast<void *>(pickle.data()), pickle.size()));
  if (bytes_sent > 0) {
    bytes_read = HANDLE_EINTR(read(client_fd_, msg_buf_, kMAX_MSG_LEN));
  }
  if (flock(lockfd_, LOCK_UN))
    LOG(ERROR) << "flock failed, errno " << errno;

  TimeTicks time_received = TimeTicks::HighResNow();

  if (bytes_sent < 1) {
    LOG(ERROR) << "Can't send to zm, errno " << errno;
    return false;
  }
  if (bytes_read < 1) {
    LOG(ERROR) << "Can't get from zm, errno " << errno;
    return false;
  }

  // Unpickle the reply
  Pickle reply(msg_buf_, bytes_read);
  void* iter = NULL;
  int kind = UnpickleHeader(reply, &iter);
  if (kind != ZMPINGED) {
    LOG(ERROR) << "reply wrong kind " << kind;
    return false;
  }
  *delta = TimeTicks::HighResNow() - time_sent;
  LOG(INFO) << "Round trip time in microseconds: " << delta->InMicroseconds();
  return true;
}

// Runs in zygote manager process
void ZygoteManager::PingHandler(const Pickle& request, void* iter,
    Pickle* reply, std::vector<std::string>** newargv) {
    reply->WriteInt(ZMPINGED);
}

// Runs in browser process, called only by base::ForkApp()
pid_t ZygoteManager::LongFork(const std::vector<std::string>& argv,
           const file_handle_mapping_vector& fds_to_remap) {
  if (client_fd_ == -1)
    return -1;

  Pickle pickle;

  // Encode the arguments and the desired remote fd numbers in the pickle,
  // and the fds in a separate buffer
  pickle.WriteString(kZMagic);
  pickle.WriteInt(getpid());
  pickle.WriteInt(ZMFORK);
  pickle.WriteInt(argv.size());
  std::vector<std::string>::const_iterator argi;
  for (argi = argv.begin(); argi != argv.end(); ++argi)
    pickle.WriteString(*argi);
  pickle.WriteInt(fds_to_remap.size());

  // Wrap the pickle and the fds together in a msghdr
  ::msghdr msg;
  memset(&msg, 0, sizeof(msg));
  struct iovec iov;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsg_buf_;
  msg.msg_controllen = CMSG_LEN(sizeof(int) * fds_to_remap.size());
  struct cmsghdr* cmsg;
  cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = msg.msg_controllen;
  int* wire_fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
  for (size_t i = 0; i < fds_to_remap.size(); i++) {
    pickle.WriteInt(fds_to_remap[i].second);
    wire_fds[i] = fds_to_remap[i].first;
  }
  iov.iov_base = const_cast<void *>(pickle.data());
  iov.iov_len = pickle.size();

  int bytes_sent;
  int bytes_read = -1;

  // Lock fork server, send the pickle, wait for the reply, unlock
  if (flock(lockfd_, LOCK_EX))
    LOG(ERROR) << "flock failed, errno " << errno;
  bytes_sent = HANDLE_EINTR(sendmsg(client_fd_, &msg, MSG_WAITALL));
  if (bytes_sent > 0) {
    bytes_read = HANDLE_EINTR(read(client_fd_, msg_buf_, kMAX_MSG_LEN));
  }
  if (flock(lockfd_, LOCK_UN))
    LOG(ERROR) << "flock failed, errno " << errno;

  if (bytes_sent < 1) {
    LOG(ERROR) << "Can't send to zm, errno " << errno << ", fd " << client_fd_;
    return (pid_t) -1;
  }
  if (bytes_read < 1) {
    LOG(ERROR) << "Can't get from zm, errno " << errno;
    return (pid_t) -1;
  }

  // Unpickle the reply
  Pickle reply(msg_buf_, bytes_read);
  void* iter = NULL;
  int kind = UnpickleHeader(reply, &iter);
  if (kind != ZMFORKED) {
    LOG(ERROR) << "reply wrong kind " << kind;
    return (pid_t) -1;
  }
  pid_t newpid = (pid_t) -1;
  int pid_errno;
  if (!reply.ReadInt(&iter, &newpid) || !reply.ReadInt(&iter, &pid_errno)) {
    LOG(ERROR) << "fork failed, can't read pid/errno";
    return (pid_t) -1;
  }
  if ((newpid == (pid_t) -1) || pid_errno != 0) {
    LOG(ERROR) << "fork failed, pid " << newpid << ", errno " << pid_errno;
    return (pid_t) -1;
  }
  return newpid;
}

// Runs in zygote manager process
bool ZygoteManager::LongForkHandler(const Pickle& request, void* iter,
    Pickle* reply, std::vector<std::string>** newargv,
    const int wire_fds[], int num_wire_fds) {
  file_handle_mapping_vector fds_to_remap;
  pid_t childpid;

  reply->WriteInt(ZMFORKED);

  // Unpickle commandline for new child
  std::vector<std::string>* argv = new std::vector<std::string>;
  int argc;
  request.ReadInt(&iter, &argc);
  for (int i = 0; i < argc; i++) {
    std::string arg;
    if (!request.ReadString(&iter, &arg)) {
      LOG(ERROR) << "can't read arg?";
      goto error_reply;
    }
    argv->push_back(arg);
  }
  // Unpickle file descriptor map for new child
  int numfds;
  request.ReadInt(&iter, &numfds);
  DCHECK(numfds == num_wire_fds);
  if (numfds != num_wire_fds) {
    LOG(ERROR) << "numfds " << numfds << " != num_wire_fds " << num_wire_fds;
    goto error_reply;
  }
  for (int i = 0; i < numfds; i++) {
    int fd;
    if (!request.ReadInt(&iter, &fd)) {
      LOG(ERROR) << "can't read fd?";
      goto error_reply;
    }
    fds_to_remap.push_back(std::pair<int, int>(wire_fds[i], fd));
  }

  // Mitosis!
  childpid = fork();

  if (childpid != 0) {
    // parent
    // first off, close our copy of the child's file descriptors
    for (file_handle_mapping_vector::const_iterator
        it = fds_to_remap.begin(); it != fds_to_remap.end(); ++it) {
      close(it->first);
    }

    // Finish formatting the reply
    reply->WriteInt(childpid);
    if (childpid == (pid_t) -1) {
      reply->WriteInt(errno);
      return false;
    } else {
      reply->WriteInt(0);
    }
  } else {
    // child
    // Apply file descriptor map
    InjectiveMultimap fd_shuffle;
    for (file_handle_mapping_vector::const_iterator
        it = fds_to_remap.begin(); it != fds_to_remap.end(); ++it) {
      fd_shuffle.push_back(InjectionArc(it->first, it->second, false));
    }

    // Avoid closing descriptor children will need to contact fork server.
    fd_shuffle.push_back(InjectionArc(client_fd_, client_fd_, false));
    // Avoid closing log descriptor we're using
    int logfd = logging::GetLoggingFileDescriptor();
    if (logfd != -1)
      fd_shuffle.push_back(InjectionArc(logfd, logfd, false));
    // And of course avoid closing the cached fds.
    std::map<std::string, int>::iterator i;
    for (i = cached_fds_.begin(); i != cached_fds_.end(); ++i) {
      fd_shuffle.push_back(InjectionArc(i->second, i->second, false));
    }

    // If there is any clash in the mapping, this function will DCHECK.
    if (!ShuffleFileDescriptors(fd_shuffle))
      exit(127);

    // Open this after shuffle to avoid using reserved slots.
    lockfd_ = open(lockfile_.c_str(), O_RDWR, 0);
    if (lockfd_ == -1) {
      // TODO(dkegel): real error handling
      exit(126);
    }
    // Mark it as not to be closed.
    fd_shuffle.push_back(InjectionArc(lockfd_, lockfd_, false));

    // Also closes reserved fds.
    CloseSuperfluousFds(fd_shuffle);

    *newargv = argv;
    // Because *newargv is set, we will return to main instead of looping
  }
  return true;

 error_reply:
  reply->WriteInt(-1);
  reply->WriteInt(-1);
  for (int i=0; i<num_wire_fds; i++)
    close(wire_fds[i]);
  return false;
}

// Runs in browser process, called by ProcessWatcher::EnsureProcessTerminated().
void ZygoteManager::EnsureProcessTerminated(pid_t childpid) {
  if (client_fd_ == -1)
    return;

  Pickle pickle;

  pickle.WriteString(kZMagic);
  pickle.WriteInt(getpid());
  pickle.WriteInt(ZMREAP);
  pickle.WriteInt(childpid);

  int bytes_sent = HANDLE_EINTR(
      write(client_fd_, const_cast<void*>(pickle.data()), pickle.size()));

  if (bytes_sent < 1) {
    LOG(ERROR) << "Can't send to zm, errno " << errno << ", fd " << client_fd_;
  }
}

// Runs in zygote manager process
void ZygoteManager::EnsureProcessTerminatedHandler(const Pickle& request,
    void* iter) {
  pid_t childpid;
  request.ReadInt(&iter, &childpid);
  NOTIMPLEMENTED();
  // TODO(dkegel): put childpid on a watch list, and terminate it
  // after a while as chrome/common/process_watcher does.
}

static bool ValidateFilename(const std::string& filename) {
  // We only have to open one kind of file, but we don't know
  // the directory it's in, so be as restrictive as we can within
  // those bounds.

  static const char* allowed_prefix = "/";
  if (filename.compare(0, strlen(allowed_prefix), allowed_prefix) != 0) {
    LOG(ERROR) << "filename did not start with " << allowed_prefix;
    return false;
  }
  static const char* allowed_suffix = ".pak";
  if ((filename.length() <= strlen(allowed_suffix)) ||
      (filename.compare(filename.length() - strlen(allowed_suffix),
                       strlen(allowed_suffix), allowed_suffix) != 0)) {
    LOG(ERROR) << "filename did not end in " << allowed_suffix;
    return false;
  }
  if (filename.find("../") != std::string::npos) {
    LOG(ERROR) << "filename contained relative component";
    return false;
  }
  static const char* forbidden_prefixes[] = {
    "/var/", "/tmp/", "/etc/", "/dev/", "/proc/", 0 };
  for (const char** p = forbidden_prefixes;
       *p; p++) {
    if (filename.compare(0, strlen(*p), *p) == 0) {
      LOG(ERROR) << "filename began with " << *p;
      return false;
    }
  }
  return true;
}

// Runs in browser process
int ZygoteManager::OpenFile(const std::string& filename) {
  // For security reasons, we only support .pak files,
  // and only in certain locations.
  if (!ValidateFilename(filename)) {
    LOG(INFO) << "ZygoteManager: filename " << filename << " disallowed.";
    return -1;
  }

  std::map<std::string, int>::iterator mapiter = cached_fds_.find(filename);
  if (mapiter != cached_fds_.end())
    return mapiter->second;

  if (client_fd_ == -1)
    return -1;

  Pickle pickle;

  pickle.WriteString(kZMagic);
  pickle.WriteInt(getpid());
  pickle.WriteInt(ZMOPEN);
  pickle.WriteString(filename);

  // Get ready to receive fds
  ::msghdr msg = {0};
  struct iovec iov = {msg_buf_, kMAX_MSG_LEN};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsg_buf_;
  msg.msg_controllen = kMAX_CMSG_LEN;

  ssize_t bytes_sent;
  ssize_t bytes_read = 0;

  if (flock(lockfd_, LOCK_EX))
    LOG(ERROR) << "flock failed, errno " << errno;
  bytes_sent = HANDLE_EINTR(
      write(client_fd_, const_cast<void *>(pickle.data()), pickle.size()));
  if (bytes_sent > 0) {
    bytes_read = HANDLE_EINTR(recvmsg(client_fd_, &msg, MSG_WAITALL));
  }
  if (flock(lockfd_, LOCK_UN))
    LOG(ERROR) << "flock failed, errno " << errno;

  if (bytes_sent < 1) {
    LOG(ERROR) << "Can't send to zm, errno " << errno << ", fd " << client_fd_;
    return -1;
  }
  if (bytes_read < 1) {
    LOG(ERROR) << "Can't get from zm, errno " << errno;
    return -1;
  }

  // Locate the sole block of sent file descriptors within the list of
  // control messages
  const int* wire_fds = NULL;
  unsigned num_wire_fds = 0;
  if (msg.msg_controllen > 0) {
    struct cmsghdr* cmsg;
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg;
         cmsg = CMSG_NXTHDR(&msg, cmsg)) {
      if (cmsg->cmsg_level == SOL_SOCKET &&
          cmsg->cmsg_type == SCM_RIGHTS) {
        const unsigned payload_len = cmsg->cmsg_len - CMSG_LEN(0);
        assert(payload_len % sizeof(int) == 0);
        wire_fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
        num_wire_fds = payload_len / sizeof(int);
        break;
      }
    }
  }
  DCHECK(!(msg.msg_flags & MSG_CTRUNC));

  // Unpickle the reply
  Pickle reply(msg_buf_, bytes_read);
  void* iter = NULL;
  int kind = UnpickleHeader(reply, &iter);
  if (kind != ZMOPENED) {
    LOG(ERROR) << "reply wrong kind " << kind;
    goto error_close;
  }
  int newfd_errno;
  if (!reply.ReadInt(&iter, &newfd_errno)) {
    LOG(ERROR) << "open failed, can't read errno";
    goto error_close;
  }
  if (newfd_errno != 0) {
    LOG(ERROR) << "open failed, errno " << newfd_errno;
    goto error_close;
  }
  if (num_wire_fds != 1) {
    LOG(ERROR) << "open failed, reply wrong number fds " << num_wire_fds;
    goto error_close;
  }
  if (wire_fds[0] == -1) {
    LOG(ERROR) << "open failed, fd -1";
    NOTREACHED();
    return -1;
  }
  return wire_fds[0];

 error_close:
  for (unsigned i=0; i<num_wire_fds; i++)
    close(wire_fds[i]);
  return -1;
}

// Runs in zygote manager process
bool ZygoteManager::OpenFileHandler(const Pickle& request, void* iter,
    Pickle* reply, ::msghdr* msg) {
  reply->WriteInt(ZMOPENED);

  std::string filename;
  if (!request.ReadString(&iter, &filename)) {
    LOG(ERROR) << "no filename?";
    return false;
  }
  if (!ValidateFilename(filename)) {
    // Fake a unix error code
    reply->WriteInt(EPERM);
    return false;
  }

  std::map<std::string, int>::iterator i;
  int newfd;
  std::map<std::string, int>::iterator mapiter = cached_fds_.find(filename);
  if (mapiter == cached_fds_.end()) {
    // Verify that file is a plain file
    struct stat statbuf;
    if (lstat(filename.c_str(), &statbuf) != 0) {
      LOG(ERROR) << "can't stat " << filename << ", errno " << errno;
      return false;
    }
    if (!S_ISREG(statbuf.st_mode)) {
      LOG(ERROR) << "not regular file " << filename;
      // Fake a unix error code
      reply->WriteInt(EISDIR);
      return false;
    }
    newfd = open(filename.c_str(), O_RDONLY, 0);
    if (newfd != -1) {
      cached_fds_[filename] = newfd;
    } else {
      LOG(ERROR) << "can't open " << filename << ", errno " << errno;
    }
  } else {
    newfd = mapiter->second;
  }
  if (newfd == -1) {
    reply->WriteInt(errno);
  } else {
    reply->WriteInt(0);
    msg->msg_control = cmsg_buf_;
    msg->msg_controllen = CMSG_LEN(sizeof(int));
    struct cmsghdr* cmsg;
    cmsg = CMSG_FIRSTHDR(msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = msg->msg_controllen;
    int* wire_fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
    wire_fds[0] = newfd;
  }

  return true;
}

// Runs in zygote manager process
bool ZygoteManager::ReadAndHandleMessage(std::vector<std::string>** newargv) {
  // Wait for activity either on canary fd or main fd.
  struct pollfd watcher[2];
  memset(watcher, 0, sizeof(watcher));
  watcher[0].fd = canary_fd_;
  watcher[0].events = POLLIN|POLLHUP;
  watcher[1].fd = server_fd_;
  watcher[1].events = POLLIN;
  // Wait at most one minute.  This lets us detect case where
  // canary socket is closed abruptly because the main client aborted.
  // Also lets us reap dead children once a minute even if we don't get SIGCHLD.
  // We'd like to wait less time, but that's hard on battery life.
  // Note: handle EINTR manually here, not with wrapper, as we need
  // to return when we're interrupted so caller can reap promptly.
  int nactive = poll(watcher, 2, 60*1000);

  if (nactive == -1) {
    if (errno == EINTR) {
      // Probably SIGCHLD.  Return to main loop so it can reap.
      return true;
    }
    LOG(ERROR) << "poll failed, errno " << errno << ", aborting";
    return false;
  }

  // If it was the canary, exit
  if (watcher[0].revents != 0) {
    LOG(INFO) << "notified of peer destruction, exiting";
    return false;
  }
  if ((watcher[1].revents & POLLIN) != POLLIN) {
    // spurious wakeup?
    return true;
  }

  ssize_t bytes_read = 0;
  struct msghdr msg = {0};
  struct iovec iov = {msg_buf_, kMAX_MSG_LEN};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsg_buf_;
  msg.msg_controllen = kMAX_CMSG_LEN;
  bytes_read = HANDLE_EINTR(recvmsg(server_fd_, &msg, MSG_WAITALL));
  if (bytes_read == 0) {
    LOG(ERROR) << "got EOF, aborting";
    return false;
  }
  if (bytes_read == -1) {
    LOG(ERROR) << "got errno " << errno << ", aborting";
    return false;
  }

  // Locate the sole block of sent file descriptors within the list of
  // control messages
  const int* wire_fds = NULL;
  unsigned num_wire_fds = 0;
  if (msg.msg_controllen > 0) {
    struct cmsghdr* cmsg;
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg;
         cmsg = CMSG_NXTHDR(&msg, cmsg)) {
      if (cmsg->cmsg_level == SOL_SOCKET &&
          cmsg->cmsg_type == SCM_RIGHTS) {
        const unsigned payload_len = cmsg->cmsg_len - CMSG_LEN(0);
        assert(payload_len % sizeof(int) == 0);
        wire_fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
        num_wire_fds = payload_len / sizeof(int);
        break;
      }
    }
  }
  DCHECK(!(msg.msg_flags & MSG_CTRUNC));

  // Unpickle/parse message
  Pickle pickle(msg_buf_, bytes_read);
  void* iter = NULL;
  std::string magic;
  if (!pickle.ReadString(&iter, &magic) || magic != std::string(kZMagic)) {
    LOG(ERROR) << "msg didn't start with " << kZMagic << ", got " << magic;
    for (unsigned i=0; i<num_wire_fds; i++)
      close(wire_fds[i]);
    return true;
  }
  pid_t clientpid = (pid_t) -1;
  pickle.ReadInt(&iter, &clientpid);
  int kind;
  pickle.ReadInt(&iter, &kind);

  Pickle reply;
  reply.WriteString(kZMagic);
  reply.WriteInt(clientpid);

  struct msghdr replymsg = {0};
  memset(&replymsg, 0, sizeof(replymsg));

  switch (kind) {
  case ZMPING:
    DCHECK_EQ(0U, num_wire_fds);
    PingHandler(pickle, iter, &reply, newargv);
    break;
  case ZMFORK:
    // TODO(dkegel): real error handling
    (void) LongForkHandler(pickle, iter, &reply, newargv, wire_fds,
                           num_wire_fds);
    if (*newargv != NULL) {
      // Child.  Just return to caller, who will return from SetLongFork.
      return true;
    }
    break;
  case ZMREAP:
    DCHECK_EQ(0U, num_wire_fds);
    EnsureProcessTerminatedHandler(pickle, iter);
    // no reply to this message
    return true;
  case ZMOPEN:
    DCHECK_EQ(0U, num_wire_fds);
    // TODO(dkegel): real error handling
    (void) OpenFileHandler(pickle, iter, &reply, &replymsg);
    break;
  default:
    // TODO(dkegel): real error handling
    LOG(ERROR) << "Unknown message kind " << kind;
    DCHECK_EQ(0U, num_wire_fds);
    break;
  }

  struct iovec riov = {const_cast<void *>(reply.data()), reply.size()};
  replymsg.msg_iov = &riov;
  replymsg.msg_iovlen = 1;

  int bytes_sent;
  bytes_sent = HANDLE_EINTR(sendmsg(server_fd_, &replymsg, MSG_WAITALL));
  if (bytes_sent != static_cast<int>(riov.iov_len)) {
    // TODO(dkegel): real error handling
    LOG(ERROR) << "Can't send reply.";
    return false;
  }
  return true;
}

// Called only by ChromeMain(), forks the zygote manager process.
std::vector<std::string>* ZygoteManager::Start() {
  DCHECK(lockfd_ == -1);
  DCHECK(canary_fd_ == -1);
  DCHECK(server_fd_ == -1);
  DCHECK(client_fd_ == -1);

#ifndef OFFICIAL_BUILD
  // Disallow nested ZygoteManager servers
  CHECK(getenv("ZYGOTE_MANAGER_STARTED") == NULL) << "already started?!";
  (void) setenv("ZYGOTE_MANAGER_STARTED", "1", 1);
#endif

  int pipe_fds[2];

  // Avoid using the reserved fd slots.
  int reserved_fds[kReservedFds];
  for (int i=0; i < kReservedFds; i++)
    reserved_fds[i] = open("/dev/null", O_RDONLY, 0);

  // Create the main communications pipe.
  int err = HANDLE_EINTR(socketpair(AF_UNIX, SOCK_DGRAM, 0, pipe_fds));
  if (err != 0) {
    // TODO(dkegel): real error handling
    exit(99);
  }
  server_fd_ = pipe_fds[1];
  client_fd_ = pipe_fds[0];

  // Create the pipe used only to relay destruction event server.
  // Must be SOCK_STREAM so close() is sensed by poll().
  err = HANDLE_EINTR(socketpair(AF_UNIX, SOCK_STREAM, 0, pipe_fds));
  if (err != 0) {
    // TODO(dkegel): real error handling
    exit(99);
  }

  // Create lock file.
  // TODO(dkegel): get rid of this
  char lockfile[256];
  strcpy(lockfile, "/tmp/zygote_manager_lock.XXXXXX");
  lockfd_ = mkstemp(lockfile);
  if (lockfd_ == -1) {
    // TODO(dkegel): real error handling
    exit(99);
  }
  lockfile_.assign(lockfile);

  // Fork a fork server.
  pid_t childpid = fork();

  if (!childpid) {
    for (int i=0; i < kReservedFds; i++)
      close(reserved_fds[i]);

    // Original child.  Continues on with the main program
    // and becomes the first client.
    close(server_fd_);
    server_fd_ = -1;

    close(pipe_fds[1]);
    canary_fd_ = pipe_fds[0];

    // Return now to indicate this is the original process.
    return NULL;
  } else {
    close(lockfd_);

    close(pipe_fds[0]);
    canary_fd_ = pipe_fds[1];

    // We need to accept SIGCHLD, even though our handler is a no-op because
    // otherwise we cannot wait on children. (According to POSIX 2001.)
    // (And otherwise poll() might not wake up on SIGCHLD.)
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = SIGCHLDHandler;
    CHECK(sigaction(SIGCHLD, &action, NULL) == 0);

    // Original process.  Acts as the server.
    while (true) {
      std::vector<std::string>* newargv = NULL;
      if (!ReadAndHandleMessage(&newargv))
        break;
      if (newargv) {
        // Return new commandline to show caller this is a new child process.
        return newargv;
      }
      // Server process continues around loop.

      // Reap children.
      while (true) {
        int status = -1;
        pid_t reaped = waitpid(-1, &status, WNOHANG);
        if (reaped != -1 && reaped != 0) {
          LOG(INFO) << "Reaped pid " << reaped;
          continue;
        }
        break;
      }
    }
    // Server cleanup after EOF or error reading from the socket.
    Delete(FilePath(lockfile_), false);
    // TODO(dkegel): real error handling
    LOG(INFO) << "exiting.  " << cached_fds_.size() << " cached fds.";
    std::map<std::string, int>::iterator i;
    for (i = cached_fds_.begin(); i != cached_fds_.end(); ++i) {
      LOG(INFO) << "Closing fd " << i->second << " filename " << i->first;
      close(i->second);
    }
    exit(0);
  }
}
}
#endif  // defined(OS_LINUX)
