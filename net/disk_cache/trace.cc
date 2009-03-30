// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/trace.h"

#include <stdio.h>
#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/logging.h"

// Change this value to 1 to enable tracing on a release build. By default,
// tracing is enabled only on debug builds.
#define ENABLE_TRACING 0

#ifndef NDEBUG
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

void DebugOutput(const char* msg) {
#if defined(OS_WIN)
  OutputDebugStringA(msg);
#else
  NOTIMPLEMENTED();
#endif
}

}  // namespace

namespace disk_cache {

// s_trace_buffer and s_trace_object are not singletons because I want the
// buffer to be destroyed and re-created when the last user goes away, and it
// must be straightforward to access the buffer from the debugger.
static TraceObject* s_trace_object = NULL;

// Static.
TraceObject* TraceObject::GetTraceObject() {
  if (s_trace_object)
    return s_trace_object;

  s_trace_object = new TraceObject();
  return s_trace_object;
}

#if ENABLE_TRACING

static TraceBuffer* s_trace_buffer = NULL;

void InitTrace(void) {
  if (s_trace_buffer)
    return;

  s_trace_buffer = new TraceBuffer;
  memset(s_trace_buffer, 0, sizeof(*s_trace_buffer));
}

void DestroyTrace(void) {
  DCHECK(s_trace_buffer);
  delete s_trace_buffer;
  s_trace_buffer = NULL;
  s_trace_object = NULL;
}

void Trace(const char* format, ...) {
  DCHECK(s_trace_buffer);
  va_list ap;
  va_start(ap, format);

#if defined(OS_WIN)
  vsprintf_s(s_trace_buffer->buffer[s_trace_buffer->current], format, ap);
#else
  vsnprintf(s_trace_buffer->buffer[s_trace_buffer->current],
            sizeof(s_trace_buffer->buffer[s_trace_buffer->current]), format,
            ap);
#endif
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

void InitTrace(void) {
  return;
}

void DestroyTrace(void) {
  s_trace_object = NULL;
}

void Trace(const char* format, ...) {
}

#endif  // ENABLE_TRACING

}  // namespace disk_cache
