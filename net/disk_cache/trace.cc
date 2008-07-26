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

#include <windows.h>

#include "net/disk_cache/trace.h"

#include "base/logging.h"

// Change this value to 1 to enable tracing on a release build. By default,
// tracing is enabled only on debug builds.
#define ENABLE_TRACING 0

#if _DEBUG
#undef ENABLE_TRACING
#define ENABLE_TRACING 1
#endif

namespace {

const int kEntrySize = 48;
const int kNumberOfEntries = 5000;  // 240 KB.

struct TraceBuffer {
  int num_traces;
  int current;
  char buffer[kNumberOfEntries][kEntrySize];
};

TraceBuffer* s_trace_buffer = NULL;

void DebugOutput(char* msg) {
  OutputDebugStringA(msg);
}

}  // namespace

namespace disk_cache {

#if ENABLE_TRACING

bool InitTrace(void) {
  DCHECK(!s_trace_buffer);
  if (s_trace_buffer)
    return false;

  s_trace_buffer = new TraceBuffer;
  memset(s_trace_buffer, 0, sizeof(*s_trace_buffer));
  return true;
}

void DestroyTrace(void) {
  DCHECK(s_trace_buffer);
  delete s_trace_buffer;
  s_trace_buffer = NULL;
}

void Trace(const char* format, ...) {
  DCHECK(s_trace_buffer);
  va_list ap;
  va_start(ap, format);

  vsprintf_s(s_trace_buffer->buffer[s_trace_buffer->current], format, ap);
  s_trace_buffer->num_traces++;
  s_trace_buffer->current++;
  if (s_trace_buffer->current == kNumberOfEntries)
    s_trace_buffer->current = 0;

  va_end(ap);
}

// Writes the last num_traces to the debugger output.
void DumpTrace(int num_traces) {
  DCHECK(s_trace_buffer);
  DebugOutput("Last traces:\n");

  if (num_traces > kNumberOfEntries || num_traces < 0)
    num_traces = kNumberOfEntries;

  if (s_trace_buffer->num_traces) {
    char line[kEntrySize + 2];

    int current = s_trace_buffer->current - num_traces;
    if (current < 0)
      current += kNumberOfEntries;

    for (int i = 0; i < num_traces; i++) {
      memcpy(line, s_trace_buffer->buffer[current], kEntrySize);
      line[kEntrySize] = '\0';
      size_t length = strlen(line);
      if (length) {
        line[length] = '\n';
        line[length + 1] = '\0';
        DebugOutput(line);
      }

      current++;
      if (current ==  kNumberOfEntries)
        current = 0;
    }
  }

  DebugOutput("End of Traces\n");
}

#else  // ENABLE_TRACING

bool InitTrace(void) {
  return true;
}

void DestroyTrace(void) {
}

void Trace(const char* format, ...) {
}

#endif  // ENABLE_TRACING

}  // namespace disk_cache
