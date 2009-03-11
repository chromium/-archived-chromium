// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_inet_job.h"

#include <algorithm>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/auth.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/wininet_util.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_ftp_job.h"
#include "net/url_request/url_request_job_metrics.h"
#include "net/url_request/url_request_job_tracker.h"

using net::WinInetUtil;

//
// HOW ASYNC IO WORKS
//
// The URLRequestInet* classes are now fully asynchronous.  This means that
// all IO operations pass buffers into WinInet, and as WinInet completes those
// IO requests, it will fill the buffer, and then callback to the client.
// Asynchronous IO Operations include:
//    HttpSendRequestEx
//    InternetWriteFile
//    HttpEndRequest
//    InternetOpenUrl
//    InternetReadFile (for FTP)
//    InternetReadFileEx (for HTTP)
//    InternetCloseHandle
//
// To understand how this works, you need to understand the basic class
// hierarchy for the URLRequestJob classes:
//
//                      URLRequestJob
//                            |
//             +--------------+-------------------+
//             |                                  |
//      (Other Job Types)                 URLRequestInetJob
//            e.g.                        |               |
//      URLRequestFileJob         URLRequestFtpJob  URLRequestHttpJob
//                                                        |
//                                               URLRequestHttpUploadJob
//
//
// To make this work, each URLRequestInetJob has a virtual method called
// OnIOComplete().  If a derived URLRequestInetJob class issues
// an asynchronous IO, it must override the OnIOComplete method
// to handle the IO completion.  Once it has overridden this method,
// *all* asynchronous IO completions will come to this method, even
// those asynchronous IOs which may have been issued by a base class.
// For example, URLRequestInetJob has methods which Read from the
// connection asynchronously.  Once URLRequestHttpJob overrides
// OnIOComplete (so that it can receive its own async IO callbacks)
// it will also receive the URLRequestInetJob async IO callbacks.  To
// make this work, the derived class must track its own state, and call
// the base class' version of OnIOComplete if appropriate.
//

COMPILE_ASSERT(
    sizeof(URLRequestInetJob::AsyncResult) == sizeof(INTERNET_ASYNC_RESULT),
    async_result_inconsistent_size);

HINTERNET URLRequestInetJob::the_internet_ = NULL;
#ifndef NDEBUG
MessageLoop* URLRequestInetJob::my_message_loop_ = NULL;
#endif

URLRequestInetJob::URLRequestInetJob(URLRequest* request)
    : URLRequestJob(request),
      connection_handle_(NULL),
      request_handle_(NULL),
      last_error_(ERROR_SUCCESS),
      is_waiting_(false),
      read_in_progress_(false),
      loop_(MessageLoop::current()) {
  // TODO(darin): we should re-create the internet if the UA string changes,
  // but we have to be careful about existing users of this internet.
  if (!the_internet_) {
    InitializeTheInternet(request->context() ?
        request->context()->GetUserAgent(GURL()) : std::string());
  }
#ifndef NDEBUG
  DCHECK(MessageLoop::current() == my_message_loop_) <<
      "All URLRequests should happen on the same thread";
#endif
}

URLRequestInetJob::~URLRequestInetJob() {
  DCHECK(!request_) << "request should be detached at this point";

  // The connections may have already been cleaned up. It is ok to call
  // CleanupConnection again to make sure the resource is properly released.
  // See bug 684997.
  CleanupConnection();
}

void URLRequestInetJob::Kill() {
  CleanupConnection();

  {
    AutoLock locked(loop_lock_);
    loop_ = NULL;
  }

  // Dispatch the NotifyDone message to the URLRequest
  URLRequestJob::Kill();
}

void URLRequestInetJob::SetAuth(const std::wstring& username,
                                const std::wstring& password) {
  DCHECK((proxy_auth_ && proxy_auth_->state == net::AUTH_STATE_NEED_AUTH) ||
         (server_auth_ && server_auth_->state == net::AUTH_STATE_NEED_AUTH));

  // Proxy gets set first, then WWW.
  net::AuthData* auth =
    (proxy_auth_ && proxy_auth_->state == net::AUTH_STATE_NEED_AUTH ?
     proxy_auth_.get() : server_auth_.get());

  if (auth) {
    auth->state = net::AUTH_STATE_HAVE_AUTH;
    auth->username = username;
    auth->password = password;
  }

  // Resend the request with the new username and password.
  // Do this asynchronously in case we were called from within a
  // NotifyDataAvailable callback.
  // TODO(mpcomplete): hmm... is it possible 'this' gets deleted before the task
  // is run?
  OnSetAuth();
}

