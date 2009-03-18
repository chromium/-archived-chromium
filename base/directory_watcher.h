// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module provides a way to monitor a directory for changes.

#ifndef BASE_DIRECTORY_WATCHER_H_
#define BASE_DIRECTORY_WATCHER_H_

#include <string>
#include "base/basictypes.h"
#include "base/ref_counted.h"

class FilePath;

// This class lets you register interest in changes on a directory.
// The delegate will get called whenever a file is added or changed in the
// directory.
class DirectoryWatcher {
 public:
  class Delegate {
   public:
    virtual void OnDirectoryChanged(const FilePath& path) = 0;
  };

  DirectoryWatcher();
  ~DirectoryWatcher();

  // Register interest in any changes in the directory |path|.
  // OnDirectoryChanged will be called back for each change within the dir.
  // If |recursive| is true, the delegate will be notified for each change
  // within the directory tree starting at |path|. Returns false on error.
  bool Watch(const FilePath& path, Delegate* delegate, bool recursive);

 private:
  class Impl;
  friend class Impl;
  scoped_refptr<Impl> impl_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryWatcher);
};

#endif  // BASE_DIRECTORY_WATCHER_H_
