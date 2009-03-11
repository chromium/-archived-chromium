// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_file_dir_job.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request.h"

#if defined(OS_POSIX)
#include <sys/stat.h>
#endif

using std::string;

URLRequestFileDirJob::URLRequestFileDirJob(URLRequest* request,
                                           const FilePath& dir_path)
    : URLRequestJob(request),
      dir_path_(dir_path),
      canceled_(false),
      list_complete_(false),
      wrote_header_(false),
      read_pending_(false),
      read_buffer_length_(0) {
}

URLRequestFileDirJob::~URLRequestFileDirJob() {
  DCHECK(read_pending_ == false);
  DCHECK(lister_ == NULL);
}

void URLRequestFileDirJob::Start() {
  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestFileDirJob::StartAsync));
}

void URLRequestFileDirJob::StartAsync() {
  DCHECK(!lister_);

  // AddRef so that *this* cannot be destroyed while the lister_
  // is trying to feed us data.

  AddRef();
  lister_ = new net::DirectoryLister(dir_path_, this);
  lister_->Start();

  NotifyHeadersComplete();
}

void URLRequestFileDirJob::Kill() {
  if (canceled_)
    return;

  canceled_ = true;

  // Don't call CloseLister or dispatch an error to the URLRequest because we
  // want OnListDone to be called to also write the error to the output stream.
  // OnListDone will notify the URLRequest at this time.
  if (lister_)
    lister_->Cancel();
}

bool URLRequestFileDirJob::ReadRawData(net::IOBuffer* buf, int buf_size,
                                       int *bytes_read) {
  DCHECK(bytes_read);
  *bytes_read = 0;

  if (is_done())
    return true;

  if (FillReadBuffer(buf->data(), buf_size, bytes_read))
    return true;

  // We are waiting for more data
  read_pending_ = true;
  read_buffer_ = buf;
  read_buffer_length_ = buf_size;
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
  return false;
}

bool URLRequestFileDirJob::GetMimeType(string* mime_type) const {
  *mime_type = "text/html";
  return true;
}

bool URLRequestFileDirJob::GetCharset(string* charset) {
  // All the filenames are converted to UTF-8 before being added.
  *charset = "utf-8";
  return true;
}

void URLRequestFileDirJob::OnListFile(
    const file_util::FileEnumerator::FindInfo& data) {
  // We wait to write out the header until we get the first file, so that we
  // can catch errors from DirectoryLister and show an error page.
  if (!wrote_header_) {
#if defined(OS_WIN)
    const std::string& title = WideToUTF8(dir_path_.value());
#elif defined(OS_POSIX)
    const std::string& title = dir_path_.value();
#endif
    data_.append(net::GetDirectoryListingHeader(title));
    wrote_header_ = true;
  }

#if defined(OS_WIN)
  FILETIME local_time;
  ::FileTimeToLocalFileTime(&data.ftLastWriteTime, &local_time);
  int64 size = (static_cast<unsigned __int64>(data.nFileSizeHigh) << 32) |
      data.nFileSizeLow;

  data_.append(net::GetDirectoryListingEntry(
      WideToUTF8(data.cFileName),
      (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? true : false,
      size,
      base::Time::FromFileTime(local_time)));

#elif defined(OS_POSIX)
  data_.append(net::GetDirectoryListingEntry(
      data.filename.c_str(),
      S_ISDIR(data.stat.st_mode),
      data.stat.st_size,
      base::Time::FromTimeT(data.stat.st_mtime)));
#endif

  // TODO(darin): coalesce more?
  CompleteRead();
}

void URLRequestFileDirJob::OnListDone(int error) {
  CloseLister();

  if (error) {
    read_pending_ = false;
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, error));
  } else if (canceled_) {
    read_pending_ = false;
    NotifyCanceled();
  } else {
    list_complete_ = true;
    CompleteRead();
  }

  Release();  // The Lister is finished; may delete *this*
}

void URLRequestFileDirJob::CloseLister() {
  if (lister_) {
    lister_->Cancel();
    lister_->set_delegate(NULL);
    lister_ = NULL;
  }
}

bool URLRequestFileDirJob::FillReadBuffer(char *buf, int buf_size,
                                          int *bytes_read) {
  DCHECK(bytes_read);

  *bytes_read = 0;

  int count = std::min(buf_size, static_cast<int>(data_.size()));
  if (count) {
    memcpy(buf, &data_[0], count);
    data_.erase(0, count);
    *bytes_read = count;
    return true;
  } else if (list_complete_) {
    // EOF
    return true;
  }
  return false;
}

void URLRequestFileDirJob::CompleteRead() {
  if (read_pending_) {
    int bytes_read;
    if (FillReadBuffer(read_buffer_->data(), read_buffer_length_,
                       &bytes_read)) {
      // We completed the read, so reset the read buffer.
      read_pending_ = false;
      read_buffer_ = NULL;
      read_buffer_length_ = 0;

      SetStatus(URLRequestStatus());
      NotifyReadComplete(bytes_read);
    } else {
      NOTREACHED();
      // TODO: Better error code.
      NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, 0));
    }
  }
}

bool URLRequestFileDirJob::IsRedirectResponse(
    GURL* location, int* http_status_code) {
  // If the URL did not have a trailing slash, treat the response as a redirect
  // to the URL with a trailing slash appended.
  std::string path = request_->url().path();
  if (path.empty() || (path[path.size() - 1] != '/')) {
    // This happens when we discovered the file is a directory, so needs a
    // slash at the end of the path.
    std::string new_path = path;
    new_path.push_back('/');
    GURL::Replacements replacements;
    replacements.SetPathStr(new_path);

    *location = request_->url().ReplaceComponents(replacements);
    *http_status_code = 301;  // simulate a permanent redirect
    return true;
  }

  return false;
}
