// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_WEBKIT_CLIENT_IMPL_H_
#define CHROME_RENDERER_RENDERER_WEBKIT_CLIENT_IMPL_H_

#include "webkit/glue/simple_webmimeregistry_impl.h"
#include "webkit/glue/webkitclient_impl.h"

class RendererWebKitClientImpl : public webkit_glue::WebKitClientImpl {
 public:
  // WebKitClient methods:
  virtual WebKit::WebMimeRegistry* mimeRegistry() {
    return &mime_registry_;
  }
  virtual uint64_t visitedLinkHash(const char* canonicalURL, size_t length);
  virtual bool isLinkVisited(uint64_t linkHash);
  virtual void setCookies(
      const WebKit::WebURL& url, const WebKit::WebURL& policy_url,
      const WebKit::WebString&);
  virtual WebKit::WebString cookies(
      const WebKit::WebURL& url, const WebKit::WebURL& policy_url);
  virtual void prefetchHostName(const WebKit::WebString&);
  virtual WebKit::WebString defaultLocale();

 private:
  class MimeRegistry : public webkit_glue::SimpleWebMimeRegistryImpl {
   public:
    virtual WebKit::WebString mimeTypeForExtension(const WebKit::WebString&);
    virtual WebKit::WebString mimeTypeFromFile(const WebKit::WebString&);
    virtual WebKit::WebString preferredExtensionForMIMEType(
        const WebKit::WebString&);
  };

  MimeRegistry mime_registry_;
};

#endif  // CHROME_RENDERER_WEBKIT_CLIENT_IMPL_H_
