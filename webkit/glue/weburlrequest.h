// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBURLREQUEST_H_
#define WEBKIT_GLUE_WEBURLREQUEST_H_

#include <map>
#include <string>

#include "base/ref_counted.h"

enum WebRequestCachePolicy {
  WebRequestUseProtocolCachePolicy,
  WebRequestReloadIgnoringCacheData,
  WebRequestReturnCacheDataElseLoad,
  WebRequestReturnCacheDataDontLoad
};

namespace net {
class UploadData;
}

class GURL;

class WebRequest {
 public:
  typedef std::map<std::string, std::string> HeaderMap;

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
  virtual std::string GetHttpMethod() const = 0;
  virtual void SetHttpMethod(const std::string& method) = 0;

  // Returns the string corresponding to a header set in the request. If the
  // given header was not set in the request, the empty string is returned.
  virtual std::string GetHttpHeaderValue(const std::string& field) const = 0;

  // Set a value for a header in the request.
  virtual void SetHttpHeaderValue(const std::string& field,
      const std::string& value) = 0;

  // Fills a map with all header name/value pairs set in the request.
  virtual void GetHttpHeaders(HeaderMap* headers) const = 0;

  // Sets the header name/value pairs for the request from a map. Values set
  // using this method replace any pre-existing values with the same name.
  // Passing in a blank value will result in a header with a blank value being
  // sent as part of the request.
  virtual void SetHttpHeaders(const HeaderMap& headers) = 0;

  // Helper function for GetHeaderValue to retrieve the referrer. This
  // referrer is generated automatically by WebKit when navigation events
  // occur. If there was no referrer (for example, the browser instructed
  // WebKit to navigate), the returned string will be empty.
  //
  // It is preferred to call this instead of GetHttpHeaderValue, because the
  // way referrers are stored may change in the future.
  //
  virtual std::string GetHttpReferrer() const = 0;

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

  // Returns true if the request has upload data.
  virtual bool HasUploadData() const = 0;

  // Returns the request upload data. This object is temporary and should be
  // deleted after use.
  virtual void GetUploadData(net::UploadData* data) const = 0;

  // Set the request upload data.
  virtual void SetUploadData(const net::UploadData& data) = 0;

  virtual ~WebRequest() { }
};

#endif  // #ifndef WEBKIT_GLUE_WEBURLREQUEST_H_
