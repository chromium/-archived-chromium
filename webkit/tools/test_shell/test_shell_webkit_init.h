// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBKIT_INIT_H_
#define WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBKIT_INIT_H_

#include "webkit/glue/simple_webmimeregistry_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webkitclient_impl.h"

#include "WebKit.h"

class TestShellWebKitInit : public webkit_glue::WebKitClientImpl {
 public:
  TestShellWebKitInit(bool layout_test_mode) {
    WebKit::initialize(this);

    webkit_glue::InitializeForTesting();

    // TODO(darin): This should eventually be a property of WebKitClientImpl.
    webkit_glue::SetLayoutTestMode(layout_test_mode);
  }

  ~TestShellWebKitInit() {
    WebKit::shutdown();
  }

  virtual WebKit::WebMimeRegistry* mimeRegistry() {
    return &mime_registry_;
  }

 private:
  webkit_glue::SimpleWebMimeRegistryImpl mime_registry_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_TEST_SHELL_WEBKIT_INIT_H_
