// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define STRSAFE_NO_DEPRECATE
#include "webkit/glue/plugins/test/plugin_execute_script_delete_test.h"
#include "webkit/glue/plugins/test/plugin_client.h"

namespace NPAPIClient {

ExecuteScriptDeleteTest::ExecuteScriptDeleteTest(NPP id, NPNetscapeFuncs *host_functions)
  : PluginTest(id, host_functions) {
}

int16 ExecuteScriptDeleteTest::HandleEvent(void* event) {
  NPEvent* np_event = reinterpret_cast<NPEvent*>(event);
  if (WM_PAINT == np_event->event ) {
    NPNetscapeFuncs* browser = NPAPIClient::PluginClient::HostFunctions();
    NPUTF8* urlString = "javascript:DeletePluginWithinScript()";
    NPUTF8* targetString = NULL;
    browser->geturl(id(), urlString, targetString);
    SignalTestCompleted();
  }
  // If this test failed, then we'd have crashed by now.
  return PluginTest::HandleEvent(event);
}

} // namespace NPAPIClient

