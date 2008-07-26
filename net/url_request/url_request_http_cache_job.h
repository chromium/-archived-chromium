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

#ifndef NET_URL_REQUEST_URL_REQUEST_HTTP_CACHE_JOB_H__
#define NET_URL_REQUEST_URL_REQUEST_HTTP_CACHE_JOB_H__

#include "net/base/completion_callback.h"
#include "net/http/http_request_info.h"
#include "net/url_request/url_request_job.h"

namespace net {
class HttpResponseInfo;
class HttpTransaction;
}
class URLRequestContext;

// A URLRequestJob subclass that is built on top of the HttpCache.  It provides
// an implementation for both HTTP and HTTPS.
class URLRequestHttpCacheJob : public URLRequestJob {
 public:
  static URLRequestJob* Factory(URLRequest* request, const std::string& scheme);

  virtual ~URLRequestHttpCacheJob();

 protected:
  URLRequestHttpCacheJob(URLRequest* request);

  // URLRequestJob methods:
  virtual void SetUpload(net::UploadData* upload);
  virtual void SetExtraRequestHeaders(const std::string& headers);
  virtual void Start();
  virtual void Kill();
  virtual net::LoadState GetLoadState() const;
  virtual uint64 GetUploadProgress() const;
  virtual bool GetMimeType(std::string* mime_type);
  virtual bool GetCharset(std::string* charset);
  virtual void GetResponseInfo(net::HttpResponseInfo* info);
  virtual bool GetResponseCookies(std::vector<std::string>* cookies);
  virtual int GetResponseCode();
  virtual bool GetContentEncoding(std::string* encoding_type);
  virtual bool IsRedirectResponse(GURL* location, int* http_status_code);
  virtual bool IsSafeRedirect(const GURL& location);
  virtual bool NeedsAuth();
  virtual void GetAuthChallengeInfo(scoped_refptr<AuthChallengeInfo>*);
  virtual void GetCachedAuthData(const AuthChallengeInfo& auth_info,
                                 scoped_refptr<AuthData>* auth_data);
  virtual void SetAuth(const std::wstring& username,
                       const std::wstring& password);
  virtual void CancelAuth();
  virtual void ContinueDespiteLastError();
  virtual bool GetMoreData();
  virtual bool ReadRawData(char* buf, int buf_size, int *bytes_read);

  // Shadows URLRequestJob's version of this method so we can grab cookies.
  void NotifyHeadersComplete();

  void DestroyTransaction();
  void StartTransaction();
  void AddExtraHeaders();
  void FetchResponseCookies();

  void OnStartCompleted(int result);
  void OnReadCompleted(int result);

  net::HttpRequestInfo request_info_;
  net::HttpTransaction* transaction_;
  const net::HttpResponseInfo* response_info_;
  std::vector<std::string> response_cookies_;

  // Auth states for proxy and origin server.
  AuthState proxy_auth_state_;
  AuthState server_auth_state_;

  net::CompletionCallbackImpl<URLRequestHttpCacheJob> start_callback_;
  net::CompletionCallbackImpl<URLRequestHttpCacheJob> read_callback_;

  bool read_in_progress_;

  // Keep a reference to the url request context to be sure it's not
  // deleted before us.
  scoped_refptr<URLRequestContext> context_;

  DISALLOW_EVIL_CONSTRUCTORS(URLRequestHttpCacheJob);
};

#endif  // NET_URL_REQUEST_URL_REQUEST_HTTP_CACHE_JOB_H__
