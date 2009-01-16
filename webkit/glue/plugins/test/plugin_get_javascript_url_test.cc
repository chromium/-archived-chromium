// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/test/plugin_get_javascript_url_test.h"

#include "base/basictypes.h"

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

  COMPILE_ASSERT(sizeof(unsigned long) <= sizeof(stream->notifyData),
                 cast_validity_check);
  unsigned long stream_id = reinterpret_cast<unsigned long>(stream->notifyData);
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

  COMPILE_ASSERT(sizeof(unsigned long) <= sizeof(stream->notifyData),
                 cast_validity_check);
  unsigned long stream_id = reinterpret_cast<unsigned long>(stream->notifyData);
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

  COMPILE_ASSERT(sizeof(unsigned long) <= sizeof(stream->notifyData),
                 cast_validity_check);
  unsigned long stream_id = reinterpret_cast<unsigned long>(stream->notifyData);
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
  COMPILE_ASSERT(sizeof(unsigned long) <= sizeof(data),
                 cast_validity_check);
  unsigned long stream_id = reinterpret_cast<unsigned long>(data);
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

