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

#include "net/url_request/url_request_http_job.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/base/cookie_monster.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_error_job.h"

// TODO(darin): make sure the port blocking code is not lost

#pragma warning(disable: 4355)

// static
URLRequestJob* URLRequestHttpJob::Factory(URLRequest* request,
                                          const std::string& scheme) {
  DCHECK(scheme == "http" || scheme == "https");

  if (!net::IsPortAllowedByDefault(request->url().IntPort()))
    return new URLRequestErrorJob(request, net::ERR_UNSAFE_PORT);

  if (!request->context() ||
      !request->context()->http_transaction_factory()) {
    NOTREACHED() << "requires a valid context";
    return new URLRequestErrorJob(request, net::ERR_INVALID_ARGUMENT);
  }

  return new URLRequestHttpJob(request);
}

URLRequestHttpJob::URLRequestHttpJob(URLRequest* request)
    : URLRequestJob(request),
      transaction_(NULL),
      response_info_(NULL),
      proxy_auth_state_(net::AUTH_STATE_DONT_NEED_AUTH),
      server_auth_state_(net::AUTH_STATE_DONT_NEED_AUTH),
      start_callback_(this, &URLRequestHttpJob::OnStartCompleted),
      read_callback_(this, &URLRequestHttpJob::OnReadCompleted),
      read_in_progress_(false),
      context_(request->context()) {
}

URLRequestHttpJob::~URLRequestHttpJob() {
  if (transaction_)
    DestroyTransaction();
}

void URLRequestHttpJob::SetUpload(net::UploadData* upload) {
  DCHECK(!transaction_) << "cannot change once started";
  request_info_.upload_data = upload;
}

void URLRequestHttpJob::SetExtraRequestHeaders(
    const std::string& headers) {
  DCHECK(!transaction_) << "cannot change once started";
  request_info_.extra_headers = headers;
}

void URLRequestHttpJob::Start() {
  DCHECK(!transaction_);

  // TODO(darin): URLRequest::referrer() should return a GURL
  GURL referrer(request_->referrer());

  // Ensure that we do not send username and password fields in the referrer.
  if (referrer.has_username() || referrer.has_password()) {
    GURL::Replacements referrer_mods;
    referrer_mods.ClearUsername();
    referrer_mods.ClearPassword();
    referrer = referrer.ReplaceComponents(referrer_mods);
  }

  request_info_.url = request_->url();
  request_info_.referrer = referrer;
  request_info_.method = request_->method();
  request_info_.load_flags = request_->load_flags();

  if (request_->context())
    request_info_.user_agent = request_->context()->user_agent();

  AddExtraHeaders();

  StartTransaction();
}

void URLRequestHttpJob::Kill() {
  if (!transaction_)
    return;

  DestroyTransaction();
  URLRequestJob::Kill();
}

net::LoadState URLRequestHttpJob::GetLoadState() const {
  return transaction_ ? transaction_->GetLoadState() : net::LOAD_STATE_IDLE;
}

uint64 URLRequestHttpJob::GetUploadProgress() const {
  return transaction_ ? transaction_->GetUploadProgress() : 0;
}

bool URLRequestHttpJob::GetMimeType(std::string* mime_type) {
  DCHECK(transaction_);

  if (!response_info_)
    return false;

  return response_info_->headers->GetMimeType(mime_type);
}

bool URLRequestHttpJob::GetCharset(std::string* charset) {
  DCHECK(transaction_);

  if (!response_info_)
    return false;

  return response_info_->headers->GetCharset(charset);
}

void URLRequestHttpJob::GetResponseInfo(net::HttpResponseInfo* info) {
  DCHECK(request_);
  DCHECK(transaction_);

  if (response_info_)
    *info = *response_info_;
}

