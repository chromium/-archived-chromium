// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/test/plugin_geturl_test.h"

#include <stdio.h>

#include "base/basictypes.h"
#include "base/file_util.h"

// url for "self".  The %22%22 is to make a statement for javascript to
// evaluate and return.
#define SELF_URL "javascript:window.location+\"\""

// The identifier for the self url stream.
#define SELF_URL_STREAM_ID 1

// The identifier for the fetched url stream.
#define FETCHED_URL_STREAM_ID 2

// url for testing GetURL with a bogus URL.
#define BOGUS_URL "bogoproto:///x:/asdf.xysdhffieasdf.asdhj/"

// The identifier for the bogus url stream.
#define BOGUS_URL_STREAM_ID 3

// The maximum chunk size of stream data.
#define STREAM_CHUNK 197

namespace NPAPIClient {

PluginGetURLTest::PluginGetURLTest(NPP id, NPNetscapeFuncs *host_functions)
  : PluginTest(id, host_functions),
    tests_started_(false),
    tests_in_progress_(0),
    test_file_(NULL) {
}

NPError PluginGetURLTest::SetWindow(NPWindow* pNPWindow) {
  if (!tests_started_) {
    tests_started_ = true;

    tests_in_progress_++;

    std::string url = SELF_URL;
    HostFunctions()->geturlnotify(id(), url.c_str(), NULL,
                                  reinterpret_cast<void*>(SELF_URL_STREAM_ID));

    tests_in_progress_++;
    std::string bogus_url = BOGUS_URL;
    HostFunctions()->geturlnotify(id(), bogus_url.c_str(), NULL,
                                  reinterpret_cast<void*>(BOGUS_URL_STREAM_ID));
  }
  return NPERR_NO_ERROR;
}

NPError PluginGetURLTest::NewStream(NPMIMEType type, NPStream* stream,
                              NPBool seekable, uint16* stype) {
  if (stream == NULL)
    SetError("NewStream got null stream");

  COMPILE_ASSERT(sizeof(unsigned long) <= sizeof(stream->notifyData),
                 cast_validity_check);
  unsigned long stream_id = reinterpret_cast<unsigned long>(
      stream->notifyData);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      break;
    case FETCHED_URL_STREAM_ID:
      {
        std::string filename = self_url_;
        if (filename.find("file:///", 0) != 0) {
          SetError("Test expects a file-url.");
          break;
        }

        filename = filename.substr(8);  // remove "file:///"

        test_file_ = file_util::OpenFile(filename, "r");
        if (!test_file_) {
          SetError("Could not open source file");
        }
      }
      break;
    case BOGUS_URL_STREAM_ID:
      SetError("Unexpected NewStream for BOGUS_URL");
      break;
    default:
      SetError("Unexpected NewStream callback");
      break;
  }
  return NPERR_NO_ERROR;
}

int32 PluginGetURLTest::WriteReady(NPStream *stream) {
  COMPILE_ASSERT(sizeof(unsigned long) <= sizeof(stream->notifyData),
                 cast_validity_check);
  unsigned long stream_id = reinterpret_cast<unsigned long>(
      stream->notifyData);
  if (stream_id == BOGUS_URL_STREAM_ID)
    SetError("Received WriteReady for BOGUS_URL");

  return STREAM_CHUNK;
}

int32 PluginGetURLTest::Write(NPStream *stream, int32 offset, int32 len,
                              void *buffer) {
  if (stream == NULL)
    SetError("Write got null stream");
  if (len < 0 || len > STREAM_CHUNK)
    SetError("Write got bogus stream chunk size");

  COMPILE_ASSERT(sizeof(unsigned long) <= sizeof(stream->notifyData),
                 cast_validity_check);
  unsigned long stream_id = reinterpret_cast<unsigned long>(
      stream->notifyData);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      self_url_.append(static_cast<char*>(buffer), len);
      break;
    case FETCHED_URL_STREAM_ID:
      {
        char read_buffer[STREAM_CHUNK];
        int32 bytes = fread(read_buffer, 1, len, test_file_);
        // Technically, fread could return fewer than len
        // bytes.  But this is not likely.
        if (bytes != len)
          SetError("Did not read correct bytelength from source file");
        if (memcmp(read_buffer, buffer, len))
          SetError("Content mismatch between data and source!");
      }
      break;
    case BOGUS_URL_STREAM_ID:
      SetError("Unexpected write callback for BOGUS_URL");
      break;
    default:
      SetError("Unexpected write callback");
      break;
  }
  // Pretend that we took all the data.
  return len;
}


NPError PluginGetURLTest::DestroyStream(NPStream *stream, NPError reason) {
  if (stream == NULL)
    SetError("NewStream got null stream");

  COMPILE_ASSERT(sizeof(unsigned long) <= sizeof(stream->notifyData),
                 cast_validity_check);
  unsigned long stream_id =
      reinterpret_cast<unsigned long>(stream->notifyData);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      // don't care
      break;
    case FETCHED_URL_STREAM_ID:
      {
        char read_buffer[STREAM_CHUNK];
        size_t bytes = fread(read_buffer, 1, sizeof(read_buffer), test_file_);
        if (bytes != 0)
          SetError("Data and source mismatch on length");
        file_util::CloseFile(test_file_);
      }
      break;
    default:
      SetError("Unexpected NewStream callback");
      break;
  }
  return NPERR_NO_ERROR;
}

void PluginGetURLTest::StreamAsFile(NPStream* stream, const char* fname) {
  if (stream == NULL)
    SetError("NewStream got null stream");

  COMPILE_ASSERT(sizeof(unsigned long) <= sizeof(stream->notifyData),
                 cast_validity_check);
  unsigned long stream_id =
      reinterpret_cast<unsigned long>(stream->notifyData);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      // don't care
      break;
    default:
      SetError("Unexpected NewStream callback");
      break;
  }
}

void PluginGetURLTest::URLNotify(const char* url, NPReason reason, void* data) {
  if (!tests_in_progress_) {
    SetError("URLNotify received after tests completed");
    return;
  }

  if (!url) {
    SetError("URLNotify received NULL url");
    return;
  }

  COMPILE_ASSERT(sizeof(unsigned long) <= sizeof(data), cast_validity_check);
  unsigned long stream_id = reinterpret_cast<unsigned long>(data);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      if (strcmp(url, SELF_URL) != 0)
        SetError("URLNotify reported incorrect url for SELF_URL");

      // We have our stream url.  Go fetch it.
      HostFunctions()->geturlnotify(id(), self_url_.c_str(), NULL,
                            reinterpret_cast<void*>(FETCHED_URL_STREAM_ID));
      break;
    case FETCHED_URL_STREAM_ID:
      if (!url || strcmp(url, self_url_.c_str()) != 0)
        SetError("URLNotify reported incorrect url for FETCHED_URL");
      tests_in_progress_--;
      break;
    case BOGUS_URL_STREAM_ID:
      if (reason != NPRES_NETWORK_ERR) {
        std::string err = "BOGUS_URL received unexpected URLNotify status: ";
        err.append(IntToString(reason));
        SetError(err);
      }
      tests_in_progress_--;
      break;
    default:
      SetError("Unexpected NewStream callback");
      break;
  }

  if (tests_in_progress_ == 0)
      SignalTestCompleted();
}

} // namespace NPAPIClient
