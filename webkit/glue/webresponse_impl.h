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

#ifndef WEBKIT_GLUE_WEBRESPONSEIMPL_H__
#define WEBKIT_GLUE_WEBRESPONSEIMPL_H__

#include <string>

#include "googleurl/src/gurl.h"
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

  // Get the security info (state of the SSL connection).
  virtual std::string GetSecurityInfo() const {
    return webkit_glue::CStringToStdString(response_.getSecurityInfo());
  }

  void set_resource_response(const WebCore::ResourceResponse& response) {
    response_ = response;
  }

 private:
  WebCore::ResourceResponse response_;

  DISALLOW_EVIL_CONSTRUCTORS(WebResponseImpl);
};

#endif  // #ifndef WEBKIT_GLUE_WEBRESPONSEIMPL_H__