bool URLRequestHttpJob::GetResponseCookies(
    std::vector<std::string>* cookies) {
  DCHECK(transaction_);

  if (!response_info_)
    return false;

  if (response_cookies_.empty())
    FetchResponseCookies();

  cookies->clear();
  cookies->swap(response_cookies_);
  return true;
}

int URLRequestHttpJob::GetResponseCode() {
  DCHECK(transaction_);

  if (!response_info_)
    return -1;

  return response_info_->headers->response_code();
}

bool URLRequestHttpJob::GetContentEncoding(std::string* encoding_type) {
  DCHECK(transaction_);

  if (!response_info_)
    return false;

  // TODO(darin): what if there are multiple content encodings?
  return response_info_->headers->EnumerateHeader(NULL, "Content-Encoding",
                                                  encoding_type);
}

bool URLRequestHttpJob::IsRedirectResponse(GURL* location,
                                           int* http_status_code) {
  if (!response_info_)
    return false;

  std::string value;
  if (!response_info_->headers->IsRedirect(&value))
    return false;

  *location = request_->url().Resolve(value);
  *http_status_code = response_info_->headers->response_code();
  return true;
}

bool URLRequestHttpJob::IsSafeRedirect(const GURL& location) {
  // We only allow redirects to certain "safe" protocols.  This does not
  // restrict redirects to externally handled protocols.  Our consumer would
  // need to take care of those.

  if (!URLRequest::IsHandledURL(location))
    return true;

  static const char* kSafeSchemes[] = {
    "http",
    "https",
    "ftp"
  };

  for (size_t i = 0; i < arraysize(kSafeSchemes); ++i) {
    if (location.SchemeIs(kSafeSchemes[i]))
      return true;
  }

  return false;
}

bool URLRequestHttpJob::NeedsAuth() {
  int code = GetResponseCode();
  if (code == -1)
    return false;

  // Check if we need either Proxy or WWW Authentication.  This could happen
  // because we either provided no auth info, or provided incorrect info.
  switch (code) {
    case 407:
      if (proxy_auth_state_ == net::AUTH_STATE_CANCELED)
        return false;
      proxy_auth_state_ = net::AUTH_STATE_NEED_AUTH;
      return true;
    case 401:
      if (server_auth_state_ == net::AUTH_STATE_CANCELED)
        return false;
      server_auth_state_ = net::AUTH_STATE_NEED_AUTH;
      return true;
  }
  return false;
}

void URLRequestHttpJob::GetAuthChallengeInfo(
    scoped_refptr<net::AuthChallengeInfo>* result) {
  DCHECK(transaction_);
  DCHECK(response_info_);

  // sanity checks:
  DCHECK(proxy_auth_state_ == net::AUTH_STATE_NEED_AUTH ||
         server_auth_state_ == net::AUTH_STATE_NEED_AUTH);
  DCHECK(response_info_->headers->response_code() == 401 ||
         response_info_->headers->response_code() == 407);

  *result = response_info_->auth_challenge;
}

void URLRequestHttpJob::GetCachedAuthData(
    const net::AuthChallengeInfo& auth_info,
    scoped_refptr<net::AuthData>* auth_data) {
  net::AuthCache* auth_cache =
      request_->context()->http_transaction_factory()->GetAuthCache();
  if (!auth_cache) {
    *auth_data = NULL;
    return;
  }
  std::string auth_cache_key =
      net::AuthCache::HttpKey(request_->url(), auth_info);
  *auth_data = auth_cache->Lookup(auth_cache_key);
}

void URLRequestHttpJob::SetAuth(const std::wstring& username,
                                const std::wstring& password) {
  DCHECK(transaction_);

  // Proxy gets set first, then WWW.
  if (proxy_auth_state_ == net::AUTH_STATE_NEED_AUTH) {
    proxy_auth_state_ = net::AUTH_STATE_HAVE_AUTH;
  } else {
    DCHECK(server_auth_state_ == net::AUTH_STATE_NEED_AUTH);
    server_auth_state_ = net::AUTH_STATE_HAVE_AUTH;
  }

  // These will be reset in OnStartCompleted.
  response_info_ = NULL;
  response_cookies_.clear();

  // No matter what, we want to report our status as IO pending since we will
  // be notifying our consumer asynchronously via OnStartCompleted.
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));

  int rv = transaction_->RestartWithAuth(username, password,
                                         &start_callback_);
  if (rv == net::ERR_IO_PENDING)
    return;

  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestHttpJob::OnStartCompleted, rv));
}

