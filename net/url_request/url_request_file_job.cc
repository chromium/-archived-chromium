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

// For loading files, we make use of overlapped i/o to ensure that reading from
// the filesystem (e.g., a network filesystem) does not block the calling
// thread.  An alternative approach would be to use a background thread or pool
// of threads, but it seems better to leverage the operating system's ability
// to do background file reads for us.
//
// Since overlapped reads require a 'static' buffer for the duration of the
// asynchronous read, the URLRequestFileJob keeps a buffer as a member var.  In
// URLRequestFileJob::Read, data is simply copied from the object's buffer into
// the given buffer.  If there is no data to copy, the URLRequestFileJob
// attempts to read more from the file to fill its buffer.  If reading from the
// file does not complete synchronously, then the URLRequestFileJob waits for a
// signal from the OS that the overlapped read has completed.  It does so by
// leveraging the MessageLoop::WatchObject API.

#include <process.h>
#include <windows.h>

#include "net/url_request/url_request_file_job.h"

#include "base/file_util.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/wininet_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_file_dir_job.h"

using net::WinInetUtil;

namespace {

// Thread used to run ResolveFile. The parameter is a pointer to the
// URLRequestFileJob object.
DWORD WINAPI NetworkFileThread(LPVOID param) {
  URLRequestFileJob* job = reinterpret_cast<URLRequestFileJob*>(param);
  job->ResolveFile();
  return 0;
}

}  // namespace

// static
URLRequestJob* URLRequestFileJob::Factory(URLRequest* request,
                                          const std::string& scheme) {
  std::wstring file_path;
  if (net_util::FileURLToFilePath(request->url(), &file_path)) {
    if (file_path[file_path.size() - 1] == file_util::kPathSeparator) {
      // Only directories have trailing slashes.
      return new URLRequestFileDirJob(request, file_path);
    }
  }

  // Use a regular file request job for all non-directories (including invalid
  // file names).
  URLRequestFileJob* job = new URLRequestFileJob(request);
  job->file_path_ = file_path;
  return job;
}

URLRequestFileJob::URLRequestFileJob(URLRequest* request)
    : URLRequestJob(request),
      handle_(INVALID_HANDLE_VALUE),
      is_waiting_(false),
      is_directory_(false),
      is_not_found_(false),
      network_file_thread_(NULL),
      loop_(NULL) {
  memset(&overlapped_, 0, sizeof(overlapped_));
}

URLRequestFileJob::~URLRequestFileJob() {
  CloseHandles();

  // The thread might still be running. We need to kill it because it holds
  // a reference to this object.
  if (network_file_thread_) {
    TerminateThread(network_file_thread_, 0);
    CloseHandle(network_file_thread_);
  }
}

// This function can be called on the main thread or on the network file thread.
// When the request is done, we call StartAsync on the main thread.
void URLRequestFileJob::ResolveFile() {
  WIN32_FILE_ATTRIBUTE_DATA file_attributes = {0};
  if (!GetFileAttributesEx(file_path_.c_str(),
                           GetFileExInfoStandard,
                           &file_attributes)) {
    is_not_found_ = true;
  } else {
    if (file_attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      is_directory_ = true;
    } else {
      // Set the file size as expected size to be read.
      ULARGE_INTEGER file_size;
      file_size.HighPart = file_attributes.nFileSizeHigh;
      file_size.LowPart = file_attributes.nFileSizeLow;

      set_expected_content_size(file_size.QuadPart);
    }
  }


  // We need to protect the loop_ pointer with a lock because if we are running
  // on the network file thread, it is possible that the main thread is
  // executing Kill() at this moment. Kill() sets the loop_ to NULL because it
  // might be going away.
  AutoLock lock(loop_lock_);
  if (loop_) {
    // Start reading asynchronously so that all error reporting and data
    // callbacks happen as they would for network requests.
    loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &URLRequestFileJob::StartAsync));
  }
}

void URLRequestFileJob::Start() {
  // This is the loop on which we should execute StartAsync. This is used in
  // ResolveFile().
  loop_ = MessageLoop::current();

  if (!file_path_.compare(0, 2, L"\\\\")) {
    // This is on a network share. It might be slow to check if it's a directory
    // or a file. We need to do it in another thread.
    network_file_thread_ = CreateThread(NULL, 0, &NetworkFileThread, this, 0,
                                        NULL);
  } else {
    // We can call the function directly because it's going to be fast.
    ResolveFile();
  }
}

void URLRequestFileJob::Kill() {
  // If we are killed while waiting for an overlapped result...
  if (is_waiting_) {
    MessageLoop::current()->WatchObject(overlapped_.hEvent, NULL);
    is_waiting_ = false;
    Release();
  }
  CloseHandles();
  URLRequestJob::Kill();

  // It is possible that the network file thread is running and will invoke
  // StartAsync() on loop_. We set loop_ to NULL here because the message loop
  // might be going away and we don't want the other thread to call StartAsync()
  // on this loop anymore. We protect loop_ with a lock in case the other thread
  // is currenly invoking the call.
  AutoLock lock(loop_lock_);
  loop_ = NULL;
}

