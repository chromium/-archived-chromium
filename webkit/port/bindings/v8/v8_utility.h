// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UTILITY_H__
#define V8_UTILITY_H__

#include "V8Utilities.h"

// To break a cycle dependency during upstreaming this block of code,
// ifdefing it out based on this #define.
//
// TODO(ajwong): After https://bugs.webkit.org/show_bug.cgi?id=25595
// lands and is rolled down into chromium, migrate all headaers to use
// the code in V8Utilities.h directly, and then, remove this code.
#ifndef V8UTILITIES_DEFINED
namespace WebCore {

class AllowAllocation {
 public:
  inline AllowAllocation() {
    m_prev = m_current;
    m_current = true;
  }
  inline ~AllowAllocation() {
    m_current = m_prev;
  }
  static bool m_current;
 private:
  bool m_prev;
};

class SafeAllocation {
 public:
  static inline v8::Local<v8::Object> NewInstance(
      v8::Handle<v8::Function> fun);
  static inline v8::Local<v8::Object> NewInstance(
      v8::Handle<v8::ObjectTemplate> templ);
  static inline v8::Local<v8::Object> NewInstance(
      v8::Handle<v8::Function> fun, int argc, v8::Handle<v8::Value> argv[]);
};

v8::Local<v8::Object> SafeAllocation::NewInstance(
    v8::Handle<v8::Function> fun) {
  if (fun.IsEmpty())
    return v8::Local<v8::Object>();
  AllowAllocation allow;
  return fun->NewInstance();
}

v8::Local<v8::Object> SafeAllocation::NewInstance(
    v8::Handle<v8::ObjectTemplate> templ) {
  if (templ.IsEmpty()) return v8::Local<v8::Object>();
  AllowAllocation allow;
  return templ->NewInstance();
}

v8::Local<v8::Object> SafeAllocation::NewInstance(
    v8::Handle<v8::Function> fun, int argc, v8::Handle<v8::Value> argv[]) {
  if (fun.IsEmpty()) return v8::Local<v8::Object>();
  AllowAllocation allow;
  return fun->NewInstance(argc, argv);
}

}  // namespace WebCore
#endif  // V8UTILITIES_DEFINED

#endif   // V8_UTILITY_H__
