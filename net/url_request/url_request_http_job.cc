// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_http_job.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/base/cookie_monster.h"
#include "net/base/filter.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/sdch_manager.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"

// TODO(darin): make sure the port blocking code is not lost

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

  // We cache the value of the switch because this code path is hit on every
  // network request.
  static const bool kForceHTTPS =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kForceHTTPS);
  if (kForceHTTPS && scheme != "https")
    return new URLRequestErrorJob(request, net::ERR_DISALLOWED_URL_SCHEME);

  return new URLRequestHttpJob(request);
}

URLRequestHttpJob::URLRequestHttpJob(URLRequest* request)
    : URLRequestJob(request),
      transaction_(NULL),
      response_info_(NULL),
      proxy_auth_state_(net::AUTH_STATE_DONT_NEED_AUTH),
      server_auth_state_(net::AUTH_STATE_DONT_NEED_AUTH),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          start_callback_(this, &URLRequestHttpJob::OnStartCompleted)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &URLRequestHttpJob::OnReadCompleted)),
      read_in_progress_(false),
      context_(request->context()) {
}

URLRequestHttpJob::~URLRequestHttpJob() {
  if (sdch_dictionary_url_.is_valid()) {
    // Prior to reaching the destructor, request_ has been set to a NULL
    // pointer, so request_->url() is no longer valid in the destructor, and we
    // use an alternate copy |request_info_.url|.
    SdchManager* manager = SdchManager::Global();
    // To be extra safe, since this is a "different time" from when we decided
    // to get the dictionary, we'll validate that an SdchManager is available.
    // At shutdown time, care is taken to be sure that we don't delete this
    // globally useful instance "too soon," so this check is just defensive
    // coding to assure that IF the system is shutting down, we don't have any
    // problem if the manager was deleted ahead of time.
    if (manager)  // Defensive programming.
      manager->FetchDictionary(request_info_.url, sdch_dictionary_url_);
  }
}

void URLRequestHttpJob::SetUpload(net::UploadData* upload) {
  DCHECK(!transaction_.get()) << "cannot change once started";
  request_info_.upload_data = upload;
}

void URLRequestHttpJob::SetExtraRequestHeaders(
    const std::string& headers) {
  DCHECK(!transaction_.get()) << "cannot change once started";
  request_info_.extra_headers = headers;
}

void URLRequestHttpJob::Start() {
  DCHECK(!transaction_.get());

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

  if (request_->context()) {
    request_info_.user_agent =
        request_->context()->GetUserAgent(request_->url());
  }

  AddExtraHeaders();

  StartTransaction();
}

void URLRequestHttpJob::Kill() {
  if (!transaction_.get())
    return;

  DestroyTransaction();
  URLRequestJob::Kill();
}

net::LoadState URLRequestHttpJob::GetLoadState() const {
  return transaction_.get() ?
      transaction_->GetLoadState() : net::LOAD_STATE_IDLE;
}

uint64 URLRequestHttpJob::GetUploadProgress() const {
  return transaction_.get() ? transaction_->GetUploadProgress() : 0;
}

bool URLRequestHttpJob::GetMimeType(std::string* mime_type) const {
  DCHECK(transaction_.get());

  if (!response_info_)
    return false;

  return response_info_->headers->GetMimeType(mime_type);
}

bool URLRequestHttpJob::GetCharset(std::string* charset) {
  DCHECK(transaction_.get());

  if (!response_info_)
    return false;

  return response_info_->headers->GetCharset(charset);
}

void URLRequestHttpJob::GetResponseInfo(net::HttpResponseInfo* info) {
  DCHECK(request_);
  DCHECK(transaction_.get());

  if (response_info_)
    *info = *response_info_;
}

bool URLRequestHttpJob::GetResponseCookies(
    std::vector<std::string>* cookies) {
  DCHECK(transaction_.get());

  if (!response_info_)
    return false;

  if (response_cookies_.empty())
    FetchResponseCookies();

  cookies->clear();
  cookies->swap(response_cookies_);
  return true;
}

int URLRequestHttpJob::GetResponseCode() {
  DCHECK(transaction_.get());

  if (!response_info_)
    return -1;

  return response_info_->headers->response_code();
}

bool URLRequestHttpJob::GetContentEncodings(
    std::vector<Filter::FilterType>* encoding_types) {
  DCHECK(transaction_.get());
  if (!response_info_)
    return false;
  DCHECK(encoding_types->empty());

  std::string encoding_type;
  void* iter = NULL;
  while (response_info_->headers->EnumerateHeader(&iter, "Content-Encoding",
                                                  &encoding_type)) {
    encoding_types->push_back(Filter::ConvertEncodingToType(encoding_type));
  }

  if (!encoding_types->empty()) {
    std::string mime_type;
    GetMimeType(&mime_type);
    // TODO(jar): Need to change this call to use the FilterContext interfaces.
    Filter::FixupEncodingTypes(IsSdchResponse(), mime_type, encoding_types);
  }
  return !encoding_types->empty();
}

