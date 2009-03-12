// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_WEBRESPONSEIMPL_H_
#define WEBKIT_GLUE_WEBRESPONSEIMPL_H_

#include "webkit/glue/glue_util.h"
#include "webkit/glue/webresponse.h"

#include "ResourceResponse.h"

class WebResponseImpl : public WebResponse {
 public:
   WebResponseImpl() { }
   explicit WebResponseImpl(const WebCore::ResourceResponse& response)
      : response_(response) { }

  virtual ~WebResponseImpl() { }

  // Get the URL.
  virtual GURL GetURL() const {
    return webkit_glue::KURLToGURL(response_.url());
  }

  // Get the http status code.
  virtual int GetHttpStatusCode() const { return response_.httpStatusCode(); }

  virtual std::string GetMimeType() const {
    return webkit_glue::StringToStdString(response_.mimeType());
  }

  // Get the security info (state of the SSL connection).
  virtual std::string GetSecurityInfo() const {
    return webkit_glue::CStringToStdString(response_.getSecurityInfo());
  }

  void set_resource_response(const WebCore::ResourceResponse& response) {
    response_ = response;
  }

 virtual bool IsContentFiltered() const {
   return response_.isContentFiltered();
 }

 private:
  WebCore::ResourceResponse response_;

  DISALLOW_EVIL_CONSTRUCTORS(WebResponseImpl);
};

#endif  // #ifndef WEBKIT_GLUE_WEBRESPONSEIMPL_H_
