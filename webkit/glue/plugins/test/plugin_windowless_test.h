// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_EXECUTE_SCRIPT_DELETE_TEST_H
#define WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_EXECUTE_SCRIPT_DELETE_TEST_H

#include "webkit/glue/plugins/test/plugin_test.h"

namespace NPAPIClient {

// This class contains a list of windowless plugin tests. Please add additional
// tests to this class.
class WindowlessPluginTest : public PluginTest {
 public:
  // Constructor.
  WindowlessPluginTest(NPP id, NPNetscapeFuncs *host_functions,
                       const std::string& test_name);
  // NPAPI HandleEvent handler
  virtual int16 HandleEvent(void* event);

 private:
  std::string test_name_;
};

} // namespace NPAPIClient

#endif  // WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_EXECUTE_SCRIPT_DELETE_TEST_H
