// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_HTTP_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_HTTP_JOB_H_

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "net/base/auth.h"
#include "net/base/completion_callback.h"
#include "net/http/http_request_info.h"
#include "net/url_request/url_request_job.h"

namespace net {
class HttpResponseInfo;
class HttpTransaction;
}
class URLRequestContext;

// A URLRequestJob subclass that is built on top of HttpTransaction.  It
// provides an implementation for both HTTP and HTTPS.
class URLRequestHttpJob : public URLRequestJob {
 public:
  static URLRequestJob* Factory(URLRequest* request, const std::string& scheme);

  virtual ~URLRequestHttpJob();

 protected:
  explicit URLRequestHttpJob(URLRequest* request);

  // URLRequestJob methods:
  virtual void SetUpload(net::UploadData* upload);
  virtual void SetExtraRequestHeaders(const std::string& headers);
  virtual void Start();
  virtual void Kill();
  virtual net::LoadState GetLoadState() const;
  virtual uint64 GetUploadProgress() const;
  virtual bool GetMimeType(std::string* mime_type) const;
  virtual bool GetCharset(std::string* charset);
  virtual void GetResponseInfo(net::HttpResponseInfo* info);
  virtual bool GetResponseCookies(std::vector<std::string>* cookies);
  virtual int GetResponseCode() const;
  virtual bool GetContentEncodings(
      std::vector<Filter::FilterType>* encoding_type);
  virtual bool IsCachedContent() const { return is_cached_content_; }
  virtual bool IsSdchResponse() const;
  virtual bool IsRedirectResponse(GURL* location, int* http_status_code);
  virtual bool IsSafeRedirect(const GURL& location);
  virtual bool NeedsAuth();
  virtual void GetAuthChallengeInfo(scoped_refptr<net::AuthChallengeInfo>*);
  virtual void SetAuth(const std::wstring& username,
                       const std::wstring& password);
  virtual void CancelAuth();
  virtual void ContinueWithCertificate(net::X509Certificate* client_cert);
  virtual void ContinueDespiteLastError();
  virtual bool GetMoreData();
  virtual bool ReadRawData(net::IOBuffer* buf, int buf_size, int *bytes_read);

  // Shadows URLRequestJob's version of this method so we can grab cookies.
  void NotifyHeadersComplete();

  void DestroyTransaction();
  void StartTransaction();
  void AddExtraHeaders();
  std::string AssembleRequestCookies();
  void FetchResponseCookies();

  // Process the X-Force-TLS header, if one exists.
  void ProcessForceTLSHeader();

  void OnStartCompleted(int result);
  void OnReadCompleted(int result);

  bool ShouldTreatAsCertificateError(int result);

  void RestartTransactionWithAuth(const std::wstring& username,
                                  const std::wstring& password);

  // Keep a reference to the url request context to be sure it's not deleted
  // before us.
  scoped_refptr<URLRequestContext> context_;

  net::HttpRequestInfo request_info_;
  const net::HttpResponseInfo* response_info_;
  std::vector<std::string> response_cookies_;

  // Auth states for proxy and origin server.
  net::AuthState proxy_auth_state_;
  net::AuthState server_auth_state_;

  net::CompletionCallbackImpl<URLRequestHttpJob> start_callback_;
  net::CompletionCallbackImpl<URLRequestHttpJob> read_callback_;

  bool read_in_progress_;

  // An URL for an SDCH dictionary as suggested in a Get-Dictionary HTTP header.
  GURL sdch_dictionary_url_;

  scoped_ptr<net::HttpTransaction> transaction_;

  // Indicated if an SDCH dictionary was advertised, and hence an SDCH
  // compressed response is expected.  We use this to help detect (accidental?)
  // proxy corruption of a response, which sometimes marks SDCH content as
  // having no content encoding <oops>.
  bool sdch_dictionary_advertised_;

  // For SDCH latency experiments, when we are able to do SDCH, we may enable
  // either an SDCH latency test xor a pass through test.  The following bools
  // indicate what we decided on for this instance.
  bool sdch_test_activated_;  // Advertising a dictionary for sdch.
  bool sdch_test_control_;    // Not even accepting-content sdch.

  // For recording of stats, we need to remember if this is cached content.
  bool is_cached_content_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestHttpJob);
};

#endif  // NET_URL_REQUEST_URL_REQUEST_HTTP_JOB_H_
