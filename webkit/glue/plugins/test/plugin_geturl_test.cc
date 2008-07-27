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

#include "webkit/glue/plugins/test/plugin_geturl_test.h"

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
    test_file_handle_(INVALID_HANDLE_VALUE) {
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

  unsigned long stream_id = PtrToUlong(stream->notifyData);
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

        test_file_handle_ = CreateFileA(filename.c_str(),
                                       GENERIC_READ,
                                       FILE_SHARE_READ,
                                       NULL,
                                       OPEN_EXISTING,
                                       0,
                                       NULL);
        if (test_file_handle_ == INVALID_HANDLE_VALUE)
          SetError("Could not open source file");
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
  unsigned long stream_id = PtrToUlong(stream->notifyData);
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

  unsigned long stream_id = PtrToUlong(stream->notifyData);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      self_url_.append(static_cast<char*>(buffer), len);
      break;
    case FETCHED_URL_STREAM_ID:
      {
        char read_buffer[STREAM_CHUNK];
        DWORD bytes = 0;
        if (!ReadFile(test_file_handle_, read_buffer, len,
                      &bytes, NULL))
          SetError("Could not read data from source file");
        // Technically, readfile could return fewer than len
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

  unsigned long stream_id = PtrToUlong(stream->notifyData);
  switch (stream_id) {
    case SELF_URL_STREAM_ID:
      // don't care
      break;
    case FETCHED_URL_STREAM_ID:
      {
        char read_buffer[STREAM_CHUNK];
        DWORD bytes = 0;
        if (!ReadFile(test_file_handle_, read_buffer, sizeof(read_buffer),
                      &bytes, NULL))
          SetError("Could not read data from source file");
        if (bytes != 0)
          SetError("Data and source mismatch on length");
        CloseHandle(test_file_handle_);
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

  unsigned long stream_id = PtrToUlong(stream->notifyData);
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

  unsigned long stream_id = PtrToUlong(data);
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
        char buf[10];
        _itoa_s(reason, buf, 10, 10);
        err.append(buf);
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
