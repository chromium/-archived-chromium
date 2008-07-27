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
