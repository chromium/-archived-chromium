// Copyright (c) 2008 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "net/proxy/proxy_script_fetcher.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/string_util.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

// TODO(eroman):
//   - Support auth-prompts.

namespace net {

namespace {

// The maximum size (in bytes) allowed for a PAC script. Responses exceeding
// this will fail with ERR_FILE_TOO_BIG.
int max_response_bytes = 1048576;  // 1 megabyte

// The maximum duration (in milliseconds) allowed for fetching the PAC script.
// Responses exceeding this will fail with ERR_TIMED_OUT.
int max_duration_ms = 300000;  // 5 minutes

// Returns true if |mime_type| is one of the known PAC mime type.
bool IsPacMimeType(const std::string& mime_type) {
  static const char * const kSupportedPacMimeTypes[] = {
    "application/x-ns-proxy-autoconfig",
    "application/x-javascript-config",
  };
  for (size_t i = 0; i < arraysize(kSupportedPacMimeTypes); ++i) {
    if (LowerCaseEqualsASCII(mime_type, kSupportedPacMimeTypes[i]))
      return true;
  }
  return false;
}

}  // namespace

class ProxyScriptFetcherImpl : public ProxyScriptFetcher,
                               public URLRequest::Delegate {
 public:
  // Creates a ProxyScriptFetcher that issues requests through
  // |url_request_context|. |url_request_context| must remain valid for the
  // lifetime of ProxyScriptFetcherImpl.
  explicit ProxyScriptFetcherImpl(URLRequestContext* url_request_context);

  virtual ~ProxyScriptFetcherImpl();

  // ProxyScriptFetcher methods:

  virtual void Fetch(const GURL& url, std::string* bytes,
                     CompletionCallback* callback);
  virtual void Cancel();

  // URLRequest::Delegate methods:

  virtual void OnAuthRequired(URLRequest* request,
                              AuthChallengeInfo* auth_info);
  virtual void OnSSLCertificateError(URLRequest* request, int cert_error,
                                     X509Certificate* cert);
  virtual void OnReceivedRedirect(URLRequest* request, const GURL& to_url);
  virtual void OnResponseStarted(URLRequest* request);
  virtual void OnReadCompleted(URLRequest* request, int num_bytes);
  virtual void OnResponseCompleted(URLRequest* request);

 private:
  // Read more bytes from the response.
  void ReadBody(URLRequest* request);

  // Called once the request has completed to notify the caller of
  // |response_code_| and |response_bytes_|.
  void FetchCompleted();

  // Clear out the state for the current request.
  void ResetCurRequestState();

  // Callback for time-out task of request with id |id|.
  void OnTimeout(int id);

  // Factory for creating the time-out task. This takes care of revoking
  // outstanding tasks when |this| is deleted.
  ScopedRunnableMethodFactory<ProxyScriptFetcherImpl> task_factory_;

  // The context used for making network requests.
  URLRequestContext* url_request_context_;

  // Buffer that URLRequest writes into.
  enum { kBufSize = 4096 };
  scoped_refptr<net::IOBuffer> buf_;

  // The next ID to use for |cur_request_| (monotonically increasing).
  int next_id_;

  // The current (in progress) request, or NULL.
  scoped_ptr<URLRequest> cur_request_;

  // State for current request (only valid when |cur_request_| is not NULL):

  // Unique ID for the current request.
  int cur_request_id_;

  // Callback to invoke on completion of the fetch.
  CompletionCallback* callback_;

  // Holds the error condition that was hit on the current request, or OK.
  int result_code_;

  // Holds the bytes read so far. Will not exceed |max_response_bytes|. This
  // buffer is owned by the owner of |callback|.
  std::string* result_bytes_;
};

ProxyScriptFetcherImpl::ProxyScriptFetcherImpl(
    URLRequestContext* url_request_context)
    : ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)),
      url_request_context_(url_request_context),
      buf_(new net::IOBuffer(kBufSize)),
      next_id_(0),
      cur_request_(NULL),
      cur_request_id_(0),
      callback_(NULL),
      result_code_(OK),
      result_bytes_(NULL) {
  DCHECK(url_request_context);
}

ProxyScriptFetcherImpl::~ProxyScriptFetcherImpl() {
  // The URLRequest's destructor will cancel the outstanding request, and
  // ensure that the delegate (this) is not called again.
}

void ProxyScriptFetcherImpl::Fetch(const GURL& url,
                                   std::string* bytes,
                                   CompletionCallback* callback) {
  // It is invalid to call Fetch() while a request is already in progress.
  DCHECK(!cur_request_.get());

  DCHECK(callback);
  DCHECK(bytes);

  cur_request_.reset(new URLRequest(url, this));
  cur_request_->set_context(url_request_context_);
  cur_request_->set_method("GET");

  // Make sure that the PAC script is downloaded using a direct connection,
  // to avoid circular dependencies (fetching is a part of proxy resolution).
  // Also disable the use of the disk cache. The cache is disabled so that if
  // the user switches networks we don't potentially use the cached response
  // from old network when we should in fact be re-fetching on the new network.
  cur_request_->set_load_flags(LOAD_BYPASS_PROXY | LOAD_DISABLE_CACHE);

  // Save the caller's info for notification on completion.
  callback_ = callback;
  result_bytes_ = bytes;
  result_bytes_->clear();

  // Post a task to timeout this request if it takes too long.
  cur_request_id_ = ++next_id_;
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      task_factory_.NewRunnableMethod(&ProxyScriptFetcherImpl::OnTimeout,
                                      cur_request_id_),
      static_cast<int>(max_duration_ms));

  // Start the request.
  cur_request_->Start();
}

