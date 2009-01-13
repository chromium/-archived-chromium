// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request.h"

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/singleton.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/upload_data.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_manager.h"

#ifndef NDEBUG
URLRequestMetrics url_request_metrics;
#endif

using base::Time;
using net::UploadData;
using std::string;
using std::wstring;

// Max number of http redirects to follow.  Same number as gecko.
static const int kMaxRedirects = 20;

static URLRequestJobManager* GetJobManager() {
  return Singleton<URLRequestJobManager>::get();
}

///////////////////////////////////////////////////////////////////////////////
// URLRequest

URLRequest::URLRequest(const GURL& url, Delegate* delegate)
    : url_(url),
      original_url_(url),
      method_("GET"),
      load_flags_(net::LOAD_NORMAL),
      delegate_(delegate),
      is_pending_(false),
      user_data_(NULL),
      enable_profiling_(false),
      redirect_limit_(kMaxRedirects),
      final_upload_progress_(0) {
  URLREQUEST_COUNT_CTOR();
  SIMPLE_STATS_COUNTER("URLRequestCount");
  origin_pid_ = base::GetCurrentProcId();

  // Sanity check out environment.
  DCHECK(MessageLoop::current()) <<
      "The current MessageLoop must exist";
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type()) <<
      "The current MessageLoop must be TYPE_IO";
}

URLRequest::~URLRequest() {
  URLREQUEST_COUNT_DTOR();

  Cancel();

  if (job_)
    OrphanJob();

  delete user_data_;  // NULL check unnecessary for delete
}

// static
URLRequest::ProtocolFactory* URLRequest::RegisterProtocolFactory(
    const string& scheme, ProtocolFactory* factory) {
  return GetJobManager()->RegisterProtocolFactory(scheme, factory);
}

// static
void URLRequest::RegisterRequestInterceptor(Interceptor* interceptor) {
  GetJobManager()->RegisterRequestInterceptor(interceptor);
}

// static
void URLRequest::UnregisterRequestInterceptor(Interceptor* interceptor) {
  GetJobManager()->UnregisterRequestInterceptor(interceptor);
}

void URLRequest::AppendBytesToUpload(const char* bytes, int bytes_len) {
  DCHECK(bytes_len > 0 && bytes);
  if (!upload_)
    upload_ = new UploadData();
  upload_->AppendBytes(bytes, bytes_len);
}

void URLRequest::AppendFileRangeToUpload(const wstring& file_path,
                                         uint64 offset, uint64 length) {
  DCHECK(file_path.length() > 0 && length > 0);
  if (!upload_)
    upload_ = new UploadData();
  upload_->AppendFileRange(file_path, offset, length);
}

void URLRequest::SetExtraRequestHeaderById(int id, const string& value,
                                           bool overwrite) {
  DCHECK(!is_pending_);
  NOTREACHED() << "implement me!";
}

void URLRequest::SetExtraRequestHeaderByName(const string& name,
                                             const string& value,
                                             bool overwrite) {
  DCHECK(!is_pending_);
  NOTREACHED() << "implement me!";
}

void URLRequest::SetExtraRequestHeaders(const string& headers) {
  DCHECK(!is_pending_);
  if (headers.empty()) {
    extra_request_headers_.clear();
  } else {
#ifndef NDEBUG
    size_t crlf = headers.rfind("\r\n", headers.size() - 1);
    DCHECK(crlf != headers.size() - 2) << "headers must not end with CRLF";
#endif
    extra_request_headers_ = headers + "\r\n";
  }

  // NOTE: This method will likely become non-trivial once the other setters
  // for request headers are implemented.
}

net::LoadState URLRequest::GetLoadState() const {
  return job_ ? job_->GetLoadState() : net::LOAD_STATE_IDLE;
}

uint64 URLRequest::GetUploadProgress() const {
  if (!job_) {
    // We haven't started or the request was cancelled
    return 0;
  }
  if (final_upload_progress_) {
    // The first job completed and none of the subsequent series of
    // GETs when following redirects will upload anything, so we return the
    // cached results from the initial job, the POST.
    return final_upload_progress_;
  }
  return job_->GetUploadProgress();
}

void URLRequest::GetResponseHeaderById(int id, string* value) {
  DCHECK(job_);
  NOTREACHED() << "implement me!";
}

void URLRequest::GetResponseHeaderByName(const string& name, string* value) {
  DCHECK(value);
  if (response_info_.headers) {
    response_info_.headers->GetNormalizedHeader(name, value);
  } else {
    value->clear();
  }
}

void URLRequest::GetAllResponseHeaders(string* headers) {
  DCHECK(headers);
  if (response_info_.headers) {
    response_info_.headers->GetNormalizedHeaders(headers);
  } else {
    headers->clear();
  }
}

bool URLRequest::GetResponseCookies(ResponseCookies* cookies) {
  DCHECK(job_);
  return job_->GetResponseCookies(cookies);
}

void URLRequest::GetMimeType(string* mime_type) {
  DCHECK(job_);
  job_->GetMimeType(mime_type);
}

void URLRequest::GetCharset(string* charset) {
  DCHECK(job_);
  job_->GetCharset(charset);
}

int URLRequest::GetResponseCode() {
  DCHECK(job_);
  return job_->GetResponseCode();
}

// static
bool URLRequest::IsHandledProtocol(const std::string& scheme) {
  return GetJobManager()->SupportsScheme(scheme);
}

