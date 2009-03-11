// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"

#if defined(OS_WIN)
#define STRSAFE_NO_DEPRECATE
#include <strsafe.h>
#endif
#include "webkit/glue/plugins/test/plugin_npobject_proxy_test.h"

namespace NPAPIClient {

NPObjectProxyTest::NPObjectProxyTest(NPP id, NPNetscapeFuncs *host_functions)
  : PluginTest(id, host_functions) {
}

NPError NPObjectProxyTest::SetWindow(NPWindow* pNPWindow) {
  NPIdentifier document_id = HostFunctions()->getstringidentifier("document");
  NPIdentifier create_text_node_id = HostFunctions()->getstringidentifier("createTextNode");
  NPIdentifier append_child_id = HostFunctions()->getstringidentifier("appendChild");

  NPVariant docv;
  NPObject *window_obj = NULL;
  HostFunctions()->getvalue(id(), NPNVWindowNPObject, &window_obj);

  HostFunctions()->getproperty(id(), window_obj, document_id, &docv);
  NPObject *doc = NPVARIANT_TO_OBJECT(docv);

  NPVariant strv;
  MSVC_SUPPRESS_WARNING(4267);
  STRINGZ_TO_NPVARIANT("div", strv);

  NPVariant textv;
  HostFunctions()->invoke(id(), doc, create_text_node_id, &strv, 1, &textv);

  NPVariant v;
  HostFunctions()->invoke(id(), doc, append_child_id, &textv, 1, &v);

  // If this test failed, then we'd have crashed by now.
  SignalTestCompleted();

  return NPERR_NO_ERROR;
}

} // namespace NPAPIClient
