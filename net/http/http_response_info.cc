// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_response_info.h"

#include "net/base/ssl_cert_request_info.h"
#include "net/http/http_response_headers.h"

namespace net {

HttpResponseInfo::HttpResponseInfo()
    : was_cached(false) {
}

HttpResponseInfo::~HttpResponseInfo() {
}

}  // namespace net
