// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/url_request_slow_download_job.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"

const int kFirstDownloadSize = 1024 * 35;
const int kSecondDownloadSize = 1024 * 10;

const wchar_t URLRequestSlowDownloadJob::kUnknownSizeUrl[] =
  L"http://url.handled.by.slow.download/download-unknown-size";
const wchar_t URLRequestSlowDownloadJob::kKnownSizeUrl[] =
  L"http://url.handled.by.slow.download/download-known-size";
const wchar_t URLRequestSlowDownloadJob::kFinishDownloadUrl[] =
  L"http://url.handled.by.slow.download/download-finish";

std::vector<URLRequestSlowDownloadJob*>
    URLRequestSlowDownloadJob::kPendingRequests;

void URLRequestSlowDownloadJob::Start() {
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(this,
      &URLRequestSlowDownloadJob::StartAsync));
}

/* static */
void URLRequestSlowDownloadJob::AddUITestUrls() {
  URLRequestFilter* filter = URLRequestFilter::GetInstance();
  filter->AddUrlHandler(GURL(WideToUTF8(kUnknownSizeUrl)),
                        &URLRequestSlowDownloadJob::Factory);
  filter->AddUrlHandler(GURL(WideToUTF8(kKnownSizeUrl)),
                        &URLRequestSlowDownloadJob::Factory);
  filter->AddUrlHandler(GURL(WideToUTF8(kFinishDownloadUrl)),
                        &URLRequestSlowDownloadJob::Factory);
}

/*static */
URLRequestJob* URLRequestSlowDownloadJob::Factory(URLRequest* request,
    const std::string& scheme) {
  URLRequestSlowDownloadJob* job = new URLRequestSlowDownloadJob(request);
  if (request->url().spec() != WideToUTF8(kFinishDownloadUrl))
    URLRequestSlowDownloadJob::kPendingRequests.push_back(job);
  return job;
}

/* static */
void URLRequestSlowDownloadJob::FinishPendingRequests() {
  typedef std::vector<URLRequestSlowDownloadJob*> JobList;
  for (JobList::iterator it = kPendingRequests.begin(); it !=
       kPendingRequests.end(); ++it) {
    (*it)->set_should_finish_download();
  }
  kPendingRequests.clear();
}

URLRequestSlowDownloadJob::URLRequestSlowDownloadJob(URLRequest* request)
  : URLRequestJob(request),
    first_download_size_remaining_(kFirstDownloadSize),
    should_finish_download_(false),
    should_send_second_chunk_(false) {
}

void URLRequestSlowDownloadJob::StartAsync() {
  if (LowerCaseEqualsASCII(kFinishDownloadUrl, request_->url().spec().c_str()))
    URLRequestSlowDownloadJob::FinishPendingRequests();

  NotifyHeadersComplete();
}

bool URLRequestSlowDownloadJob::ReadRawData(net::IOBuffer* buf, int buf_size,
                                            int *bytes_read) {
  if (LowerCaseEqualsASCII(kFinishDownloadUrl,
                           request_->url().spec().c_str())) {
    *bytes_read = 0;
    return true;
  }

  if (should_send_second_chunk_) {
    DCHECK(buf_size > kSecondDownloadSize);
    for (int i = 0; i < kSecondDownloadSize; ++i) {
      buf->data()[i] = '*';
    }
    *bytes_read = kSecondDownloadSize;
    should_send_second_chunk_ = false;
    return true;
  }

  if (first_download_size_remaining_ > 0) {
    int send_size = std::min(first_download_size_remaining_, buf_size);
    for (int i = 0; i < send_size; ++i) {
      buf->data()[i] = '*';
    }
    *bytes_read = send_size;
    first_download_size_remaining_ -= send_size;

    SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
    DCHECK(!is_done());
    return true;
  }

  if (should_finish_download_) {
    *bytes_read = 0;
    return true;
  }

  // If we make it here, the first chunk has been sent and we need to wait
  // until a request is made for kFinishDownloadUrl.
  MessageLoop::current()->PostDelayedTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestSlowDownloadJob::CheckDoneStatus), 100);
  AddRef();

  // Return false to signal there is pending data.
  return false;
}

void URLRequestSlowDownloadJob::CheckDoneStatus() {
  if (should_finish_download_) {
    should_send_second_chunk_ = true;
    SetStatus(URLRequestStatus());
    NotifyReadComplete(kSecondDownloadSize);
    Release();
  } else {
    MessageLoop::current()->PostDelayedTask(FROM_HERE, NewRunnableMethod(
        this, &URLRequestSlowDownloadJob::CheckDoneStatus), 100);
  }
}

// Public virtual version.
void URLRequestSlowDownloadJob::GetResponseInfo(net::HttpResponseInfo* info) {
  // Forward to private const version.
  GetResponseInfoConst(info);
}

// Private const version.
void URLRequestSlowDownloadJob::GetResponseInfoConst(
    net::HttpResponseInfo* info) const {
  // Send back mock headers.
  std::string raw_headers;
  if (LowerCaseEqualsASCII(kFinishDownloadUrl,
                           request_->url().spec().c_str())) {
    raw_headers.append(
      "HTTP/1.1 200 OK\n"
      "Content-type: text/plain\n");
  } else {
    raw_headers.append(
      "HTTP/1.1 200 OK\n"
      "Content-type: application/octet-stream\n"
      "Cache-Control: max-age=0\n");

    if (LowerCaseEqualsASCII(kKnownSizeUrl, request_->url().spec().c_str())) {
      raw_headers.append(StringPrintf("Content-Length: %d\n",
          kFirstDownloadSize + kSecondDownloadSize));
    }
  }

  // ParseRawHeaders expects \0 to end each header line.
  ReplaceSubstringsAfterOffset(&raw_headers, 0, "\n", std::string("\0", 1));
  info->headers = new net::HttpResponseHeaders(raw_headers);
}

bool URLRequestSlowDownloadJob::GetMimeType(std::string* mime_type) const {
  net::HttpResponseInfo info;
  GetResponseInfoConst(&info);
  return info.headers && info.headers->GetMimeType(mime_type);
}
