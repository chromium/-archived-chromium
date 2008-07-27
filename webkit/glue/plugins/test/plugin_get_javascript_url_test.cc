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

#include "webkit/glue/plugins/test/plugin_get_javascript_url_test.h"


// url for "self".  
#define SELF_URL "javascript:window.location+\"\""
// The identifier for the self url stream.
#define SELF_URL_STREAM_ID 1

// The identifier for the fetched url stream.
#define FETCHED_URL_STREAM_ID 2

// The maximum chunk size of stream data.
#define STREAM_CHUNK 197

namespace NPAPIClient {

ExecuteGetJavascriptUrlTest::ExecuteGetJavascriptUrlTest(NPP id, NPNetscapeFuncs *host_functions)
  : PluginTest(id, host_functions),
    test_started_(false) {
}

NPError ExecuteGetJavascriptUrlTest::SetWindow(NPWindow* pNPWindow) {
  if (!test_started_) {
    std::string url = SELF_URL;
    HostFunctions()->geturlnotify(id(), url.c_str(), "_top",
                                  reinterpret_cast<void*>(SELF_URL_STREAM_ID));
    test_started_ = true;
  }
  return NPERR_NO_ERROR;
}

NPError ExecuteGetJavascriptUrlTest::NewStream(NPMIMEType type, NPStream* stream,
                              NPBool seekable, uint16* stype) {
  if (stream == NULL)
    SetError("NewStream got null stream");

  unsigned long stream_id = PtrToUlong(stream->notifyData);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      break;
    default:
      SetError("Unexpected NewStream callback");
      break;
  }
  return NPERR_NO_ERROR;
}

int32 ExecuteGetJavascriptUrlTest::WriteReady(NPStream *stream) {
  return STREAM_CHUNK;
}

int32 ExecuteGetJavascriptUrlTest::Write(NPStream *stream, int32 offset, int32 len,
                              void *buffer) {
  if (stream == NULL)
    SetError("Write got null stream");
  if (len < 0 || len > STREAM_CHUNK)
    SetError("Write got bogus stream chunk size");

  unsigned long stream_id = PtrToUlong(stream->notifyData);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      self_url_.append(static_cast<char*>(buffer), len);
      break;
    default:
      SetError("Unexpected write callback");
      break;
  }
  // Pretend that we took all the data.
  return len;
}


NPError ExecuteGetJavascriptUrlTest::DestroyStream(NPStream *stream, NPError reason) {
  if (stream == NULL)
    SetError("NewStream got null stream");

  unsigned long stream_id = PtrToUlong(stream->notifyData);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      // don't care
      break;
    default:
      SetError("Unexpected NewStream callback");
      break;
  }
  return NPERR_NO_ERROR;
}

void ExecuteGetJavascriptUrlTest::URLNotify(const char* url, NPReason reason, void* data) {
  unsigned long stream_id = PtrToUlong(data);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      if (strcmp(url, SELF_URL) != 0)
        SetError("URLNotify reported incorrect url for SELF_URL");
      if (self_url_.empty())
        SetError("Failed to obtain window location.");
      SignalTestCompleted();
      break;
    default:
      SetError("Unexpected NewStream callback");
      break;
  }
}

} // namespace NPAPIClient