void URLRequestHttpJob::CancelAuth() {
  // Proxy gets set first, then WWW.
  if (proxy_auth_state_ == net::AUTH_STATE_NEED_AUTH) {
    proxy_auth_state_ = net::AUTH_STATE_CANCELED;
  } else {
    DCHECK(server_auth_state_ == net::AUTH_STATE_NEED_AUTH);
    server_auth_state_ = net::AUTH_STATE_CANCELED;
  }

  // These will be reset in OnStartCompleted.
  response_info_ = NULL;
  response_cookies_.clear();

  // OK, let the consumer read the error page...
  //
  // Because we set the AUTH_STATE_CANCELED flag, NeedsAuth will return false,
  // which will cause the consumer to receive OnResponseStarted instead of
  // OnAuthRequired.
  //
  // We have to do this via InvokeLater to avoid "recursing" the consumer.
  //
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestHttpJob::OnStartCompleted, net::OK));
}

void URLRequestHttpJob::ContinueDespiteLastError() {
  DCHECK(transaction_);
  DCHECK(!response_info_) << "should not have a response yet";

  // No matter what, we want to report our status as IO pending since we will
  // be notifying our consumer asynchronously via OnStartCompleted.
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));

  int rv = transaction_->RestartIgnoringLastError(&start_callback_);
  if (rv == net::ERR_IO_PENDING)
    return;

  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestHttpJob::OnStartCompleted, rv));
}

bool URLRequestHttpJob::GetMoreData() {
  return transaction_ && !read_in_progress_;
}

bool URLRequestHttpJob::ReadRawData(char* buf, int buf_size, int *bytes_read) {
  DCHECK_NE(buf_size, 0);
  DCHECK(bytes_read);
  DCHECK(!read_in_progress_);

  int rv = transaction_->Read(buf, buf_size, &read_callback_);
  if (rv >= 0) {
    *bytes_read = rv;
    return true;
  }

  if (rv == net::ERR_IO_PENDING) {
    read_in_progress_ = true;
    SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));
  } else {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, rv));
  }

  return false;
}

void URLRequestHttpJob::OnStartCompleted(int result) {
  // If the request was destroyed, then there is no more work to do.
  if (!request_ || !request_->delegate())
    return;

  // If the transaction was destroyed, then the job was cancelled, and
  // we can just ignore this notification.
  if (!transaction_)
    return;

  // Clear the IO_PENDING status
  SetStatus(URLRequestStatus());

  if (result == net::OK) {
    NotifyHeadersComplete();
  } else if (net::IsCertificateError(result)) {
    // We encountered an SSL certificate error.  Ask our delegate to decide
    // what we should do.
    // TODO(wtc): also pass ssl_info.cert_status, or just pass the whole
    // ssl_info.
    request_->delegate()->OnSSLCertificateError(
        request_, result, transaction_->GetResponseInfo()->ssl_info.cert);
  } else {
    NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, result));
  }
}

void URLRequestHttpJob::OnReadCompleted(int result) {
  read_in_progress_ = false;

  if (result == 0) {
    NotifyDone(URLRequestStatus());
  } else if (result < 0) {
    NotifyDone(URLRequestStatus(URLRequestStatus::FAILED, result));
  } else {
    // Clear the IO_PENDING status
    SetStatus(URLRequestStatus());
  }

  NotifyReadComplete(result);
}