bool URLRequestFileJob::ReadRawData(char* dest, int dest_size,
                                    int *bytes_read) {
  DCHECK_NE(dest_size, 0);
  DCHECK(bytes_read);
  DCHECK(!is_waiting_);

  *bytes_read = 0;

  DWORD bytes;
  if (ReadFile(handle_, dest, dest_size, &bytes, &overlapped_)) {
    // data is immediately available
    overlapped_.Offset += bytes;
    *bytes_read = static_cast<int>(bytes);
    DCHECK(!is_waiting_);
    DCHECK(!is_done());
    return true;
  }

  // otherwise, a read error occured.
  DWORD err = GetLastError();
  if (err == ERROR_IO_PENDING) {
    // OK, wait for the object to become signaled
    MessageLoop::current()->WatchObject(overlapped_.hEvent, this);
    is_waiting_ = true;
    SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
    AddRef();
    return false;
  }
  if (err == ERROR_HANDLE_EOF) {
    // OK, nothing more to read
    return true;
  }

  NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
                              WinInetUtil::OSErrorToNetError(err)));
  return false;
}

bool URLRequestFileJob::GetMimeType(std::string* mime_type) {
  DCHECK(request_);
  return mime_util::GetMimeTypeFromFile(file_path_, mime_type);
}

void URLRequestFileJob::CloseHandles() {
  DCHECK(!is_waiting_);
  if (handle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(handle_);
    handle_ = INVALID_HANDLE_VALUE;
  }
  if (overlapped_.hEvent) {
    CloseHandle(overlapped_.hEvent);
    overlapped_.hEvent = NULL;
  }
}

void URLRequestFileJob::StartAsync() {
  if (network_file_thread_) {
    CloseHandle(network_file_thread_);
    network_file_thread_ = NULL;
  }

  // The request got killed, we need to stop.
  if (!loop_)
    return;

  // We may have been orphaned...
  if (!request_)
    return;

  // This is not a file, this is a directory.
  if (is_directory_) {
    NotifyHeadersComplete();
    return;
  }

  if (is_not_found_) {
    // some kind of invalid file URI
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
                                net::ERR_FILE_NOT_FOUND));
  } else {
    handle_ = CreateFile(file_path_.c_str(),
                         GENERIC_READ|SYNCHRONIZE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,
                         OPEN_EXISTING,
                         FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN,
                         NULL);
    if (handle_ == INVALID_HANDLE_VALUE) {
      NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
                 WinInetUtil::OSErrorToNetError(GetLastError())));
    } else {
      // Get setup for overlapped reads (w/ a manual reset event)
      overlapped_.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    }
  }

  NotifyHeadersComplete();
}

void URLRequestFileJob::OnObjectSignaled(HANDLE object) {
  DCHECK(overlapped_.hEvent == object);
  DCHECK(is_waiting_);

  // We'll resume watching this handle if need be when we do
  // another IO.
  MessageLoop::current()->WatchObject(object, NULL);
  is_waiting_ = false;

  DWORD bytes_read = 0;
  if (!GetOverlappedResult(handle_, &overlapped_, &bytes_read, FALSE)) {
    DWORD err = GetLastError();
    if (err == ERROR_HANDLE_EOF) {
      // successfully read all data
      NotifyDone(URLRequestStatus());
    } else {
      NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
                                  WinInetUtil::OSErrorToNetError(err)));
    }
  } else if (bytes_read) {
    overlapped_.Offset += bytes_read;
    // clear the IO_PENDING status
    SetStatus(URLRequestStatus());
  } else {
    // there was no more data so we're done
    NotifyDone(URLRequestStatus());
  }
  NotifyReadComplete(bytes_read);

  Release();  // Balance with AddRef from FillBuf; may destroy |this|
}

bool URLRequestFileJob::IsRedirectResponse(GURL* location,
                                           int* http_status_code) {
  if (is_directory_) {
    // This happens when we discovered the file is a directory, so needs a slash
    // at the end of the path.
    std::string new_path = request_->url().path();
    new_path.push_back('/');
    GURL::Replacements replacements;
    replacements.SetPathStr(new_path);

    *location = request_->url().ReplaceComponents(replacements);
    *http_status_code = 301;  // simulate a permanent redirect
    return true;
  }

  size_t found;
  found = file_path_.find_last_of('.');

  // We just resolve .lnk file, ignor others.
  if (found == std::string::npos ||
      !LowerCaseEqualsASCII(file_path_.substr(found), ".lnk"))
    return false;

  std::wstring new_path = file_path_;
  bool resolved;
  resolved = file_util::ResolveShortcut(&new_path);

  // If shortcut is not resolved succesfully, do not redirect.
  if (!resolved)
    return false;

  *location = net_util::FilePathToFileURL(new_path);
  *http_status_code = 301;
  return true;
}
