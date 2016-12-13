// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_FILE_DIR_JOB_H__
#define NET_URL_REQUEST_URL_REQUEST_FILE_DIR_JOB_H__

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "net/base/directory_lister.h"
#include "net/url_request/url_request_job.h"

class URLRequestFileDirJob
  : public URLRequestJob,
    public net::DirectoryLister::DirectoryListerDelegate {
 public:
  URLRequestFileDirJob(URLRequest* request, const FilePath& dir_path);
  virtual ~URLRequestFileDirJob();

  // URLRequestJob methods:
  virtual void Start();
  virtual void StartAsync();
  virtual void Kill();
  virtual bool ReadRawData(net::IOBuffer* buf, int buf_size, int *bytes_read);
  virtual bool GetMimeType(std::string* mime_type) const;
  virtual bool GetCharset(std::string* charset);
  virtual bool IsRedirectResponse(GURL* location, int* http_status_code);

  // DirectoryLister::DirectoryListerDelegate methods:
  virtual void OnListFile(const file_util::FileEnumerator::FindInfo& data);
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
  FilePath dir_path_;
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
  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_length_;

  DISALLOW_EVIL_CONSTRUCTORS(URLRequestFileDirJob);
};

#endif  // NET_URL_REQUEST_URL_REQUEST_FILE_DIR_JOB_H__
