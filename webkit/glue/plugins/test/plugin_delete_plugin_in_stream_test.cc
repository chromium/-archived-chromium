// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
