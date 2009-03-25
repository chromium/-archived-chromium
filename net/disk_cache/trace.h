// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides support for basic in-memory tracing of short events. We
// keep a static circular buffer where we store the last traced events, so we
// can review the cache recent behavior should we need it.

#ifndef NET_DISK_CACHE_TRACE_H__
#define NET_DISK_CACHE_TRACE_H__

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"

namespace disk_cache {

// Create and destroy the tracing buffer.
void InitTrace(void);
void DestroyTrace(void);

// Simple class to handle the trace buffer lifetime. Any object interested in
// tracing should keep a reference to the object returned by GetTraceObject().
class TraceObject : public base::RefCounted<TraceObject> {
  friend class base::RefCounted<TraceObject>;
 public:
  static TraceObject* GetTraceObject();

 private:
  TraceObject() {
    InitTrace();
  }

  ~TraceObject() {
    DestroyTrace();
  }
  DISALLOW_EVIL_CONSTRUCTORS(TraceObject);
};

// Traces to the internal buffer.
void Trace(const char* format, ...);

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_TRACE_H__
