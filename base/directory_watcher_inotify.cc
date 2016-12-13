// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/directory_watcher.h"

#include <errno.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/hash_tables.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/task.h"
#include "base/thread.h"
#include "base/waitable_event.h"

namespace {

class DirectoryWatcherImpl;

// Singleton to manage all inotify watches.
class InotifyReader {
 public:
  typedef int Watch;  // Watch descriptor used by AddWatch and RemoveWatch.
  static const Watch kInvalidWatch = -1;

  // Watch |path| for changes. |watcher| will be notified on each change.
  // Returns kInvalidWatch on failure.
  Watch AddWatch(const FilePath& path, DirectoryWatcherImpl* watcher);

  // Remove |watch|. Returns true on success.
  bool RemoveWatch(Watch watch, DirectoryWatcherImpl* watcher);

  // Callback for InotifyReaderTask.
  void OnInotifyEvent(const inotify_event* event);

 private:
  friend struct DefaultSingletonTraits<InotifyReader>;

  typedef std::set<DirectoryWatcherImpl*> WatcherSet;

  InotifyReader();
  ~InotifyReader();

  // We keep track of which delegates want to be notified on which watches.
  base::hash_map<Watch, WatcherSet> watchers_;

  // For each watch we also want to know the path it's watching.
  base::hash_map<Watch, FilePath> paths_;

  // Lock to protect delegates_ and paths_.
  Lock lock_;

  // Separate thread on which we run blocking read for inotify events.
  base::Thread thread_;

  // File descriptor returned by inotify_init.
  const int inotify_fd_;

  // Use self-pipe trick to unblock select during shutdown.
  int shutdown_pipe_[2];

  // Flag set to true when startup was successful.
  bool valid_;

  DISALLOW_COPY_AND_ASSIGN(InotifyReader);
};

class DirectoryWatcherImpl : public DirectoryWatcher::PlatformDelegate {
 public:
  typedef std::set<FilePath> FilePathSet;

  DirectoryWatcherImpl();
  ~DirectoryWatcherImpl();

  void EnsureSetupFinished();

  // Called for each event coming from one of watches.
  void OnInotifyEvent(const inotify_event* event);

  // Callback for RegisterSubtreeWatchesTask.
  bool OnEnumeratedSubtree(const FilePathSet& paths);

  // Start watching |path| for changes and notify |delegate| on each change.
  // If |recursive| is true, watch entire subtree.
  // Returns true if watch for |path| has been added successfully. Watches
  // required for |recursive| are added on a background thread and have no
  // effect on the return value.
  virtual bool Watch(const FilePath& path, DirectoryWatcher::Delegate* delegate,
                     MessageLoop* backend_loop, bool recursive);

 private:
  typedef std::set<InotifyReader::Watch> WatchSet;
  typedef std::set<ino_t> InodeSet;

  // Returns true if |inode| is watched by DirectoryWatcherImpl.
  bool IsInodeWatched(ino_t inode) const;

  // Delegate to notify upon changes.
  DirectoryWatcher::Delegate* delegate_;

  // Path we're watching (passed to delegate).
  FilePath root_path_;

  // Watch returned by InotifyReader.
  InotifyReader::Watch watch_;

  // Set of watched inodes.
  InodeSet inodes_watched_;

  // Keep track of registered watches.
  WatchSet watches_;

  // Lock to protect inodes_watched_ and watches_.
  Lock lock_;

  // Flag set to true when recursively watching subtree.
  bool recursive_;

  // Loop where we post directory change notifications to.
  MessageLoop* loop_;

  // Event signaled when the background task finished adding initial inotify
  // watches for recursive watch.
  base::WaitableEvent recursive_setup_finished_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryWatcherImpl);
};

class RegisterSubtreeWatchesTask : public Task {
 public:
  RegisterSubtreeWatchesTask(DirectoryWatcherImpl* watcher,
                             const FilePath& path)
      : watcher_(watcher),
        path_(path) {
  }

  virtual void Run() {
    file_util::FileEnumerator dir_list(path_, true,
        file_util::FileEnumerator::DIRECTORIES);

    DirectoryWatcherImpl::FilePathSet subtree;
    for (FilePath subdirectory = dir_list.Next();
         !subdirectory.empty();
         subdirectory = dir_list.Next()) {
      subtree.insert(subdirectory);
    }
    watcher_->OnEnumeratedSubtree(subtree);
  }

 private:
  DirectoryWatcherImpl* watcher_;
  FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(RegisterSubtreeWatchesTask);
};

