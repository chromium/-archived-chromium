// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
