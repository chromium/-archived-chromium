// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_context.h"

// These headers are used implicitly by the destructor when destroying the
// scoped_[ref]ptr.

#include "net/proxy/proxy_service.h"
#include "net/base/cookie_monster.h"

URLRequestContext::URLRequestContext() : is_off_the_record_(false) {}
URLRequestContext::~URLRequestContext() {}