void ProxyScriptFetcherImpl::Cancel() {
  // ResetCurRequestState will free the URLRequest, which will cause
  // cancellation.
  ResetCurRequestState();
}

void ProxyScriptFetcherImpl::OnAuthRequired(URLRequest* request,
                                            AuthChallengeInfo* auth_info) {
  DCHECK(request == cur_request_.get());
  // TODO(eroman):
  LOG(WARNING) << "Auth required to fetch PAC script, aborting.";
  result_code_ = ERR_NOT_IMPLEMENTED;
  request->CancelAuth();
}

void ProxyScriptFetcherImpl::OnSSLCertificateError(URLRequest* request,
                                                   int cert_error,
                                                   X509Certificate* cert) {
  DCHECK(request == cur_request_.get());
  LOG(WARNING) << "SSL certificate error when fetching PAC script, aborting.";
  // Certificate errors are in same space as net errors.
  result_code_ = cert_error;
  request->Cancel();
}

void ProxyScriptFetcherImpl::OnReceivedRedirect(URLRequest* request,
                                                const GURL& to_url) {
  DCHECK(request == cur_request_.get());
  // OK, thanks for telling.
}

void ProxyScriptFetcherImpl::OnResponseStarted(URLRequest* request) {
  DCHECK(request == cur_request_.get());

  if (!request->status().is_success()) {
    OnResponseCompleted(request);
    return;
  }

  // Require HTTP responses to have a success status code.
  if (request->url().SchemeIs("http") || request->url().SchemeIs("https")) {
    // NOTE about status codes: We are like Firefox 3 in this respect.
    // {IE 7, Safari 3, Opera 9.5} do not care about the status code.
    if (request->GetResponseCode() != 200) {
      LOG(INFO) << "Fetched PAC script had (bad) status line: "
                << request->response_headers()->GetStatusLine();
      result_code_ = ERR_PAC_STATUS_NOT_OK;
      request->Cancel();
      return;
    }

    // NOTE about mime types: We do not enforce mime types on PAC files.
    // This is for compatibility with {IE 7, Firefox 3, Opera 9.5}. We will
    // however log mismatches to help with debugging.
    std::string mime_type;
    cur_request_->GetMimeType(&mime_type);
    if (!IsPacMimeType(mime_type)) {
      LOG(INFO) << "Fetched PAC script does not have a proper mime type: "
                << mime_type;
    }
  }

  ReadBody(request);
}

void ProxyScriptFetcherImpl::OnReadCompleted(URLRequest* request,
                                             int num_bytes) {
  DCHECK(request == cur_request_.get());
  if (num_bytes > 0) {
    // Enforce maximum size bound.
    if (num_bytes + result_bytes_->size() >
        static_cast<size_t>(max_response_bytes)) {
      result_code_ = ERR_FILE_TOO_BIG;
      request->Cancel();
      return;
    }
    result_bytes_->append(buf_->data(), num_bytes);
    ReadBody(request);
  } else {  // Error while reading, or EOF
    OnResponseCompleted(request);
  }
}

void ProxyScriptFetcherImpl::OnResponseCompleted(URLRequest* request) {
  DCHECK(request == cur_request_.get());

  // Use |result_code_| as the request's error if we have already set it to
  // something specific.
  if (result_code_ == OK && !request->status().is_success())
    result_code_ = request->status().os_error();

  FetchCompleted();
}

void ProxyScriptFetcherImpl::ReadBody(URLRequest* request) {
  int num_bytes;
  if (request->Read(buf_, kBufSize, &num_bytes)) {
    OnReadCompleted(request, num_bytes);
  } else if (!request->status().is_io_pending()) {
    // Read failed synchronously.
    OnResponseCompleted(request);
  }
}

void ProxyScriptFetcherImpl::FetchCompleted() {
  // On error, the caller expects empty string for bytes.
  if (result_code_ != OK)
    result_bytes_->clear();

  int result_code = result_code_;
  CompletionCallback* callback = callback_;

  ResetCurRequestState();

  callback->Run(result_code);
}

void ProxyScriptFetcherImpl::ResetCurRequestState() {
  cur_request_.reset();
  cur_request_id_ = 0;
  callback_ = NULL;
  result_code_ = OK;
  result_bytes_ = NULL;
}

void ProxyScriptFetcherImpl::OnTimeout(int id) {
  // Timeout tasks may outlive the URLRequest they reference. Make sure it
  // is still applicable.
  if (cur_request_id_ != id)
    return;

  DCHECK(cur_request_.get());
  result_code_ = ERR_TIMED_OUT;
  cur_request_->Cancel();
}

// static
ProxyScriptFetcher* ProxyScriptFetcher::Create(
    URLRequestContext* url_request_context) {
  return new ProxyScriptFetcherImpl(url_request_context);
}

// static
int ProxyScriptFetcher::SetTimeoutConstraintForUnittest(
    int timeout_ms) {
  int prev = max_duration_ms;
  max_duration_ms = timeout_ms;
  return prev;
}

// static
size_t ProxyScriptFetcher::SetSizeConstraintForUnittest(size_t size_bytes) {
  size_t prev = max_response_bytes;
  max_response_bytes = size_bytes;
  return prev;
}

}  // namespace net