void URLRequestInetJob::CancelAuth() {
  DCHECK((proxy_auth_ && proxy_auth_->state == net::AUTH_STATE_NEED_AUTH) ||
         (server_auth_ && server_auth_->state == net::AUTH_STATE_NEED_AUTH));

  // Proxy gets set first, then WWW.
  net::AuthData* auth =
    (proxy_auth_ && proxy_auth_->state == net::AUTH_STATE_NEED_AUTH ?
     proxy_auth_.get() : server_auth_.get());

  if (auth) {
    auth->state = net::AUTH_STATE_CANCELED;
  }

  // Once the auth is cancelled, we proceed with the request as though
  // there were no auth.  So, send the OnResponseStarted.  Schedule this
  // for later so that we don't cause any recursing into the caller
  // as a result of this call.
  OnCancelAuth();
}

void URLRequestInetJob::OnIOComplete(const AsyncResult& result) {
  URLRequestStatus status;

  if (read_in_progress_) {
    read_in_progress_ = false;
    int bytes_read = 0;
    if (GetReadBytes(result, &bytes_read)) {
      SetStatus(status);
      if (bytes_read == 0) {
        NotifyDone(status);
        CleanupConnection();
      }
    } else {
      bytes_read = -1;
      URLRequestStatus status;
      status.set_status(URLRequestStatus::FAILED);
      status.set_os_error(WinInetUtil::OSErrorToNetError(result.dwError));
      NotifyDone(status);
      CleanupConnection();
    }
    NotifyReadComplete(bytes_read);
  } else {
    // If we get here, an IO is completing which we didn't
    // start or we lost track of our state.
    NOTREACHED();
  }
}

bool URLRequestInetJob::ReadRawData(net::IOBuffer* dest, int dest_size,
                                    int *bytes_read) {
  if (is_done())
    return 0;

  DCHECK_NE(dest_size, 0);
  DCHECK_NE(bytes_read, (int*)NULL);
  DCHECK(!read_in_progress_);

  *bytes_read = 0;

  int result = CallInternetRead(dest->data(), dest_size, bytes_read);
  if (result == ERROR_SUCCESS) {
    DLOG(INFO) << "read " << *bytes_read << " bytes";
    if (*bytes_read == 0)
      CleanupConnection();   // finished reading all the data
    return true;
  }

  if (ProcessRequestError(result))
    read_in_progress_ = true;

  // Whether we had an error or the request is pending.
  // Both of these cases return false.
  return false;
}

void URLRequestInetJob::CallOnIOComplete(const AsyncResult& result) {
  // It's important to clear this flag before calling OnIOComplete
  is_waiting_ = false;

  // the job could have completed with an error while the message was pending
  if (!is_done()) {
    // Verify that our status is currently set to IO_PENDING and
    // reset it on success.
    DCHECK(GetStatus().is_io_pending());
    if (result.dwResult && result.dwError == 0)
      SetStatus(URLRequestStatus());

    OnIOComplete(result);
  }

  Release();  // may destroy self if last reference
}

bool URLRequestInetJob::ProcessRequestError(int error) {
  if (error == ERROR_IO_PENDING) {
    DLOG(INFO) << "waiting for WinInet call to complete";
    AddRef();  // balanced in CallOnIOComplete
    is_waiting_ = true;
    SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
    return true;
  }
  DLOG(ERROR) << "WinInet call failed: " << error;
  CleanupConnection();
  NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
                              WinInetUtil::OSErrorToNetError(error)));
  return false;
}

bool URLRequestInetJob::GetMoreData() {
  if (!is_waiting_ && !is_done()) {
    // The connection is still in the middle of transmission.
    // Return true so InternetReadFileExA can be called again.
    return true;
  } else {
    return false;
  }
}

void URLRequestInetJob::CleanupConnection() {
  if (!request_handle_ && !connection_handle_)
    return;  // nothing to clean up

  if (request_handle_) {
    CleanupHandle(request_handle_);
    request_handle_ = NULL;
  }
  if (connection_handle_) {
    CleanupHandle(connection_handle_);
    connection_handle_ = NULL;
  }
}

