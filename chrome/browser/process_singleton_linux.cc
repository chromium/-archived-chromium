// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On Linux, when the user tries to launch a second copy of chrome, we check
// for a socket in the user's profile directory.  If the socket file is open we
// send a message to the first chrome browser process with the current
// directory and second process command line flags.  The second process then
// exits.

#include "chrome/browser/process_singleton.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <set>

#include "base/base_paths.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/browser_init.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"

namespace {

const char kStartToken[] = "START";
const char kTokenDelimiter = '\0';
const int kTimeOutInSeconds = 5;
const int kMaxMessageLength = 32 * 1024;

// Return 0 on success, -1 on failure.
int SetNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (-1 == flags)
    return flags;
  if (flags & O_NONBLOCK)
    return 0;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// ProcessSingleton::LinuxWatcher
// A helper class for a Linux specific implementation of the process singleton.
// This class sets up a listener on the singleton socket and handles parsing
// messages that come in on the singleton socket.
class ProcessSingleton::LinuxWatcher
    : public MessageLoopForIO::Watcher,
      public MessageLoop::DestructionObserver,
      public base::RefCountedThreadSafe<ProcessSingleton::LinuxWatcher> {
 public:
  // We expect to only be constructed on the UI thread.
  explicit LinuxWatcher(ProcessSingleton* parent)
      : ui_message_loop_(MessageLoop::current()),
        parent_(parent) {
  }

  virtual ~LinuxWatcher() {
    STLDeleteElements(&readers_);
  }

  // Start listening for connections on the socket.  This method should be
  // called from the IO thread.
  void StartListening(int socket);

  // This method determines if we should use the same process and if we should,
  // opens a new browser tab.  This runs on the UI thread.
  void HandleMessage(std::string current_dir, std::vector<std::string> argv);

  // MessageLoopForIO::Watcher impl.  These run on the IO thread.
  virtual void OnFileCanReadWithoutBlocking(int fd);
  virtual void OnFileCanWriteWithoutBlocking(int fd) {
    // ProcessSingleton only watches for accept (read) events.
    NOTREACHED();
  }

  // MessageLoop::DestructionObserver
  virtual void WillDestroyCurrentMessageLoop() {
    fd_watcher_.StopWatchingFileDescriptor();
  }

 private:
  class SocketReader : public MessageLoopForIO::Watcher {
   public:
    SocketReader(ProcessSingleton::LinuxWatcher* parent,
                 MessageLoop* ui_message_loop,
                 int fd)
        : parent_(parent),
          ui_message_loop_(ui_message_loop),
          fd_(fd),
          bytes_read_(0) {
      // Wait for reads.
      MessageLoopForIO::current()->WatchFileDescriptor(
          fd, true, MessageLoopForIO::WATCH_READ, &fd_reader_, this);
      timer_.Start(base::TimeDelta::FromSeconds(kTimeOutInSeconds),
                   this, &SocketReader::OnTimerExpiry);
    }

    virtual ~SocketReader() {
      int rv = HANDLE_EINTR(close(fd_));
      DCHECK_EQ(0, rv) << "Error closing socket: " << strerror(errno);
    }

    // MessageLoopForIO::Watcher impl.
    virtual void OnFileCanReadWithoutBlocking(int fd);
    virtual void OnFileCanWriteWithoutBlocking(int fd) {
      // SocketReader only watches for accept (read) events.
      NOTREACHED();
    }

   private:
    // If we haven't completed in a reasonable amount of time, give up.
    void OnTimerExpiry() {
      parent_->RemoveSocketReader(this);
      // We're deleted beyond this point.
    }

    MessageLoopForIO::FileDescriptorWatcher fd_reader_;

    // The ProcessSingleton::LinuxWatcher that owns us.
    ProcessSingleton::LinuxWatcher* const parent_;

    // A reference to the UI message loop.
    MessageLoop* const ui_message_loop_;

    // The file descriptor we're reading.
    const int fd_;

    // Store the message in this buffer.
    char buf_[kMaxMessageLength];

    // Tracks the number of bytes we've read in case we're getting partial
    // reads.
    size_t bytes_read_;

    base::OneShotTimer<SocketReader> timer_;

    DISALLOW_COPY_AND_ASSIGN(SocketReader);
  };

  // Removes and deletes the SocketReader.
  void RemoveSocketReader(SocketReader* reader);

  MessageLoopForIO::FileDescriptorWatcher fd_watcher_;

  // A reference to the UI message loop (i.e., the message loop we were
  // constructed on).
  MessageLoop* ui_message_loop_;

  // The ProcessSingleton that owns us.
  ProcessSingleton* const parent_;

  std::set<SocketReader*> readers_;

  DISALLOW_COPY_AND_ASSIGN(LinuxWatcher);
};

void ProcessSingleton::LinuxWatcher::OnFileCanReadWithoutBlocking(int fd) {
  // Accepting incoming client.
  sockaddr_un from;
  socklen_t from_len = sizeof(from);
  int connection_socket = HANDLE_EINTR(accept(
      fd, reinterpret_cast<sockaddr*>(&from), &from_len));
  if (-1 == connection_socket) {
    LOG(ERROR) << "accept() failed: " << strerror(errno);
    return;
  }
  SocketReader* reader = new SocketReader(this,
                                          ui_message_loop_,
                                          connection_socket);
  readers_.insert(reader);
}

void ProcessSingleton::LinuxWatcher::StartListening(int socket) {
  DCHECK(ChromeThread::GetMessageLoop(ChromeThread::IO) ==
         MessageLoop::current());
  // Watch for client connections on this socket.
  MessageLoopForIO* ml = MessageLoopForIO::current();
  ml->AddDestructionObserver(this);
  ml->WatchFileDescriptor(socket, true, MessageLoopForIO::WATCH_READ,
                          &fd_watcher_, this);
}

void ProcessSingleton::LinuxWatcher::HandleMessage(std::string current_dir,
    std::vector<std::string> argv) {
  DCHECK(ui_message_loop_ == MessageLoop::current());
  // Ignore the request if the browser process is already in shutdown path.
  if (!g_browser_process || g_browser_process->IsShuttingDown()) {
    LOG(WARNING) << "Not handling interprocess notification as browser"
                    " is shutting down";
    return;
  }

  // If locked, it means we are not ready to process this message because
  // we are probably in a first run critical phase.
  if (parent_->locked()) {
    DLOG(WARNING) << "Browser is locked";
    return;
  }

  CommandLine parsed_command_line(argv);
  PrefService* prefs = g_browser_process->local_state();
  DCHECK(prefs);

  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);
  if (!profile) {
    // We should only be able to get here if the profile already exists and
    // has been created.
    NOTREACHED();
    return;
  }

