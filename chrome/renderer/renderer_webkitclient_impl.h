// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_WEBKIT_CLIENT_IMPL_H_
#define CHROME_RENDERER_RENDERER_WEBKIT_CLIENT_IMPL_H_

#include "webkit/glue/simple_webmimeregistry_impl.h"
#include "webkit/glue/webkitclient_impl.h"

#if defined(OS_WIN)
#include "third_party/WebKit/WebKit/chromium/public/win/WebSandboxSupport.h"
#endif

class RendererWebKitClientImpl : public webkit_glue::WebKitClientImpl {
 public:
  // WebKitClient methods:
  virtual WebKit::WebMimeRegistry* mimeRegistry();
  virtual WebKit::WebSandboxSupport* sandboxSupport();
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

#if defined(OS_WIN)
  class SandboxSupport : public WebKit::WebSandboxSupport {
   public:
    virtual bool ensureFontLoaded(HFONT);
  };
#endif

  MimeRegistry mime_registry_;
#if defined(OS_WIN)
  SandboxSupport sandbox_support_;
#endif
};

#endif  // CHROME_RENDERER_WEBKIT_CLIENT_IMPL_H_
