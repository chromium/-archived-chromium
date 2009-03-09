// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_FTP_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_FTP_JOB_H_

#include <string>

#include "net/url_request/url_request_inet_job.h"

// A basic FTP job that handles download files and showing directory listings.
class URLRequestFtpJob : public URLRequestInetJob {
 public:
  static URLRequestJob* Factory(URLRequest* request, const std::string& scheme);

  virtual ~URLRequestFtpJob();

  // URLRequestJob methods:
  virtual void Start();
  virtual bool GetMimeType(std::string* mime_type) const;

  // URLRequestInetJob methods:
  virtual void OnIOComplete(const AsyncResult& result);

 protected:
  explicit URLRequestFtpJob(URLRequest* request);

  // Starts the WinInet request.
  virtual void SendRequest();

  virtual int CallInternetRead(char* dest, int dest_size, int *bytes_read);
  virtual bool GetReadBytes(const AsyncResult& result, int* bytes_read);
  virtual void OnCancelAuth();
  virtual void OnSetAuth();
  virtual bool NeedsAuth();
  virtual void GetAuthChallengeInfo(scoped_refptr<net::AuthChallengeInfo>*);
  virtual bool IsRedirectResponse(GURL* location, int* http_status_code);

 private:
  // Called after InternetConnect successfully connects to server.
  void OnConnect();

  // Called after FtpSetCurrentDirectory attempts to change current dir.
  void OnSetCurrentDirectory(DWORD last_error);

  // Requests the next file in the directory listing from WinInet.
  void FindNextFile();

  // Called when the first file in a directory listing is available.
  void OnFindFirstFile(DWORD last_error);

  // Called when a file in a directory listing is available.
  void OnFindFile(DWORD last_error);

  // Call this when starting a directory listing to setup the html.
  void OnStartDirectoryTraversal();

  // Call this at the end of a directory listing to complete the html.
  void OnFinishDirectoryTraversal();

  // If given data, writes it to the directory listing html.  If
  // call_io_complete is true, will also notify the parent class that we wrote
  // data in the given buffer.
  int WriteData(const std::string* data, bool call_io_complete);

  // Continuation function for calling OnIOComplete through the message loop.
  virtual void ContinueIOComplete(int bytes_written);

  // Continuation function for calling NotifyHeadersComplete through the message
  // loop.
  virtual void ContinueNotifyHeadersComplete();

  typedef enum {
    START = 0x200,          // initial state of the ftp job
    CONNECTING,             // opening the url
    SETTING_CUR_DIRECTORY,  // attempting to change current dir to match request
    FINDING_FIRST_FILE,     // retrieving first file information in cur dir (by
                            // FtpFindFirstFile)
    GETTING_DIRECTORY,      // retrieving the directory listing (if directory)
    GETTING_FILE_HANDLE,    // initiate access to file by call to FtpOpenFile
                            // (if file).
    GETTING_FILE,           // retrieving the file (if file)
    DONE                    // URLRequestInetJob is reading the response now
  } FtpJobState;

  // The FtpJob has several asynchronous operations which happen
  // in sequence.  The state keeps track of which asynchronous IO
  // is pending at any given point in time.
  FtpJobState state_;

  // In IE 4 and before, this pointer passed to asynchronous InternetReadFile
  // calls is where the number of read bytes is written to.
  DWORD bytes_read_;

  bool is_directory_;  // does the url point to a file or directory
  WIN32_FIND_DATAA find_data_;
  std::string directory_html_;  // if url is directory holds html

  // When building a directory listing, we need to temporarily hold on to the
  // buffer in between the time a Read() call comes in and we get the file
  // entry from WinInet.
  char* dest_;
  int dest_size_;


  DISALLOW_EVIL_CONSTRUCTORS(URLRequestFtpJob);
};

#endif  // NET_URL_REQUEST_URL_REQUEST_FTP_JOB_H_

