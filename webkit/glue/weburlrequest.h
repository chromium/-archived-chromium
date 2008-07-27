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

#ifndef WEBKIT_GLUE_WEBURLREQUEST_H__
#define WEBKIT_GLUE_WEBURLREQUEST_H__

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"

enum WebRequestCachePolicy {
  WebRequestUseProtocolCachePolicy,
  WebRequestReloadIgnoringCacheData,
  WebRequestReturnCacheDataElseLoad,
  WebRequestReturnCacheDataDontLoad
};

class GURL;

class WebRequest {
 public:
  // Extra information that is associated with a request. The embedder derives
  // from this REFERENCE COUNTED class to associated data with a request and
  // get it back when the page loads.
  //
  // Note that for reloads (and possibly things like back/forward), there is no
  // way to specify the request that it will use, so the extra data pointer
  // will be invalid. Users should always check for NULL.
  class ExtraData : public base::RefCounted<ExtraData> {
   public:
    virtual ~ExtraData() {}
  };

  // Creates a WebRequest.
  static WebRequest* Create(const GURL& url);

  // Creates a copy of this WebRequest.
  virtual WebRequest* Clone() const = 0;

  // Sets the extra request info that the embedder can retrieve later. This
  // will AddRef the ExtraData and store it with the request.
  virtual void SetExtraData(ExtraData* extra) = 0;

  // Returns any previously set request info. It does not AddRef, the caller
  // is assumed to assign this to a RefPtr. This may return NULL if no extra
  // data has been set on this request. Even if the embedder sets request info
  // for every request, WebRequests can get created during reload operations
  // so callers should not assume the data is always valid.
  virtual ExtraData* GetExtraData() const = 0;

  // Get/set the URL.
  virtual GURL GetURL() const = 0;
  virtual void SetURL(const GURL& url) = 0;

  // Get/set the main document URL, which may be different from the URL for a
  // subframe load.
  virtual GURL GetMainDocumentURL() const = 0;
  virtual void SetMainDocumentURL(const GURL& url) = 0;

  // Get/set the cache policy.
  virtual WebRequestCachePolicy GetCachePolicy() const = 0;
  virtual void SetCachePolicy(WebRequestCachePolicy policy) = 0;

  // Get/set the HTTP request method.
  virtual std::wstring GetHttpMethod() const = 0;
  virtual void SetHttpMethod(const std::wstring& method) = 0;

  // Returns the string corresponding to a header set in the request. If the
  // given header was not set in the request, the empty string is returned.
  virtual std::wstring GetHttpHeaderValue(const std::wstring& field) const = 0;

  // Helper function for GetHeaderValue to retrieve the referrer. This
  // referrer is generated automatically by WebKit when navigation events
  // occur. If there was no referrer (for example, the browser instructed
  // WebKit to navigate), the returned string will be empty.
  //
  // It is preferred to call this instead of GetHttpHeaderValue, because the
  // way referrers are stored may change in the future.
  //
  virtual std::wstring GetHttpReferrer() const = 0;

  // Get/set the opaque history state (used for back/forward navigations).
  virtual std::string GetHistoryState() const = 0;
  virtual void SetHistoryState(const std::string& state) = 0;

  // Get/set an opaque value containing the security info (including SSL
  // connection state) that should be reported as used in the response for that
  // request, or an empty string if no security info should be reported.  This
  // is usually used to simulate security errors on a page (typically an error
  // page that should contain the errors of the actual page that has the
  // errors).
  virtual std::string GetSecurityInfo() const = 0;
  virtual void SetSecurityInfo(const std::string& info) = 0;

  // Returns true if this request contains history state that has form data.
  virtual bool HasFormData() const = 0;

  virtual ~WebRequest() { }
};

#endif  // #ifndef WEBKIT_GLUE_WEBURLREQUEST_H__
