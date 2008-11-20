// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_INET_JOB_H__
#define NET_URL_REQUEST_URL_REQUEST_INET_JOB_H__

#include <windows.h>
#include <wininet.h>

#include "base/lock.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

namespace net {
class AuthData;
}

class MessageLoop;

// For all WinInet-based URL requests
class URLRequestInetJob : public URLRequestJob {
 public:
  URLRequestInetJob(URLRequest* request);
  virtual ~URLRequestInetJob();

  virtual void SetExtraRequestHeaders(const std::string& headers) {
    extra_request_headers_ = headers;
  }

  virtual void Kill();
  virtual bool ReadRawData(char* buf, int buf_size, int *bytes_read);

  // URLRequestJob Authentication methods
  virtual void SetAuth(const std::wstring& username,
                       const std::wstring& password);
  virtual void CancelAuth();

  // A structure holding the result and error code of an asynchronous IO.
  // This is a copy of INTERNET_ASYNC_RESULT.
  struct AsyncResult {
    DWORD_PTR dwResult;
    DWORD dwError;
  };

  // A virtual method to handle WinInet callbacks.  If this class
  // issues asynchronous IO, it will need to override this method
  // to receive completions of those asynchronous IOs.  The class
  // must track whether it has an async IO outstanding, and if it
  // does not it must call the base class' OnIOComplete.
  virtual void OnIOComplete(const AsyncResult& result) = 0;

  // Used internally to setup the OnIOComplete call. Public because this
  // is called from the Windows procedure, and we don't want to make it a
  // friend so we can avoid the Windows headers for this header file.
  void CallOnIOComplete(const AsyncResult& result);

  HINTERNET request_handle() const { return request_handle_; }

protected:
  // Called by this class and subclasses to send or resend this request.
  virtual void SendRequest() = 0;

  // Calls InternetReadFile(Ex) depending on the derived class.
  // Returns ERROR_SUCCESS on success, or else a standard Windows error code
  // on failure (from GetLastError()).
  virtual int CallInternetRead(char* dest, int dest_size, int *bytes_read) = 0;

  // After the base class calls CallInternetRead and the result is available,
  // it will call this method to get the number of received bytes.
  virtual bool GetReadBytes(const AsyncResult& result, int* bytes_read) = 0;

  // Called by this class and subclasses whenever a WinInet call fails.  This
  // method returns true if the error just means that we have to wait for
  // OnIOComplete to be called.
  bool ProcessRequestError(int error);

  // Called by URLRequestJob to get more data from the data stream of this job.
  virtual bool GetMoreData();

  // Cleans up the connection, if necessary, and closes the connection and
  // request handles. May be called multiple times, it will be a NOP if
  // there is nothing to do.
  void CleanupConnection();

  // Closes the given handle.
  void CleanupHandle(HINTERNET handle);

  // Returns the global handle to the internet (NOT the same as the connection
  // or request handle below)
  static HINTERNET GetTheInternet();

  // Makes appropriate asynch call to re-send a request based on
  // dynamic scheme type and user action at authentication prompt
  //(OK or Cancel)
  virtual void OnCancelAuth() = 0;
  virtual void OnSetAuth() = 0;

  // Handle of the connection for this request. This handle is created
  // by subclasses that create the connection according to their requirements.
  // It will be automatically destroyed by this class when the connection is
  // being closed. See also 'request_handle_'
  HINTERNET connection_handle_;

  // Handle of the specific request created by subclasses to meet their own
  // requirements. This handle has a more narrow scope than the connection
  // handle. If non-null, it will be automatically destroyed by this class
  // when the connection is being closed. It will be destroyed before the
  // connection handle.
  HINTERNET request_handle_;

  // The last error that occurred.  Used by ContinueDespiteLastError to adjust
  // the request's load_flags to ignore this error.
  DWORD last_error_;

  // Any extra request headers (\n-delimited) that should be included in the
  // request.
  std::string extra_request_headers_;

  // Authentication information.
  scoped_refptr<net::AuthData> proxy_auth_;
  scoped_refptr<net::AuthData> server_auth_;

 private:

  // One-time global state setup
  static void InitializeTheInternet(const std::string& user_agent);

  // Runs on some background thread (called by WinInet)
  static void CALLBACK URLRequestStatusCallback(HINTERNET handle,
                                                DWORD_PTR job_id,
                                                DWORD status,
                                                LPVOID status_info,
                                                DWORD status_info_len);

  static HINTERNET the_internet_;
#ifndef NDEBUG
  static MessageLoop* my_message_loop_;  // Used to sanity-check that all
                                         // requests are made on the same
                                         // thread
#endif

  // true if waiting for OnIOComplete to be called
  bool is_waiting_;

  // debugging state - is there a read already in progress
  bool read_in_progress_;

  // The result and error code of asynchronous IO.  It is modified by the
  // status callback functions on asynchronous IO completion and passed to
  // CallOnIOComplete.  Since there is at most one pending IO, the object
  // can reuse the async_result_ member for all its asynchronous IOs.
  AsyncResult async_result_;

  Lock loop_lock_;
  MessageLoop* loop_;

  DISALLOW_EVIL_CONSTRUCTORS(URLRequestInetJob);
};

#endif  // NET_URL_REQUEST_URL_REQUEST_INET_JOB_H__

