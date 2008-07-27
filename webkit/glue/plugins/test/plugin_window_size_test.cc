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

#include "webkit/glue/plugins/test/plugin_window_size_test.h"
#include "webkit/glue/plugins/test/plugin_client.h"

namespace NPAPIClient {

PluginWindowSizeTest::PluginWindowSizeTest(NPP id, 
                                           NPNetscapeFuncs *host_functions)
    : PluginTest(id, host_functions) {
}

NPError PluginWindowSizeTest::SetWindow(NPWindow* pNPWindow) {
  if (!pNPWindow || 
      !::IsWindow(reinterpret_cast<HWND>(pNPWindow->window))) {
    SetError("Invalid arguments passed in");
    return NPERR_INVALID_PARAM;
  }

  RECT window_rect = {0};
  window_rect.left = pNPWindow->x;
  window_rect.top = pNPWindow->y;
  window_rect.right = pNPWindow->width;
  window_rect.bottom = pNPWindow->height;
  
  if (!::IsRectEmpty(&window_rect)) {
    RECT client_rect = {0};
    ::GetClientRect(reinterpret_cast<HWND>(pNPWindow->window),
                    &client_rect);
    if (::IsRectEmpty(&client_rect)) {
      SetError("The client rect of the plugin window is empty. Test failed");
    }

    SignalTestCompleted();
  }

  return NPERR_NO_ERROR;
}

} // namespace NPAPIClient
