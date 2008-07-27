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

#ifndef V8_UTILITY_H__
#define V8_UTILITY_H__

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
  if (fun.IsEmpty()) return v8::Local<v8::Object>();
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

#endif   // V8_UTILITY_H__
