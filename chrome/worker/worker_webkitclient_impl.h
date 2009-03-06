// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_WORKER_WORKER_WEBKIT_CLIENT_IMPL_H_
#define CHROME_WORKER_WORKER_WEBKIT_CLIENT_IMPL_H_

#include "webkit/glue/webkitclient_impl.h"

class WorkerWebKitClientImpl : public webkit_glue::WebKitClientImpl {
 public:
  // WebKitClient methods:
  virtual WebKit::WebMimeRegistry* mimeRegistry();
  virtual uint64_t visitedLinkHash(const char* canonicalURL, size_t length);
  virtual bool isLinkVisited(uint64_t linkHash);
  virtual void setCookies(const WebKit::WebURL& url,
                          const WebKit::WebURL& policy_url,
                          const WebKit::WebString& value);
  virtual WebKit::WebString cookies(const WebKit::WebURL& url,
                                    const WebKit::WebURL& policy_url);
  virtual void prefetchHostName(const WebKit::WebString&);
  virtual WebKit::WebString defaultLocale();
};

#endif  // CHROME_WORKER_WORKER_WEBKIT_CLIENT_IMPL_H_