void URLRequestInetJob::CleanupHandle(HINTERNET handle) {
  // We no longer need notifications from this connection.
  InternetSetStatusCallback(handle, NULL);

  if (!InternetCloseHandle(handle)) {
    // InternetCloseHandle is evil. The documentation specifies that it
    // either succeeds immediately or returns ERROR_IO_PENDING if there is
    // something outstanding, in which case the close will happen automagically
    // later. In either of these cases, it will call us back with
    // INTERNET_STATUS_HANDLE_CLOSING (because we set up the async callbacks)
    // and we simply do nothing for the message.
    //
    // However, sometimes it also seems to fail with ERROR_INVALID_HANDLE.
    // This seems to happen when we cancel before it has called us back with
    // data. For example, if we cancel during DNS resolution or while waiting
    // for a slow server.
    //
    // Our speculation is that in these cases WinInet creates a handle for
    // us with an internal structure, but that the driver has not yet called
    // it back with a "real" handle (the driver level is probably what
    // generates IO_PENDING). The driver has not yet specified a handle, which
    // causes WinInet to barf.
    //
    // However, in this case, the cancel seems to work. The TCP connection is
    // closed and we still get a callback that the handle is being closed. Yay.
    //
    // We assert that the error is either of these two because we aren't sure
    // if any other error values could also indicate this bogus condition, and
    // we want to notice if we do something wrong that causes a real error.
    DWORD last_error = GetLastError();
    DCHECK(last_error == ERROR_INVALID_HANDLE) <<
        "Unknown error when closing handle, possibly leaking job";
    if (ERROR_IO_PENDING == last_error) {
      SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));

      AsyncResult result;
      result.dwError = ERROR_INTERNET_CONNECTION_ABORTED;
      result.dwResult = reinterpret_cast<DWORD_PTR>(handle);
      MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
          this, &URLRequestInetJob::CallOnIOComplete, result));
    }
  }
}

// static
HINTERNET URLRequestInetJob::GetTheInternet() {
  return the_internet_;
}

// static
void URLRequestInetJob::InitializeTheInternet(const std::string& user_agent) {
  // Hack attack.  We are hitting a deadlock in wininet deinitialization.
  // What is happening is that when we deinitialize, FreeLibrary will be
  // called on wininet.  The loader lock is held, and wininet!DllMain is
  // called.  The problem is that wininet tries to do a bunch of cleanup
  // in their DllMain, including calling ICAsyncThread::~ICASyncThread.
  // This tries to shutdown the "select thread", and then does a
  // WaitForSingleObject on the thread with a 5 sec timeout.  However the
  // thread they are waiting for cannot exit because the thread shutdown
  // routine (LdrShutdownThread) is trying to acquire the loader lock.
  // This causes chrome.exe to hang for 5 seconds on shutdown before the
  // process will exit. Making sure we close our wininet handles did not help.
  //
  // Since DLLs are reference counted, we inflate the reference count on
  // wininet so that it will never be deinitialized :)
  LoadLibraryA("wininet");

  the_internet_ = InternetOpenA(user_agent.c_str(),
                                INTERNET_OPEN_TYPE_PRECONFIG,
                                NULL,  // no proxy override
                                NULL,  // no proxy bypass list
                                INTERNET_FLAG_ASYNC);
  InternetSetStatusCallback(the_internet_, URLRequestStatusCallback);

  // Keep track of this message loop so we can catch callers who don't make
  // requests on the same thread.  Only do this in debug mode; in release mode
  // my_message_loop_ doesn't exist.
#ifndef NDEBUG
  DCHECK(!my_message_loop_) << "InitializeTheInternet() called twice";
  DCHECK(my_message_loop_ = MessageLoop::current());
#endif
}

// static
void CALLBACK URLRequestInetJob::URLRequestStatusCallback(
    HINTERNET handle, DWORD_PTR job_id, DWORD status, LPVOID status_info,
    DWORD status_info_len) {
  switch (status) {
    case INTERNET_STATUS_REQUEST_COMPLETE: {
      URLRequestInetJob* job = reinterpret_cast<URLRequestInetJob*>(job_id);

      DCHECK(status_info_len == sizeof(AsyncResult));
      AsyncResult* result = static_cast<AsyncResult*>(status_info);

      AutoLock locked(job->loop_lock_);
      if (job->loop_) {
        job->loop_->PostTask(FROM_HERE, NewRunnableMethod(
            job, &URLRequestInetJob::CallOnIOComplete, *result));
      }
      break;
    }
    case INTERNET_STATUS_USER_INPUT_REQUIRED:
    case INTERNET_STATUS_STATE_CHANGE:
      // TODO(darin): This is probably a security problem.  Do something better.
      ResumeSuspendedDownload(handle, 0);
      break;
  }
}
