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

#ifndef WEBKIT_GLUE_WEBURLREQUEST_IMPL_H__
#define WEBKIT_GLUE_WEBURLREQUEST_IMPL_H__

#include "webkit/glue/weburlrequest.h"
#include "base/basictypes.h"

#pragma warning(push, 0)
#include "FrameLoadRequest.h"
#include "HistoryItem.h"
#pragma warning(pop)

class WebRequestImpl : public WebRequest {
 public:
  WebRequestImpl();
  
  explicit WebRequestImpl(const GURL& url);
  explicit WebRequestImpl(const WebCore::ResourceRequest& request);
  explicit WebRequestImpl(const WebCore::FrameLoadRequest& request);

  // WebRequest
  virtual WebRequest* Clone() const;
  virtual void SetExtraData(ExtraData* extra);
  virtual ExtraData* GetExtraData() const;
  virtual void SetURL(const GURL& url);
  virtual GURL GetURL() const;
  virtual void SetMainDocumentURL(const GURL& url);
  virtual GURL GetMainDocumentURL() const;
  virtual WebRequestCachePolicy GetCachePolicy() const;
  virtual void SetCachePolicy(WebRequestCachePolicy policy);
  virtual std::wstring GetHttpMethod() const;
  virtual void SetHttpMethod(const std::wstring& method);
  virtual std::wstring GetHttpHeaderValue(const std::wstring& field) const;
  virtual std::wstring GetHttpReferrer() const;
  virtual std::string GetHistoryState() const;
  virtual void SetHistoryState(const std::string& value);
  virtual std::string GetSecurityInfo() const;
  virtual void SetSecurityInfo(const std::string& value);
  virtual bool HasFormData() const;

  // WebRequestImpl
  const WebCore::FrameLoadRequest& frame_load_request() const {
    return request_;
  }
  void set_frame_load_request(const WebCore::FrameLoadRequest& request) {
    request_ = request;
  }

  PassRefPtr<WebCore::HistoryItem> history_item() const {
    return history_item_;
  }

 protected:
  WebCore::FrameLoadRequest request_;
  RefPtr<WebCore::HistoryItem> history_item_;
  scoped_refptr<ExtraData> extra_data_;
};

#endif  // #ifndef WEBKIT_GLUE_WEBURLREQUEST_IMPL_H__