bool URLRequestHttpJob::IsSdchResponse() const {
  return response_info_ &&
      (request_info_.load_flags & net::LOAD_SDCH_DICTIONARY_ADVERTISED);
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
  DCHECK(transaction_.get());
  DCHECK(response_info_);

  // sanity checks:
  DCHECK(proxy_auth_state_ == net::AUTH_STATE_NEED_AUTH ||
         server_auth_state_ == net::AUTH_STATE_NEED_AUTH);
  DCHECK(response_info_->headers->response_code() == 401 ||
         response_info_->headers->response_code() == 407);

  *result = response_info_->auth_challenge;
}

void URLRequestHttpJob::SetAuth(const std::wstring& username,
                                const std::wstring& password) {
  DCHECK(transaction_.get());

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
  // If the transaction was destroyed, then the job was cancelled.
  if (!transaction_.get())
    return;

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
  return transaction_.get() && !read_in_progress_;
}

bool URLRequestHttpJob::ReadRawData(net::IOBuffer* buf, int buf_size,
                                    int *bytes_read) {
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
  if (!transaction_.get())
    return;

  // Clear the IO_PENDING status
  SetStatus(URLRequestStatus());

  if (result == net::OK) {
    NotifyHeadersComplete();
  } else if (net::IsCertificateError(result) &&
             !CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kForceHTTPS)) {
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
  if (!(request_info_.load_flags & net::LOAD_DO_NOT_SAVE_COOKIES)) {
    URLRequestContext* ctx = request_->context();
    if (ctx && ctx->cookie_store() &&
        ctx->cookie_policy()->CanSetCookie(request_->url(),
                                           request_->policy_url())) {
      FetchResponseCookies();
      net::CookieMonster::CookieOptions options;
      options.set_include_httponly();
      ctx->cookie_store()->SetCookiesWithOptions(request_->url(),
                                                 response_cookies_,
                                                 options);
    }
  }

  if (SdchManager::Global() &&
      SdchManager::Global()->IsInSupportedDomain(request_->url())) {
    static const std::string name = "Get-Dictionary";
    std::string url_text;
    void* iter = NULL;
    // TODO(jar): We need to not fetch dictionaries the first time they are
    // seen, but rather wait until we can justify their usefulness.
    // For now, we will only fetch the first dictionary, which will at least
    // require multiple suggestions before we get additional ones for this site.
    // Eventually we should wait until a dictionary is requested several times
    // before we even download it (so that we don't waste memory or bandwidth).
    if (response_info_->headers->EnumerateHeader(&iter, name, &url_text)) {
      // request_->url() won't be valid in the destructor, so we use an
      // alternate copy.
      DCHECK(request_->url() == request_info_.url);
      // Resolve suggested URL relative to request url.
      sdch_dictionary_url_ = request_info_.url.Resolve(url_text);
    }
  }

  URLRequestJob::NotifyHeadersComplete();
}

void URLRequestHttpJob::DestroyTransaction() {
  DCHECK(transaction_.get());

  transaction_.reset();
  response_info_ = NULL;
}

void URLRequestHttpJob::StartTransaction() {
  // NOTE: This method assumes that request_info_ is already setup properly.

  // Create a transaction.
  DCHECK(!transaction_.get());

  DCHECK(request_->context());
  DCHECK(request_->context()->http_transaction_factory());

  transaction_.reset(
      request_->context()->http_transaction_factory()->CreateTransaction());

  // No matter what, we want to report our status as IO pending since we will
  // be notifying our consumer asynchronously via OnStartCompleted.
  SetStatus(URLRequestStatus(URLRequestStatus::IO_PENDING, 0));

  int rv;
  if (transaction_.get()) {
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
  // Supply Accept-Encoding headers first so that it is more likely that they
  // will be in the first transmitted packet.  This can sometimes make it easier
  // to filter and analyze the streams to assure that a proxy has not damaged
  // these headers.  Some proxies deliberately corrupt Accept-Encoding headers.
  if (!SdchManager::Global() ||
      !SdchManager::Global()->IsInSupportedDomain(request_->url())) {
    // Tell the server what compression formats we support (other than SDCH).
    request_info_.extra_headers += "Accept-Encoding: gzip,deflate,bzip2\r\n";
  } else {
    // Supply SDCH related headers, as well as accepting that encoding.
    // Tell the server what compression formats we support.
    request_info_.extra_headers += "Accept-Encoding: "
        "gzip,deflate,bzip2,sdch\r\n";

    // TODO(jar): See if it is worth optimizing away these bytes when the URL is
    // probably an img or such. (and SDCH encoding is not likely).
    std::string avail_dictionaries;
    SdchManager::Global()->GetAvailDictionaryList(request_->url(),
                                                  &avail_dictionaries);
    if (!avail_dictionaries.empty()) {
      request_info_.extra_headers += "Avail-Dictionary: "
          + avail_dictionaries + "\r\n";
      request_info_.load_flags |= net::LOAD_SDCH_DICTIONARY_ADVERTISED;
    }
  }

  URLRequestContext* context = request_->context();
  if (context) {
    // Add in the cookie header.  TODO might we need more than one header?
    if (context->cookie_store() &&
        context->cookie_policy()->CanGetCookies(request_->url(),
                                               request_->policy_url())) {
      net::CookieMonster::CookieOptions options;
      options.set_include_httponly();
      std::string cookies = request_->context()->cookie_store()->
          GetCookiesWithOptions(request_->url(), options);
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
