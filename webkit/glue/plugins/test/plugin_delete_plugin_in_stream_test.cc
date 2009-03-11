// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/test/plugin_delete_plugin_in_stream_test.h"

#include "webkit/glue/plugins/test/plugin_client.h"

namespace NPAPIClient {

#define kUrl "javascript:window.location+\"\""
#define kUrlStreamId 1

DeletePluginInStreamTest::DeletePluginInStreamTest(NPP id, NPNetscapeFuncs *host_functions)
  : PluginTest(id, host_functions),
    test_started_(false) {
}

NPError DeletePluginInStreamTest::SetWindow(NPWindow* pNPWindow) {
  if (!test_started_) {
    std::string url = "self_delete_plugin_stream.html";
    HostFunctions()->geturlnotify(id(), url.c_str(), NULL,
                                  reinterpret_cast<void*>(kUrlStreamId));
    test_started_ = true;
  }
  return NPERR_NO_ERROR;
}

NPError DeletePluginInStreamTest::NewStream(NPMIMEType type, NPStream* stream,
                              NPBool seekable, uint16* stype) {
  NPIdentifier delete_id = HostFunctions()->getstringidentifier("DeletePluginWithinScript");

  NPObject *window_obj = NULL;
  HostFunctions()->getvalue(id(), NPNVWindowNPObject, &window_obj);

  NPVariant rv;
  HostFunctions()->invoke(id(), window_obj, delete_id, NULL, 0, &rv);

  return NPERR_NO_ERROR;
}

} // namespace NPAPIClient
