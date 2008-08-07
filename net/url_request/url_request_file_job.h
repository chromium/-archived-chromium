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

#ifndef NET_URL_REQUEST_URL_REQUEST_FILE_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_FILE_JOB_H_

#include "base/lock.h"
#include "base/message_loop.h"
#include "base/object_watcher.h"
#include "base/thread.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

// A request job that handles reading file URLs
class URLRequestFileJob : public URLRequestJob,
                          public base::ObjectWatcher::Delegate {
 public:
  URLRequestFileJob(URLRequest* request);
  virtual ~URLRequestFileJob();

  virtual void Start();
  virtual void Kill();
  virtual bool ReadRawData(char* buf, int buf_size, int *bytes_read);
  virtual bool IsRedirectResponse(GURL* location, int* http_status_code);

  virtual bool GetMimeType(std::string* mime_type);

  // Checks the status of the file. Set is_directory_ and is_not_found_
  // accordingly. Call StartAsync on the main message loop when it's done.
  void ResolveFile();

  static URLRequest::ProtocolFactory Factory;

 protected:
  // The OS-specific full path name of the file
  std::wstring file_path_;

 private:
  void CloseHandles();
  void StartAsync();

  // base::ObjectWatcher::Delegate implementation:
  virtual void OnObjectSignaled(HANDLE object);

  // We use overlapped reads to ensure that reads from network file systems do
  // not hang the application thread.
  HANDLE handle_;
  OVERLAPPED overlapped_;
  bool is_waiting_;  // true when waiting for overlapped result
  bool is_directory_;  // true when the file request is for a direcotry.
  bool is_not_found_;  // true when the file requested does not exist.

  base::ObjectWatcher watcher_;

  // This lock ensure that the network_file_thread is not using the loop_ after
  // is has been set to NULL in Kill().
  Lock loop_lock_;

  // Main message loop where StartAsync has to be called.
  MessageLoop* loop_;

  // Thread used to query the attributes of files on the network.
  // We need to do it on a separate thread because it can be really
  // slow.
  HANDLE network_file_thread_;

  DISALLOW_EVIL_CONSTRUCTORS(URLRequestFileJob);
};

#endif  // NET_URL_REQUEST_URL_REQUEST_FILE_JOB_H_