// static
bool URLRequest::IsHandledURL(const GURL& url) {
  if (!url.is_valid()) {
    // We handle error cases.
    return true;
  }

  return IsHandledProtocol(url.scheme());
}

void URLRequest::Start() {
  DCHECK(!is_pending_);
  DCHECK(!job_);

  job_ = GetJobManager()->CreateJob(this);
  job_->SetExtraRequestHeaders(extra_request_headers_);

  if (upload_.get())
    job_->SetUpload(upload_.get());

  is_pending_ = true;
  response_info_.request_time = Time::Now();
  response_info_.was_cached = false;

  // Don't allow errors to be sent from within Start().
  // TODO(brettw) this may cause NotifyDone to be sent synchronously,
  // we probably don't want this: they should be sent asynchronously so
  // the caller does not get reentered.
  job_->Start();
}

void URLRequest::Cancel() {
  CancelWithError(net::ERR_ABORTED);
}

void URLRequest::CancelWithError(int os_error) {
  DCHECK(os_error < 0);

  // There's nothing to do if we are not waiting on a Job.
  if (!is_pending_ || !job_)
    return;

  // If the URL request already has an error status, then canceling is a no-op.
  // Plus, we don't want to change the error status once it has been set.
  if (status_.is_success()) {
    status_.set_status(URLRequestStatus::CANCELED);
    status_.set_os_error(os_error);
  }

  job_->Kill();

  // The Job will call our NotifyDone method asynchronously.  This is done so
  // that the Delegate implementation can call Cancel without having to worry
  // about being called recursively.
}

bool URLRequest::Read(char* dest, int dest_size, int *bytes_read) {
  DCHECK(job_);
  DCHECK(bytes_read);
  DCHECK(!job_->is_done());
  *bytes_read = 0;

  if (dest_size == 0) {
    // Caller is not too bright.  I guess we've done what they asked.
    return true;
  }

  // Once the request fails or is cancelled, read will just return 0 bytes
  // to indicate end of stream.
  if (!status_.is_success()) {
    return true;
  }

  return job_->Read(dest, dest_size, bytes_read);
}

void URLRequest::SetAuth(const wstring& username, const wstring& password) {
  DCHECK(job_);
  DCHECK(job_->NeedsAuth());

  job_->SetAuth(username, password);
}

void URLRequest::CancelAuth() {
  DCHECK(job_);
  DCHECK(job_->NeedsAuth());

  job_->CancelAuth();
}

void URLRequest::ContinueDespiteLastError() {
  DCHECK(job_);

  job_->ContinueDespiteLastError();
}

void URLRequest::OrphanJob() {
  job_->DetachRequest();  // ensures that the job will not call us again
  job_ = NULL;
}

// static
std::string URLRequest::StripPostSpecificHeaders(const std::string& headers) {
  // These are headers that may be attached to a POST.
  static const char* const kPostHeaders[] = {
      "content-type",
      "content-length",
      "origin"
  };

  std::string stripped_headers;
  net::HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\r\n");

  while (it.GetNext()) {
    bool is_post_specific = false;
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kPostHeaders); ++i) {
      if (LowerCaseEqualsASCII(it.name_begin(), it.name_end(),
                               kPostHeaders[i])) {
        is_post_specific = true;
        break;
      }
    }
    if (!is_post_specific) {
      // Assume that name and values are on the same line.
      stripped_headers.append(it.name_begin(), it.values_end());
      stripped_headers.append("\r\n");
    }
  }
  return stripped_headers;
}

int URLRequest::Redirect(const GURL& location, int http_status_code) {
  if (redirect_limit_ <= 0) {
    DLOG(INFO) << "disallowing redirect: exceeds limit";
    return net::ERR_TOO_MANY_REDIRECTS;
  }

  if (!job_->IsSafeRedirect(location)) {
    DLOG(INFO) << "disallowing redirect: unsafe protocol";
    return net::ERR_UNSAFE_REDIRECT;
  }

  bool strip_post_specific_headers = false;
  if (http_status_code != 307) {
    // NOTE: Even though RFC 2616 says to preserve the request method when
    // following a 302 redirect, normal browsers don't do that.  Instead, they
    // all convert a POST into a GET in response to a 302 and so shall we.  For
    // 307 redirects, browsers preserve the method.  The RFC says to prompt the
    // user to confirm the generation of a new POST request, but IE omits this
    // prompt and so shall we.
    strip_post_specific_headers = method_ == "POST";
    method_ = "GET";
  }
  url_ = location;
  upload_ = 0;
  status_ = URLRequestStatus();
  --redirect_limit_;

  if (strip_post_specific_headers) {
    // If being switched from POST to GET, must remove headers that were
    // specific to the POST and don't have meaning in GET. For example
    // the inclusion of a multipart Content-Type header in GET can cause
    // problems with some servers:
    // http://code.google.com/p/chromium/issues/detail?id=843
    //
    // TODO(eroman): It would be better if this data was structured into
    // specific fields/flags, rather than a stew of extra headers.
    extra_request_headers_ = StripPostSpecificHeaders(extra_request_headers_);
  }

  if (!final_upload_progress_) {
    final_upload_progress_ = job_->GetUploadProgress();
  }

  OrphanJob();

  is_pending_ = false;
  Start();
  return net::OK;
}

int64 URLRequest::GetExpectedContentSize() const {
  int64 expected_content_size = -1;
  if (job_)
    expected_content_size = job_->expected_content_size();

  return expected_content_size;
}
