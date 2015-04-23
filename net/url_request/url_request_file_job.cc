// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
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
#include "base/platform_file.h"
#include "base/string_util.h"
#include "base/worker_pool.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_file_dir_job.h"

#if defined(OS_WIN)
class URLRequestFileJob::AsyncResolver :
    public base::RefCountedThreadSafe<URLRequestFileJob::AsyncResolver> {
 public:
  explicit AsyncResolver(URLRequestFileJob* owner)
      : owner_(owner), owner_loop_(MessageLoop::current()) {
  }

  void Resolve(const FilePath& file_path) {
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
  FilePath file_path;
  if (net::FileURLToFilePath(request->url(), &file_path) &&
      file_util::EnsureEndsWithSeparator(&file_path) &&
      file_path.IsAbsolute())
    return new URLRequestFileDirJob(request, file_path);

  // Use a regular file request job for all non-directories (including invalid
  // file names).
  return new URLRequestFileJob(request, file_path);
}

URLRequestFileJob::URLRequestFileJob(URLRequest* request,
                                     const FilePath& file_path)
    : URLRequestJob(request),
      file_path_(file_path),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          io_callback_(this, &URLRequestFileJob::DidRead)),
      is_directory_(false),
      remaining_bytes_(0) {
}

URLRequestFileJob::~URLRequestFileJob() {
#if defined(OS_WIN)
  DCHECK(!async_resolver_);
#endif
}

void URLRequestFileJob::Start() {
#if defined(OS_WIN)
  // Resolve UNC paths on a background thread.
  if (!file_path_.value().compare(0, 2, L"\\\\")) {
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

bool URLRequestFileJob::ReadRawData(net::IOBuffer* dest, int dest_size,
                                    int *bytes_read) {
  DCHECK_NE(dest_size, 0);
  DCHECK(bytes_read);
  DCHECK_GE(remaining_bytes_, 0);

  if (remaining_bytes_ < dest_size)
    dest_size = static_cast<int>(remaining_bytes_);

  // If we should copy zero bytes because |remaining_bytes_| is zero, short
  // circuit here.
  if (!dest_size) {
    *bytes_read = 0;
    return true;
  }

  int rv = stream_.Read(dest->data(), dest_size, &io_callback_);
  if (rv >= 0) {
    // Data is immediately available.
    *bytes_read = rv;
    remaining_bytes_ -= rv;
    DCHECK_GE(remaining_bytes_, 0);
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

bool URLRequestFileJob::GetContentEncodings(
    std::vector<Filter::FilterType>* encoding_types) {
  DCHECK(encoding_types->empty());

  // Bug 9936 - .svgz files needs to be decompressed.
  if (LowerCaseEqualsASCII(file_path_.Extension(), ".svgz"))
    encoding_types->push_back(Filter::FILTER_TYPE_GZIP);

  return !encoding_types->empty();
}

bool URLRequestFileJob::GetMimeType(std::string* mime_type) const {
  DCHECK(request_);
  return net::GetMimeTypeFromFile(file_path_, mime_type);
}

void URLRequestFileJob::SetExtraRequestHeaders(const std::string& headers) {
  // We only care about "Range" header here.
  std::vector<net::HttpByteRange> ranges;
  if (net::HttpUtil::ParseRanges(headers, &ranges)) {
    if (ranges.size() == 1) {
      byte_range_ = ranges[0];
    } else {
      // We don't support multiple range requests in one single URL request,
      // because we need to do multipart encoding here.
      // TODO(hclam): decide whether we want to support multiple range requests.
      NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
                 net::ERR_REQUEST_RANGE_NOT_SATISFIABLE));
    }
  }
}

void URLRequestFileJob::DidResolve(
    bool exists, const file_util::FileInfo& file_info) {
#if defined(OS_WIN)
  async_resolver_ = NULL;
#endif

  // We may have been orphaned...
  if (!request_)
    return;

  int rv = net::OK;
  // We use URLRequestFileJob to handle valid and invalid files as well as
  // invalid directories. For a directory to be invalid, it must either not
  // exist, or be "\" on Windows. (Windows resolves "\" to "C:\", thus
  // reporting it as existent.) On POSIX, we don't count any existent
  // directory as invalid.
  if (!exists || file_info.is_directory) {
    rv = net::ERR_FILE_NOT_FOUND;
  } else {
    int flags = base::PLATFORM_FILE_OPEN |
                base::PLATFORM_FILE_READ |
                base::PLATFORM_FILE_ASYNC;
    rv = stream_.Open(file_path_, flags);
  }

  if (rv != net::OK) {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, rv));
    return;
  }

  if (!byte_range_.ComputeBounds(file_info.size)) {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
               net::ERR_REQUEST_RANGE_NOT_SATISFIABLE));
    return;
  }

  remaining_bytes_ = byte_range_.last_byte_position() -
                     byte_range_.first_byte_position() + 1;
  DCHECK_GE(remaining_bytes_, 0);

  // Do the seek at the beginning of the request.
  if (remaining_bytes_ > 0 &&
      byte_range_.first_byte_position() != 0 &&
      byte_range_.first_byte_position() !=
          stream_.Seek(net::FROM_BEGIN, byte_range_.first_byte_position())) {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
               net::ERR_REQUEST_RANGE_NOT_SATISFIABLE));
    return;
  }

  set_expected_content_size(remaining_bytes_);
  NotifyHeadersComplete();
}

void URLRequestFileJob::DidRead(int result) {
  if (result > 0) {
    SetStatus(URLRequestStatus());  // Clear the IO_PENDING status
  } else if (result == 0) {
    NotifyDone(URLRequestStatus());
  } else {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, result));
  }

  remaining_bytes_ -= result;
  DCHECK_GE(remaining_bytes_, 0);

  NotifyReadComplete(result);
}

bool URLRequestFileJob::IsRedirectResponse(
    GURL* location, int* http_status_code) {
#if defined(OS_WIN)
  std::wstring extension =
      file_util::GetFileExtensionFromPath(file_path_.value());

  // Follow a Windows shortcut.
  // We just resolve .lnk file, ignore others.
  if (!LowerCaseEqualsASCII(extension, "lnk"))
    return false;

  std::wstring new_path = file_path_.value();
  bool resolved;
  resolved = file_util::ResolveShortcut(&new_path);

  // If shortcut is not resolved succesfully, do not redirect.
  if (!resolved)
    return false;

  *location = net::FilePathToFileURL(FilePath(new_path));
  *http_status_code = 301;
  return true;
#else
  return false;
#endif
}
