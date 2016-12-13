// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_NPOBJECT_PROXY_TEST_H__
#define WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_NPOBJECT_PROXY_TEST_H__

#include "webkit/glue/plugins/test/plugin_test.h"

namespace NPAPIClient {

// The NPObjectProxyTest tests that when we proxy an NPObject that is itself
// a proxy, we don't create a new proxy but instead just use the original
// pointer.

class NPObjectProxyTest : public PluginTest {
 public:
  // Constructor.
  NPObjectProxyTest(NPP id, NPNetscapeFuncs *host_functions);

  // NPAPI SetWindow handler.
  virtual NPError SetWindow(NPWindow* pNPWindow);
};

} // namespace NPAPIClient

#endif  // WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_NPOBJECT_PROXY_TEST_H__
