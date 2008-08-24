// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_DIRECTORY_LISTER_H__
#define NET_BASE_DIRECTORY_LISTER_H__

#include <windows.h>
#include <string>

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
class DirectoryLister : public base::RefCountedThreadSafe<DirectoryLister> {
 public:
  // Implement this class to receive directory entries.
  class Delegate {
   public:
    virtual void OnListFile(const WIN32_FIND_DATA& data) = 0;
    virtual void OnListDone(int error) = 0;
  };

  DirectoryLister(const std::wstring& dir, Delegate* delegate);
  ~DirectoryLister();

  // Call this method to start the directory enumeration thread.
  bool Start();

  // Call this method to asynchronously stop directory enumeration.  The
  // delegate will receive the OnListDone notification with an error code of
  // ERROR_OPERATION_ABORTED.
  void Cancel();

  // The delegate pointer may be modified at any time.
  Delegate* delegate() const { return delegate_; }
  void set_delegate(Delegate* d) { delegate_ = d; }

  // Returns the directory being enumerated.
  const std::wstring& directory() const { return dir_; }

  // Returns true if the directory enumeration was canceled.
  bool was_canceled() const { return canceled_; }

 private:
  friend class DirectoryDataEvent;

  void OnReceivedData(const WIN32_FIND_DATA* data, int count);
  void OnDone(int error);

  static unsigned __stdcall ThreadFunc(void* param);

  std::wstring dir_;
  Delegate* delegate_;
  MessageLoop* message_loop_;
  HANDLE thread_;
  bool canceled_;
};

}  // namespace net

#endif  // NET_BASE_DIRECTORY_LISTER_H__