  // TODO(tc): Send an ACK response on success.

  // Run the browser startup sequence again, with the command line of the
  // signalling process.
  FilePath current_dir_file_path(current_dir);
  BrowserInit::ProcessCommandLine(parsed_command_line,
                                  current_dir_file_path.ToWStringHack(),
                                  false, profile, NULL);
}

void ProcessSingleton::LinuxWatcher::RemoveSocketReader(SocketReader* reader) {
  DCHECK(reader);
  readers_.erase(reader);
  delete reader;
}

///////////////////////////////////////////////////////////////////////////////
// ProcessSingleton::LinuxWatcher::SocketReader
//

void ProcessSingleton::LinuxWatcher::SocketReader::OnFileCanReadWithoutBlocking(
    int fd) {
  DCHECK_EQ(fd, fd_);
  while (bytes_read_ < sizeof(buf_)) {
    ssize_t rv = HANDLE_EINTR(
        read(fd, buf_ + bytes_read_, sizeof(buf_) - bytes_read_));
    if (rv < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        LOG(ERROR) << "read() failed: " << strerror(errno);
        rv = HANDLE_EINTR(close(fd));
        DCHECK_EQ(0, rv) << "Error closing socket: " << strerror(errno);
        return;
      } else {
        // It would block, so we just return and continue to watch for the next
        // opportunity to read.
        return;
      }
    } else if (!rv) {
      // No more data to read.  It's time to process the message.
      break;
    } else {
      bytes_read_ += rv;
    }
  }

  // Validate the message.  The shortest message is kStartToken\0x\0x
  const size_t kMinMessageLength = arraysize(kStartToken) + 4;
  if (bytes_read_ < kMinMessageLength) {
    buf_[bytes_read_] = 0;
    LOG(ERROR) << "Invalid socket message (wrong length):" << buf_;
    return;
  }

  std::string str(buf_, bytes_read_);
  std::vector<std::string> tokens;
  SplitString(str, kTokenDelimiter, &tokens);

  if (tokens.size() < 3 || tokens[0] != kStartToken) {
    LOG(ERROR) << "Wrong message format: " << str;
    return;
  }

  std::string current_dir = tokens[1];
  // Remove the first two tokens.  The remaining tokens should be the command
  // line argv array.
  tokens.erase(tokens.begin());
  tokens.erase(tokens.begin());

  // Return to the UI thread to handle opening a new browser tab.
  ui_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      parent_,
      &ProcessSingleton::LinuxWatcher::HandleMessage,
      current_dir,
      tokens));
  fd_reader_.StopWatchingFileDescriptor();

  parent_->RemoveSocketReader(this);
  // We are deleted beyond this point.
}