class DirectoryWatcherImplNotifyTask : public Task {
 public:
  DirectoryWatcherImplNotifyTask(DirectoryWatcher::Delegate* delegate,
                                 const FilePath& path)
      : delegate_(delegate),
        path_(path) {
  }

  virtual void Run() {
    delegate_->OnDirectoryChanged(path_);
  }

 private:
  DirectoryWatcher::Delegate* delegate_;
  FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryWatcherImplNotifyTask);
};

class InotifyReaderTask : public Task {
 public:
  InotifyReaderTask(InotifyReader* reader, int inotify_fd, int shutdown_fd)
      : reader_(reader),
        inotify_fd_(inotify_fd),
        shutdown_fd_(shutdown_fd) {
  }

  virtual void Run() {
    while (true) {
      fd_set rfds;
      FD_ZERO(&rfds);
      FD_SET(inotify_fd_, &rfds);
      FD_SET(shutdown_fd_, &rfds);

      // Wait until some inotify events are available.
      int select_result =
        HANDLE_EINTR(select(std::max(inotify_fd_, shutdown_fd_) + 1,
                            &rfds, NULL, NULL, NULL));
      if (select_result < 0) {
        DLOG(WARNING) << "select failed: " << strerror(errno);
        return;
      }

      if (FD_ISSET(shutdown_fd_, &rfds))
        return;

      // Adjust buffer size to current event queue size.
      int buffer_size;
      int ioctl_result = HANDLE_EINTR(ioctl(inotify_fd_, FIONREAD,
                                            &buffer_size));

      if (ioctl_result != 0) {
        DLOG(WARNING) << "ioctl failed: " << strerror(errno);
        return;
      }

      std::vector<char> buffer(buffer_size);

      ssize_t bytes_read = HANDLE_EINTR(read(inotify_fd_, &buffer[0],
                                             buffer_size));

      if (bytes_read < 0) {
        DLOG(WARNING) << "read from inotify fd failed: " << strerror(errno);
        return;
      }

      ssize_t i = 0;
      while (i < bytes_read) {
        inotify_event* event = reinterpret_cast<inotify_event*>(&buffer[i]);
        size_t event_size = sizeof(inotify_event) + event->len;
        DCHECK(i + event_size <= static_cast<size_t>(bytes_read));
        reader_->OnInotifyEvent(event);
        i += event_size;
      }
    }
  }

 private:
  InotifyReader* reader_;
  int inotify_fd_;
  int shutdown_fd_;

  DISALLOW_COPY_AND_ASSIGN(InotifyReaderTask);
};

InotifyReader::InotifyReader()
    : thread_("inotify_reader"),
      inotify_fd_(inotify_init()),
      valid_(false) {
  shutdown_pipe_[0] = -1;
  shutdown_pipe_[1] = -1;
  if (inotify_fd_ >= 0 && pipe(shutdown_pipe_) == 0 && thread_.Start()) {
    thread_.message_loop()->PostTask(
        FROM_HERE, new InotifyReaderTask(this, inotify_fd_, shutdown_pipe_[0]));
    valid_ = true;
  }
}

InotifyReader::~InotifyReader() {
  if (valid_) {
    // Write to the self-pipe so that the select call in InotifyReaderTask
    // returns.
    HANDLE_EINTR(write(shutdown_pipe_[1], "", 1));
    thread_.Stop();
  }
  if (inotify_fd_ >= 0)
    close(inotify_fd_);
  if (shutdown_pipe_[0] >= 0)
    close(shutdown_pipe_[0]);
  if (shutdown_pipe_[1] >= 0)
    close(shutdown_pipe_[1]);
}

InotifyReader::Watch InotifyReader::AddWatch(
    const FilePath& path, DirectoryWatcherImpl* watcher) {

  if (!valid_)
    return kInvalidWatch;

  AutoLock auto_lock(lock_);

  Watch watch = inotify_add_watch(inotify_fd_, path.value().c_str(),
                                  IN_CREATE | IN_DELETE |
                                  IN_CLOSE_WRITE | IN_MOVE);

  if (watch == kInvalidWatch)
    return kInvalidWatch;

  if (paths_[watch].empty())
    paths_[watch] = path;  // We don't yet watch this path.

  watchers_[watch].insert(watcher);

  return watch;
}

bool InotifyReader::RemoveWatch(Watch watch,
                                DirectoryWatcherImpl* watcher) {
  if (!valid_)
    return false;

  AutoLock auto_lock(lock_);

  if (paths_[watch].empty())
    return false;  // We don't recognize this watch.

  watchers_[watch].erase(watcher);

  if (watchers_[watch].empty()) {
    paths_.erase(watch);
    watchers_.erase(watch);
    return (inotify_rm_watch(inotify_fd_, watch) == 0);
  }

  return true;
}