void URLRequestHttpJob::NotifyHeadersComplete() {
  DCHECK(!response_info_);

  response_info_ = transaction_->GetResponseInfo();

  // Get the Set-Cookie values, and send them to our cookie database.

  FetchResponseCookies();

  URLRequestContext* ctx = request_->context();
  if (ctx && ctx->cookie_store() &&
      ctx->cookie_policy()->CanSetCookie(request_->url(),
                                        request_->policy_url()))
    ctx->cookie_store()->SetCookies(request_->url(), response_cookies_);

  URLRequestJob::NotifyHeadersComplete();
}

void URLRequestHttpJob::DestroyTransaction() {
  DCHECK(transaction_);

  transaction_->Destroy();
  transaction_ = NULL;
  response_info_ = NULL;
}

void URLRequestHttpJob::StartTransaction() {
  // NOTE: This method assumes that request_info_ is already setup properly.

  // Create a transaction.
  DCHECK(!transaction_);

  DCHECK(request_->context());
  DCHECK(request_->context()->http_transaction_factory());

  transaction_ =
      request_->context()->http_transaction_factory()->CreateTransaction();

  // No matter what, we want to report our status as IO pending since we will
  // be notifying our consumer asynchronously via OnStartCompleted.
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));

  int rv;
  if (transaction_) {
    rv = transaction_->Start(&request_info_, &start_callback_);
    if (rv == net::ERR_IO_PENDING)
      return;
  } else {
    rv = net::ERR_FAILED;
  }

  // The transaction started synchronously, but we need to notify the
  // URLRequest delegate via the message loop.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
      this, &URLRequestHttpJob::OnStartCompleted, rv));
}

void URLRequestHttpJob::AddExtraHeaders() {
  URLRequestContext* context = request_->context();
  if (context) {
    // Add in the cookie header.  TODO might we need more than one header?
    if (context->cookie_store() &&
        context->cookie_policy()->CanGetCookies(request_->url(),
                                               request_->policy_url())) {
      std::string cookies = request_->context()->cookie_store()->
          GetCookiesWithOptions(request_->url(),
                                net::CookieMonster::INCLUDE_HTTPONLY);
      if (!cookies.empty())
        request_info_.extra_headers += "Cookie: " + cookies + "\r\n";
    }
    if (!context->accept_language().empty())
      request_info_.extra_headers += "Accept-Language: " +
          context->accept_language() + "\r\n";
    if (!context->accept_charset().empty())
      request_info_.extra_headers += "Accept-Charset: " +
          context->accept_charset() + "\r\n";
  }

#ifdef CHROME_LAST_MINUTE
  // Tell the server what compression formats we support.
  request_info_.extra_headers += "Accept-Encoding: gzip,deflate,bzip2\r\n";
#else
  // Tell the server that we support gzip/deflate encoding.
  request_info_.extra_headers += "Accept-Encoding: gzip,deflate";

  // const string point to google domain
  static const char kGoogleDomain[] = "google.com";
  static const unsigned int kGoogleDomainLen = arraysize(kGoogleDomain) - 1;
  static const char kLocalHostName[] = "localhost";

  // At now, only support bzip2 feature for those requests which are
  // sent to google domain or localhost.
  // TODO(jnd) : we will remove the "google.com" domain check before launch.
  // See bug : 861940
  const std::string &host = request_->url().host();

  if (host == kLocalHostName ||
      request_->url().DomainIs(kGoogleDomain, kGoogleDomainLen)) {
    request_info_.extra_headers += ",bzip2\r\n";
  } else {
    request_info_.extra_headers += "\r\n";
  }
#endif
}

void URLRequestHttpJob::FetchResponseCookies() {
  DCHECK(response_info_);
  DCHECK(response_cookies_.empty());

  std::string name = "Set-Cookie";
  std::string value;

  void* iter = NULL;
  while (response_info_->headers->EnumerateHeader(&iter, name, &value))
    response_cookies_.push_back(value);
}
