// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBURLREQUEST_IMPL_H__
#define WEBKIT_GLUE_WEBURLREQUEST_IMPL_H__

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "webkit/glue/weburlrequest.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "FrameLoadRequest.h"
#include "HistoryItem.h"
MSVC_POP_WARNING();

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

