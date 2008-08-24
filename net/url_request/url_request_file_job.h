// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_FILE_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_FILE_JOB_H_

#include "base/lock.h"
#include "base/message_loop.h"
#include "base/thread.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

// A request job that handles reading file URLs
class URLRequestFileJob : public URLRequestJob,
                          protected MessageLoop::Watcher {
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
  // The net util test wants to run our FileURLToFilePath function.
  FRIEND_TEST(NetUtilTest, FileURLConversion);

  void CloseHandles();
  void StartAsync();

  // MessageLoop::Watcher callback
  virtual void OnObjectSignaled(HANDLE object);

  // We use overlapped reads to ensure that reads from network file systems do
  // not hang the application thread.
  HANDLE handle_;
  OVERLAPPED overlapped_;
  bool is_waiting_;  // true when waiting for overlapped result
  bool is_directory_;  // true when the file request is for a direcotry.
  bool is_not_found_;  // true when the file requested does not exist.

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

