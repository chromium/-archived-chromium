// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/in_process_webkit/browser_webkitclient_impl.h"

#include "base/logging.h"
#include "webkit/api/public/WebData.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURL.h"

WebKit::WebClipboard* BrowserWebKitClientImpl::clipboard() {
  NOTREACHED();
  return NULL;
}

WebKit::WebMimeRegistry* BrowserWebKitClientImpl::mimeRegistry() {
  NOTREACHED();
  return NULL;
}

WebKit::WebSandboxSupport* BrowserWebKitClientImpl::sandboxSupport() {
  NOTREACHED();
  return NULL;
}

unsigned long long BrowserWebKitClientImpl::visitedLinkHash(
    const char* canonical_url,
    size_t length) {
  NOTREACHED();
  return 0;
}

bool BrowserWebKitClientImpl::isLinkVisited(unsigned long long link_hash) {
  NOTREACHED();
  return false;
}

void BrowserWebKitClientImpl::setCookies(const WebKit::WebURL& url,
                                         const WebKit::WebURL& policy_url,
                                         const WebKit::WebString& value) {
  NOTREACHED();
}

WebKit::WebString BrowserWebKitClientImpl::cookies(
    const WebKit::WebURL& url, const WebKit::WebURL& policy_url) {
  NOTREACHED();
  return WebKit::WebString();
}

void BrowserWebKitClientImpl::prefetchHostName(const WebKit::WebString&) {
  NOTREACHED();
}

WebKit::WebString BrowserWebKitClientImpl::defaultLocale() {
  NOTREACHED();
  return WebKit::WebString();
}

WebKit::WebThemeEngine* BrowserWebKitClientImpl::themeEngine() {
  NOTREACHED();
  return NULL;
}

WebKit::WebURLLoader* BrowserWebKitClientImpl::createURLLoader() {
  NOTREACHED();
  return NULL;
}

void BrowserWebKitClientImpl::getPluginList(bool refresh,
    WebKit::WebPluginListBuilder* builder) {
  NOTREACHED();
}

WebKit::WebData BrowserWebKitClientImpl::loadResource(const char* name) {
  NOTREACHED();
  return WebKit::WebData();
}
