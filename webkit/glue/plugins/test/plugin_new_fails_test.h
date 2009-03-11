// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_PLUGIN_NEW_FAILS_TEST_H__
#define WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_PLUGIN_NEW_FAILS_TEST_H__

#include "webkit/glue/plugins/test/plugin_test.h"

namespace NPAPIClient {

class NewFailsTest : public PluginTest {
 public:
  NewFailsTest(NPP id, NPNetscapeFuncs *host_functions);
  virtual NPError New(uint16 mode, int16 argc, const char* argn[],
                      const char* argv[], NPSavedData* saved);
};

} // namespace NPAPIClient

#endif  // WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_PLUGIN_NPP_NEW_FAILS_TEST_H__
