// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_DIRECTORY_LISTER_H__
#define NET_BASE_DIRECTORY_LISTER_H__

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/platform_thread.h"
#include "base/ref_counted.h"

class MessageLoop;

namespace net {

//
// This class provides an API for listing the contents of a directory on the
// filesystem asynchronously.  It spawns a background thread, and enumerates
// the specified directory on that thread.  It marshalls WIN32_FIND_DATA
// structs over to the main application thread.  The consumer of this class
// is insulated from any of the multi-threading details.
//
class DirectoryLister : public base::RefCountedThreadSafe<DirectoryLister>,
                        public PlatformThread::Delegate {
 public:
  // Implement this class to receive directory entries.
  class DirectoryListerDelegate {
   public:
    virtual void OnListFile(
        const file_util::FileEnumerator::FindInfo& data) = 0;
    virtual void OnListDone(int error) = 0;
  };

  DirectoryLister(const FilePath& dir, DirectoryListerDelegate* delegate);
  ~DirectoryLister();

  // Call this method to start the directory enumeration thread.
  bool Start();

  // Call this method to asynchronously stop directory enumeration.  The
  // delegate will receive the OnListDone notification with an error code of
  // net::ERR_ABORTED.
  void Cancel();

  // The delegate pointer may be modified at any time.
  DirectoryListerDelegate* delegate() const { return delegate_; }
  void set_delegate(DirectoryListerDelegate* d) { delegate_ = d; }

  // PlatformThread::Delegate implementation
  void ThreadMain();

 private:
  friend class DirectoryDataEvent;

  void OnReceivedData(const file_util::FileEnumerator::FindInfo* data,
                      int count);
  void OnDone(int error);

  FilePath dir_;
  DirectoryListerDelegate* delegate_;
  MessageLoop* message_loop_;
  PlatformThreadHandle thread_;
  bool canceled_;
};

}  // namespace net

#endif  // NET_BASE_DIRECTORY_LISTER_H__
