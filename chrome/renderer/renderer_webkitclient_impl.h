// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_WEBKIT_CLIENT_IMPL_H_
#define CHROME_RENDERER_RENDERER_WEBKIT_CLIENT_IMPL_H_

#include "webkit/glue/simple_webmimeregistry_impl.h"
#include "webkit/glue/webclipboard_impl.h"
#include "webkit/glue/webkitclient_impl.h"

#if defined(OS_WIN)
#include "webkit/api/public/win/WebSandboxSupport.h"
#elif defined(OS_LINUX)
#include <string>
#include <map>
#include "base/lock.h"
#include "webkit/api/public/linux/WebSandboxSupport.h"
#endif

class RendererWebKitClientImpl : public webkit_glue::WebKitClientImpl {
 public:
  // WebKitClient methods:
  virtual WebKit::WebClipboard* clipboard();
  virtual WebKit::WebMimeRegistry* mimeRegistry();
  virtual WebKit::WebSandboxSupport* sandboxSupport();
  virtual unsigned long long visitedLinkHash(
      const char* canonicalURL, size_t length);
  virtual bool isLinkVisited(unsigned long long linkHash);
  virtual void setCookies(const WebKit::WebURL& url,
                          const WebKit::WebURL& first_party_for_cookies,
                          const WebKit::WebString&);
  virtual WebKit::WebString cookies(
      const WebKit::WebURL& url, const WebKit::WebURL& first_party_for_cookies);
  virtual void prefetchHostName(const WebKit::WebString&);
  virtual bool getFileSize(const WebKit::WebString& path, long long& result);
  virtual WebKit::WebString defaultLocale();
  virtual void suddenTerminationChanged(bool enabled);

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
#elif defined(OS_LINUX)
  class SandboxSupport : public WebKit::WebSandboxSupport {
   public:
    virtual WebKit::WebString getFontFamilyForCharacters(
        const WebKit::WebUChar* characters, size_t numCharacters);

   private:
    // WebKit likes to ask us for the correct font family to use for a set of
    // unicode code points. It needs this information frequently so we cache it
    // here. The key in this map is an array of 16-bit UTF16 values from WebKit.
    // The value is a string containing the correct font family.
    Lock unicode_font_families_mutex_;
    std::map<std::string, std::string> unicode_font_families_;
  };
#endif

  webkit_glue::WebClipboardImpl clipboard_;

  MimeRegistry mime_registry_;
#if defined(OS_WIN) || defined(OS_LINUX)
  SandboxSupport sandbox_support_;
#endif
};

#endif  // CHROME_RENDERER_WEBKIT_CLIENT_IMPL_H_