///////////////////////////////////////////////////////////////////////////////
// ProcessSingleton
//
ProcessSingleton::ProcessSingleton(const FilePath& user_data_dir)
    : locked_(false),
      foreground_window_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(watcher_(new LinuxWatcher(this))) {
  socket_path_ = user_data_dir.Append(chrome::kSingletonSocketFilename);
}

ProcessSingleton::~ProcessSingleton() {
}

bool ProcessSingleton::NotifyOtherProcess() {
  int socket;
  sockaddr_un addr;
  SetupSocket(&socket, &addr);

  // Connecting to the socket
  int ret = HANDLE_EINTR(connect(socket,
                                 reinterpret_cast<sockaddr*>(&addr),
                                 sizeof(addr)));
  if (ret < 0)
    return false;  // Tell the caller there's nobody to notify.

  timeval timeout = {20, 0};
  setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

  // Found another process, prepare our command line
  // format is "START\0<current dir>\0<argv[0]>\0...\0<argv[n]>".
  std::string to_send(kStartToken);
  to_send.push_back(kTokenDelimiter);

  FilePath current_dir;
  if (!PathService::Get(base::DIR_CURRENT, &current_dir))
    return false;
  to_send.append(current_dir.value());

  const std::vector<std::string>& argv =
      CommandLine::ForCurrentProcess()->argv();
  for (std::vector<std::string>::const_iterator it = argv.begin();
      it != argv.end(); ++it) {
    to_send.push_back(kTokenDelimiter);
    to_send.append(*it);
  }

  // Send the message
  int bytes_written = 0;
  const int buf_len = to_send.length();
  do {
    int rv = HANDLE_EINTR(
        write(socket, to_send.data() + bytes_written, buf_len - bytes_written));
    if (rv < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // The socket shouldn't block, we're sending so little data.  Just give
        // up here, since NotifyOtherProcess() doesn't have an asynchronous api.
        LOG(ERROR) << "ProcessSingleton would block on write(), so it gave up.";
        return false;
      }
      LOG(ERROR) << "write() failed: " << strerror(errno);
      return false;
    }
    bytes_written += rv;
  } while (bytes_written < buf_len);

  int rv = shutdown(socket, SHUT_WR);
  if (rv < 0) {
    LOG(ERROR) << "shutdown() failed: " << strerror(errno);
  } else {
    // TODO(tc): We should wait for an ACK, and if we don't get it in a certain
    // time period, kill the other process.
  }

  rv = HANDLE_EINTR(close(socket));
  DCHECK_EQ(0, rv) << strerror(errno);

  // Assume the other process is handling the request.
  return true;
}

void ProcessSingleton::Create() {
  int sock;
  sockaddr_un addr;
  SetupSocket(&sock, &addr);

  if (unlink(socket_path_.value().c_str()) < 0)
    DCHECK_EQ(errno, ENOENT);

  if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    LOG(ERROR) << "bind() failed: " << strerror(errno);
    LOG(ERROR) << "SingletonSocket failed to create a socket in your home "
                  "directory. This means that running multiple instances of "
                  "the Chrome binary will start multiple browser process "
                  "rather than opening a new window in the existing process.";
    close(sock);
    return;
  }

  if (listen(sock, 5) < 0)
    NOTREACHED() << "listen failed: " << strerror(errno);

  // Normally we would use ChromeThread, but the IO thread hasn't started yet.
  // Using g_browser_process, we start the thread so we can listen on the
  // socket.
  MessageLoop* ml = g_browser_process->io_thread()->message_loop();
  DCHECK(ml);
  ml->PostTask(FROM_HERE, NewRunnableMethod(
    watcher_.get(),
    &ProcessSingleton::LinuxWatcher::StartListening,
    sock));
}

void ProcessSingleton::SetupSocket(int* sock, struct sockaddr_un* addr) {
  *sock = socket(PF_UNIX, SOCK_STREAM, 0);
  if (*sock < 0)
    LOG(FATAL) << "socket() failed: " << strerror(errno);

  int rv = SetNonBlocking(*sock);
  DCHECK_EQ(0, rv) << "Failed to make non-blocking socket.";

  addr->sun_family = AF_UNIX;
  if (socket_path_.value().length() > sizeof(addr->sun_path) - 1)
    LOG(FATAL) << "Socket path too long: " << socket_path_.value();
  base::strlcpy(addr->sun_path, socket_path_.value().c_str(),
                sizeof(addr->sun_path));
}