void InotifyReader::OnInotifyEvent(const inotify_event* event) {
  if (event->mask & IN_IGNORED)
    return;

  WatcherSet watchers_to_notify;
  FilePath changed_path;

  {
    AutoLock auto_lock(lock_);
    changed_path = paths_[event->wd];
    watchers_to_notify.insert(watchers_[event->wd].begin(),
                              watchers_[event->wd].end());
  }

  for (WatcherSet::iterator watcher = watchers_to_notify.begin();
       watcher != watchers_to_notify.end();
       ++watcher) {
    (*watcher)->OnInotifyEvent(event);
  }
}

DirectoryWatcherImpl::DirectoryWatcherImpl()
    : watch_(InotifyReader::kInvalidWatch),
      recursive_setup_finished_(false, false) {
}

DirectoryWatcherImpl::~DirectoryWatcherImpl() {
  if (watch_ == InotifyReader::kInvalidWatch)
    return;

  if (recursive_)
    recursive_setup_finished_.Wait();
  for (WatchSet::iterator watch = watches_.begin();
       watch != watches_.end();
       ++watch) {
    Singleton<InotifyReader>::get()->RemoveWatch(*watch, this);
  }
  watches_.clear();
  inodes_watched_.clear();
}

void DirectoryWatcherImpl::OnInotifyEvent(const inotify_event* event) {
  loop_->PostTask(FROM_HERE,
      new DirectoryWatcherImplNotifyTask(delegate_, root_path_));

  if (!(event->mask & IN_ISDIR))
    return;

  if (event->mask & IN_CREATE || event->mask & IN_MOVED_TO) {
    // TODO(phajdan.jr): add watch for this new directory.
    NOTIMPLEMENTED();
  } else if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM) {
    // TODO(phajdan.jr): remove our watch for this directory.
    NOTIMPLEMENTED();
  }
}

bool DirectoryWatcherImpl::IsInodeWatched(ino_t inode) const {
  return inodes_watched_.find(inode) != inodes_watched_.end();
}

bool DirectoryWatcherImpl::OnEnumeratedSubtree(const FilePathSet& subtree) {
  DCHECK(recursive_);

  if (watch_ == InotifyReader::kInvalidWatch) {
    recursive_setup_finished_.Signal();
    return false;
  }

  bool success = true;

  {
    // Limit the scope of auto_lock so it releases lock_ before we signal
    // recursive_setup_finished_. Our dtor waits on recursive_setup_finished_
    // and could otherwise destroy the lock before we release it.
    AutoLock auto_lock(lock_);

    for (FilePathSet::iterator subdirectory = subtree.begin();
         subdirectory != subtree.end();
         ++subdirectory) {
      ino_t inode;
      if (!file_util::GetInode(*subdirectory, &inode)) {
        success = false;
        continue;
      }
      if (IsInodeWatched(inode))
        continue;
      InotifyReader::Watch watch =
        Singleton<InotifyReader>::get()->AddWatch(*subdirectory, this);
      if (watch != InotifyReader::kInvalidWatch) {
        watches_.insert(watch);
        inodes_watched_.insert(inode);
      }
    }
  }

  recursive_setup_finished_.Signal();
  return success;
}

bool DirectoryWatcherImpl::Watch(const FilePath& path,
                                 DirectoryWatcher::Delegate* delegate,
                                 MessageLoop* backend_loop, bool recursive) {

  // Can only watch one path.
  DCHECK(watch_ == InotifyReader::kInvalidWatch);

  ino_t inode;
  if (!file_util::GetInode(path, &inode))
    return false;

  InotifyReader::Watch watch =
      Singleton<InotifyReader>::get()->AddWatch(path, this);
  if (watch == InotifyReader::kInvalidWatch)
    return false;

  delegate_ = delegate;
  recursive_ = recursive;
  root_path_ = path;
  watch_ = watch;
  loop_ = MessageLoop::current();

  {
    AutoLock auto_lock(lock_);
    inodes_watched_.insert(inode);
    watches_.insert(watch_);
  }

  if (recursive_) {
    Task* subtree_task = new RegisterSubtreeWatchesTask(this, root_path_);
    if (backend_loop) {
      backend_loop->PostTask(FROM_HERE, subtree_task);
    } else {
      subtree_task->Run();
      delete subtree_task;
    }
  }

  return true;
}

}  // namespace

DirectoryWatcher::DirectoryWatcher() {
  impl_ = new DirectoryWatcherImpl();
}

