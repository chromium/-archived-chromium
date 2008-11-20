// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

#include "net/url_request/url_request_file_job.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/worker_pool.h"
#include "googleurl/src/gurl.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_file_dir_job.h"

#if defined(OS_WIN)
class URLRequestFileJob::AsyncResolver :
    public base::RefCountedThreadSafe<URLRequestFileJob::AsyncResolver> {
 public:
  explicit AsyncResolver(URLRequestFileJob* owner)
      : owner_(owner), owner_loop_(MessageLoop::current()) {
  }

  void Resolve(const std::wstring& file_path) {
    file_util::FileInfo file_info;
    bool exists = file_util::GetFileInfo(file_path, &file_info);
    AutoLock locked(lock_);
    if (owner_loop_) {
      owner_loop_->PostTask(FROM_HERE, NewRunnableMethod(
          this, &AsyncResolver::ReturnResults, exists, file_info));
    }
  }

  void Cancel() {
    owner_ = NULL;

    AutoLock locked(lock_);
    owner_loop_ = NULL;
  }

 private:
  void ReturnResults(bool exists, const file_util::FileInfo& file_info) {
    if (owner_)
      owner_->DidResolve(exists, file_info);
  }

  URLRequestFileJob* owner_;

  Lock lock_;
  MessageLoop* owner_loop_;
};
#endif

// static
URLRequestJob* URLRequestFileJob::Factory(
    URLRequest* request, const std::string& scheme) {
  std::wstring file_path;
  if (net::FileURLToFilePath(request->url(), &file_path)) {
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
      ALLOW_THIS_IN_INITIALIZER_LIST(
          io_callback_(this, &URLRequestFileJob::DidRead)),
      is_directory_(false) {
}

URLRequestFileJob::~URLRequestFileJob() {
#if defined(OS_WIN)
  DCHECK(!async_resolver_);
#endif
}

void URLRequestFileJob::Start() {
#if defined(OS_WIN)
  // Resolve UNC paths on a background thread.
  if (!file_path_.compare(0, 2, L"\\\\")) {
    DCHECK(!async_resolver_);
    async_resolver_ = new AsyncResolver(this);
    WorkerPool::PostTask(FROM_HERE, NewRunnableMethod(
        async_resolver_.get(), &AsyncResolver::Resolve, file_path_), true);
    return;
  }
#endif
  file_util::FileInfo file_info;
  bool exists = file_util::GetFileInfo(file_path_, &file_info);

  // Continue asynchronously.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestFileJob::DidResolve, exists, file_info));
}

void URLRequestFileJob::Kill() {
  stream_.Close();

#if defined(OS_WIN)
  if (async_resolver_) {
    async_resolver_->Cancel();
    async_resolver_ = NULL;
  }
#endif

  URLRequestJob::Kill();
}

bool URLRequestFileJob::ReadRawData(
    char* dest, int dest_size, int *bytes_read) {
  DCHECK_NE(dest_size, 0);
  DCHECK(bytes_read);

  int rv = stream_.Read(dest, dest_size, &io_callback_);
  if (rv >= 0) {
    // Data is immediately available.
    *bytes_read = rv;
    return true;
  }

  // Otherwise, a read error occured.  We may just need to wait...
  if (rv == net::ERR_IO_PENDING) {
    SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
  } else {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, rv));
  }
  return false;
}

bool URLRequestFileJob::GetMimeType(std::string* mime_type) {
  DCHECK(request_);
  return net::GetMimeTypeFromFile(file_path_, mime_type);
}

void URLRequestFileJob::DidResolve(
    bool exists, const file_util::FileInfo& file_info) {
#if defined(OS_WIN)
  async_resolver_ = NULL;
#endif

  // We may have been orphaned...
  if (!request_)
    return;

  is_directory_ = file_info.is_directory;

  int rv = net::OK;
  if (!exists) {
    rv = net::ERR_FILE_NOT_FOUND;
  } else if (!is_directory_) {
    int flags = base::PLATFORM_FILE_OPEN | 
                base::PLATFORM_FILE_READ |
                base::PLATFORM_FILE_ASYNC;
    rv = stream_.Open(file_path_, flags);
  }

  if (rv == net::OK) {
    set_expected_content_size(file_info.size);
    NotifyHeadersComplete();
  } else {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, rv));
  }
}

void URLRequestFileJob::DidRead(int result) {
  if (result > 0) {
    SetStatus(URLRequestStatus());  // Clear the IO_PENDING status
  } else if (result == 0) {
    NotifyDone(URLRequestStatus());
  } else {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, result));
  }
  NotifyReadComplete(result);
}

bool URLRequestFileJob::IsRedirectResponse(
    GURL* location, int* http_status_code) {
  if (is_directory_) {
    // This happens when we discovered the file is a directory, so needs a
    // slash at the end of the path.
    std::string new_path = request_->url().path();
    new_path.push_back('/');
    GURL::Replacements replacements;
    replacements.SetPathStr(new_path);

    *location = request_->url().ReplaceComponents(replacements);
    *http_status_code = 301;  // simulate a permanent redirect
    return true;
  }

#if defined(OS_WIN)
  // Follow a Windows shortcut.
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

  *location = net::FilePathToFileURL(new_path);
  *http_status_code = 301;
  return true;
#else
  return false;
#endif
}

