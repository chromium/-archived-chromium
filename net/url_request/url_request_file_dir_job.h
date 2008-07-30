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

#ifndef NET_URL_REQUEST_URL_REQUEST_FILE_DIR_JOB_H__
#define NET_URL_REQUEST_URL_REQUEST_FILE_DIR_JOB_H__

#include "net/base/directory_lister.h"
#include "net/url_request/url_request_job.h"

class URLRequestFileDirJob : public URLRequestJob,
                             public net::DirectoryLister::Delegate {
 public:
  URLRequestFileDirJob(URLRequest* request, const std::wstring& dir_path);
  virtual ~URLRequestFileDirJob();

  // URLRequestJob methods:
  virtual void Start();
  virtual void StartAsync();
  virtual void Kill();
  virtual bool ReadRawData(char* buf, int buf_size, int *bytes_read);
  virtual bool GetMimeType(std::string* mime_type);
  virtual bool GetCharset(std::string* charset);

  // DirectoryLister::Delegate methods:
  virtual void OnListFile(const WIN32_FIND_DATA& data);
  virtual void OnListDone(int error);

 private:
  void CloseLister();
  // When we have data and a read has been pending, this function
  // will fill the response buffer and notify the request
  // appropriately.
  void CompleteRead();

  // Fills a buffer with the output.
  bool FillReadBuffer(char *buf, int buf_size, int *bytes_read);

  scoped_refptr<net::DirectoryLister> lister_;
  std::wstring dir_path_;
  std::string data_;
  bool canceled_;

  // Indicates whether we have the complete list of the dir
  bool list_complete_;

  // Indicates whether we have written the HTML header
  bool wrote_header_;

  // To simulate Async IO, we hold onto the Reader's buffer while
  // we wait for IO to complete.  When done, we fill the buffer
  // manually.
  bool read_pending_;
  char *read_buffer_;
  int read_buffer_length_;

  DISALLOW_EVIL_CONSTRUCTORS(URLRequestFileDirJob);
};

#endif  // NET_URL_REQUEST_URL_REQUEST_FILE_DIR_JOB_H__
