// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/test/plugin_private_test.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "webkit/glue/plugins/test/plugin_client.h"

namespace NPAPIClient {

PrivateTest::PrivateTest(NPP id, NPNetscapeFuncs *host_functions)
    : PluginTest(id, host_functions) {
}

NPError PrivateTest::New(uint16 mode, int16 argc,
                         const char* argn[], const char* argv[],
                         NPSavedData* saved) {
  PluginTest::New(mode, argc, argn, argv, saved);

  NPBool private_mode = 0;
  NPNetscapeFuncs* browser = NPAPIClient::PluginClient::HostFunctions();
  NPError result = browser->getvalue(
      id(), NPNVprivateModeBool, &private_mode);
  if (result != NPERR_NO_ERROR) {
    SetError("Failed to read NPNVprivateModeBool value.");
  } else {
    NPIdentifier location = HostFunctions()->getstringidentifier("location");
    NPIdentifier href = HostFunctions()->getstringidentifier("href");

    NPObject *window_obj = NULL;
    HostFunctions()->getvalue(id(), NPNVWindowNPObject, &window_obj);

    NPVariant location_var;
    HostFunctions()->getproperty(id(), window_obj, location, &location_var);

    NPVariant href_var;
    HostFunctions()->getproperty(id(), NPVARIANT_TO_OBJECT(location_var), href,
                                 &href_var);
    std::string href_str(href_var.value.stringValue.UTF8Characters,
                         href_var.value.stringValue.UTF8Length);
    bool private_expected = href_str.find("?private") != href_str.npos;
    if (private_mode != static_cast<NPBool>(private_expected))
      SetError("NPNVprivateModeBool returned incorrect value.");

    HostFunctions()->releasevariantvalue(&href_var);
    HostFunctions()->releasevariantvalue(&location_var);
    HostFunctions()->releaseobject(window_obj);
  }

  SignalTestCompleted();

  return NPERR_NO_ERROR;
}

} // namespace NPAPIClient
