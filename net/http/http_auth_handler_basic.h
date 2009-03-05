// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_AUTH_HANDLER_BASIC_H_
#define NET_HTTP_HTTP_AUTH_HANDLER_BASIC_H_

#include "net/http/http_auth_handler.h"

namespace net {

// Code for handling http basic authentication.
class HttpAuthHandlerBasic : public HttpAuthHandler {
 public:
  virtual std::string GenerateCredentials(const std::wstring& username,
                                          const std::wstring& password,
                                          const HttpRequestInfo*,
                                          const ProxyInfo*);
 protected:
  virtual bool Init(std::string::const_iterator challenge_begin,
                    std::string::const_iterator challenge_end);

};

}  // namespace net

#endif  // NET_HTTP_HTTP_AUTH_HANDLER_BASIC_H_
