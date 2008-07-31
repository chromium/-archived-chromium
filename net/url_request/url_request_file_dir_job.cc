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

#include "net/url_request/url_request_file_dir_job.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "net/base/wininet_util.h"
#include "net/url_request/url_request.h"

using std::string;
using std::wstring;

using net::WinInetUtil;

URLRequestFileDirJob::URLRequestFileDirJob(URLRequest* request,
                                           const wstring& dir_path)
    : URLRequestJob(request),
      dir_path_(dir_path),
      canceled_(false),
      list_complete_(false),
      wrote_header_(false),
      read_pending_(false),
      read_buffer_(NULL),
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

bool URLRequestFileDirJob::ReadRawData(char* buf, int buf_size,
                                       int *bytes_read) {
  DCHECK(bytes_read);
  *bytes_read = 0;

  if (is_done())
    return true;

  if (FillReadBuffer(buf, buf_size, bytes_read))
    return true;

  // We are waiting for more data
  read_pending_ = true;
  read_buffer_ = buf;
  read_buffer_length_ = buf_size;
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
  return false;
}

bool URLRequestFileDirJob::GetMimeType(string* mime_type) {
  *mime_type = "text/html";
  return true;
}

bool URLRequestFileDirJob::GetCharset(string* charset) {
  // All the filenames are converted to UTF-8 before being added.
  *charset = "utf-8";
  return true;
}

void URLRequestFileDirJob::OnListFile(const WIN32_FIND_DATA& data) {
  FILETIME local_time;
  FileTimeToLocalFileTime(&data.ftLastWriteTime, &local_time);
  int64 size = (static_cast<unsigned __int64>(data.nFileSizeHigh) << 32) |
      data.nFileSizeLow;

  // We wait to write out the header until we get the first file, so that we
  // can catch errors from DirectoryLister and show an error page.
  if (!wrote_header_) {
    data_.append(net::GetDirectoryListingHeader(WideToUTF8(dir_path_)));
    wrote_header_ = true;
  }

  data_.append(net::GetDirectoryListingEntry(
      WideToUTF8(data.cFileName), data.dwFileAttributes, size, &local_time));

  // TODO(darin): coalesce more?

  CompleteRead();
}

void URLRequestFileDirJob::OnListDone(int error) {
  CloseLister();

  if (error) {
    read_pending_ = false;
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED,
                                WinInetUtil::OSErrorToNetError(error)));
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
    if (FillReadBuffer(read_buffer_, read_buffer_length_, &bytes_read)) {
      // We completed the read, so reset the read buffer.
      read_pending_ = false;
      read_buffer_ = NULL;
      read_buffer_length_ = 0;

      SetStatus(URLRequestStatus());
      NotifyReadComplete(bytes_read);
    } else {
      NOTREACHED();
      NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, 0));  // TODO: Better error code.
    }
  }
}
