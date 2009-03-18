// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/directory_watcher.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/object_watcher.h"

// Private implementation class implementing the behavior of DirectoryWatcher.
class DirectoryWatcher::Impl : public base::RefCounted<DirectoryWatcher::Impl>,
                               public base::ObjectWatcher::Delegate {
 public:
  Impl(DirectoryWatcher::Delegate* delegate)
      : delegate_(delegate), handle_(INVALID_HANDLE_VALUE) {}
  ~Impl();

  // Register interest in any changes in |path|.
  // Returns false on error.
  bool Watch(const FilePath& path);

  // Callback from MessageLoopForIO.
  virtual void OnObjectSignaled(HANDLE object);

 private:
  // Delegate to notify upon changes.
  DirectoryWatcher::Delegate* delegate_;
  // Path we're watching (passed to delegate).
  FilePath path_;
  // Handle for FindFirstChangeNotification.
  HANDLE handle_;
  // ObjectWatcher to watch handle_ for events.
  base::ObjectWatcher watcher_;
};

DirectoryWatcher::Impl::~Impl() {
  if (handle_ != INVALID_HANDLE_VALUE) {
    watcher_.StopWatching();
    FindCloseChangeNotification(handle_);
  }
}

bool DirectoryWatcher::Impl::Watch(const FilePath& path) {
  DCHECK(path_.value().empty());  // Can only watch one path.

  // NOTE: If you want to change this code to *not* watch subdirectories, have a
  // look at http://code.google.com/p/chromium/issues/detail?id=5072 first.
  handle_ = FindFirstChangeNotification(
      path.value().c_str(),
      TRUE,  // Watch subtree.
      FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE |
      FILE_NOTIFY_CHANGE_LAST_WRITE);
  if (handle_ == INVALID_HANDLE_VALUE)
    return false;

  path_ = path;
  watcher_.StartWatching(handle_, this);

  return true;
}

void DirectoryWatcher::Impl::OnObjectSignaled(HANDLE object) {
  DCHECK(object == handle_);
  // Make sure we stay alive through the body of this function.
  scoped_refptr<DirectoryWatcher::Impl> keep_alive(this);

  delegate_->OnDirectoryChanged(path_);

  // Register for more notifications on file change.
  BOOL ok = FindNextChangeNotification(object);
  DCHECK(ok);
  watcher_.StartWatching(object, this);
}

DirectoryWatcher::DirectoryWatcher() {
}

DirectoryWatcher::~DirectoryWatcher() {
  // Declared in .cc file for access to ~DirectoryWatcher::Impl.
}

bool DirectoryWatcher::Watch(const FilePath& path,
                             Delegate* delegate, bool recursive) {
  if (!recursive) {
    // See http://crbug.com/5072.
    NOTIMPLEMENTED();
    return false;
  }
  impl_ = new DirectoryWatcher::Impl(delegate);
  return impl_->Watch(path);
}
