// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_IN_PROCESS_WEBKIT_WEBKIT_CLIENT_IMPL_H_
#define CHROME_BROWSER_IN_PROCESS_WEBKIT_WEBKIT_CLIENT_IMPL_H_

#include "webkit/glue/webkitclient_impl.h"

class BrowserWebKitClientImpl : public webkit_glue::WebKitClientImpl {
 public:
  // WebKitClient methods:
  virtual WebKit::WebClipboard* clipboard();
  virtual WebKit::WebMimeRegistry* mimeRegistry();
  virtual WebKit::WebSandboxSupport* sandboxSupport();
  virtual unsigned long long visitedLinkHash(const char* canonicalURL,
                                             size_t length);
  virtual bool isLinkVisited(unsigned long long linkHash);
  virtual void setCookies(const WebKit::WebURL& url,
                          const WebKit::WebURL& policy_url,
                          const WebKit::WebString& value);
  virtual WebKit::WebString cookies(const WebKit::WebURL& url,
                                    const WebKit::WebURL& policy_url);
  virtual void prefetchHostName(const WebKit::WebString&);
  virtual WebKit::WebString defaultLocale();
  virtual WebKit::WebThemeEngine* themeEngine();
  virtual WebKit::WebURLLoader* createURLLoader();
  virtual void getPluginList(bool refresh, WebKit::WebPluginListBuilder*);
  virtual WebKit::WebData loadResource(const char* name);
};

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_WEBKIT_CLIENT_IMPL_H_
