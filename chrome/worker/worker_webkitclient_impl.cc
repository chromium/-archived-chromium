// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/worker/worker_webkitclient_impl.h"

#include "WebString.h"
#include "WebURL.h"

WebKit::WebMimeRegistry* WorkerWebKitClientImpl::mimeRegistry() {
  return NULL;
}

uint64_t WorkerWebKitClientImpl::visitedLinkHash(const char* canonical_url,
                                                 size_t length) {
  return 0;
}

bool WorkerWebKitClientImpl::isLinkVisited(uint64_t link_hash) {
  return false;
}

void WorkerWebKitClientImpl::setCookies(const WebKit::WebURL& url,
                                        const WebKit::WebURL& policy_url,
                                        const WebKit::WebString& value) {
}

WebKit::WebString WorkerWebKitClientImpl::cookies(
    const WebKit::WebURL& url, const WebKit::WebURL& policy_url) {
  return WebKit::WebString();
}

void WorkerWebKitClientImpl::prefetchHostName(const WebKit::WebString&) {
}

WebKit::WebString WorkerWebKitClientImpl::defaultLocale() {
  return WebKit::WebString();
}
