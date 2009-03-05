// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBKIT_INIT_H_
#define WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBKIT_INIT_H_

#include "base/string_util.h"
#include "webkit/glue/simple_webmimeregistry_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webkitclient_impl.h"
#include "webkit/tools/test_shell/simple_resource_loader_bridge.h"

#include "WebKit.h"
#include "WebString.h"
#include "WebURL.h"

class TestShellWebKitInit : public webkit_glue::WebKitClientImpl {
 public:
  TestShellWebKitInit(bool layout_test_mode) {
    WebKit::initialize(this);
    WebKit::setLayoutTestMode(layout_test_mode);
    WebKit::registerURLSchemeAsLocal(
        ASCIIToUTF16(webkit_glue::GetUIResourceProtocol()));
  }

  ~TestShellWebKitInit() {
    WebKit::shutdown();
  }

  virtual WebKit::WebMimeRegistry* mimeRegistry() {
    return &mime_registry_;
  }

  virtual void setCookies(
      const WebKit::WebURL& url, const WebKit::WebURL& policy_url,
      const WebKit::WebString& value) {
    SimpleResourceLoaderBridge::SetCookie(url, policy_url, UTF16ToUTF8(value));
  }

  virtual WebKit::WebString cookies(
      const WebKit::WebURL& url, const WebKit::WebURL& policy_url) {
    return UTF8ToUTF16(SimpleResourceLoaderBridge::GetCookies(url, policy_url));
  }

  virtual void prefetchHostName(const WebKit::WebString&) {
  }

  virtual WebKit::WebString defaultLocale() {
    return ASCIIToUTF16("en-US");
  }

 private:
  webkit_glue::SimpleWebMimeRegistryImpl mime_registry_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBKIT_INIT_H_
