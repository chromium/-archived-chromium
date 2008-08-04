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

#ifndef NET_URL_REQUEST_URL_REQUEST_FTP_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_FTP_JOB_H_

#include "net/url_request/url_request_inet_job.h"

// A basic FTP job that handles download files and showing directory listings.
class URLRequestFtpJob : public URLRequestInetJob {
 public:
  static URLRequestJob* Factory(URLRequest* request, const std::string& scheme);

  virtual ~URLRequestFtpJob();

  // URLRequestJob methods:
  virtual void Start();
  virtual bool GetMimeType(std::string* mime_type);

  // URLRequestInetJob methods:
  virtual void OnIOComplete(const AsyncResult& result);

 protected:
  URLRequestFtpJob(URLRequest* request);

  // Starts the WinInet request.
  virtual void SendRequest();

  virtual int CallInternetRead(char* dest, int dest_size, int *bytes_read);
  virtual bool GetReadBytes(const AsyncResult& result, int* bytes_read);
  virtual void OnCancelAuth();
  virtual void OnSetAuth();
  virtual bool NeedsAuth();
  virtual void GetAuthChallengeInfo(scoped_refptr<net::AuthChallengeInfo>*);
  virtual void GetCachedAuthData(const net::AuthChallengeInfo& auth_info,
                                 scoped_refptr<net::AuthData>* auth_data);
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

  // Continuation function for calling NotifyHeadersComplete through
  //the message loop
  virtual void ContinueNotifyHeadersComplete();

  typedef enum {
    START = 0x200,          // initial state of the ftp job
    CONNECTING,             // opening the url
    SETTING_CUR_DIRECTORY, // attempting to change current dir to match request
    FINDING_FIRST_FILE,     // retrieving first file information in cur dir (by FtpFindFirstFile)
    GETTING_DIRECTORY,      // retrieving the directory listing (if directory)
    GETTING_FILE_HANDLE,    // initiate access to file by call to FtpOpenFile (if file)
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
  std::string directory_html_; // if url is directory holds html

  // When building a directory listing, we need to temporarily hold on to the
  // buffer in between the time a Read() call comes in and we get the file
  // entry from WinInet.
  char* dest_;
  int dest_size_;


  DISALLOW_EVIL_CONSTRUCTORS(URLRequestFtpJob);
};

#endif  // NET_URL_REQUEST_URL_REQUEST_FTP_JOB_H_
