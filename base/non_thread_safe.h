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

#ifndef BASE_NON_THREAD_SAFE_H__
#define BASE_NON_THREAD_SAFE_H__

#include "base/logging.h"

// A helper class used to help verify that methods of a class are
// called from the same thread.  One can inherit from this class and use
// CalledOnValidThread() to verify.
//
// This is intended to be used with classes that appear to be thread safe, but
// aren't.  For example, a service or a singleton like the preferences system.
//
// Example:
// class MyClass : public NonThreadSafe {
//  public:
//   void Foo() {
//     DCHECK(CalledOnValidThread());
//     ... (do stuff) ...
//   }
// }
//
// In Release mode, CalledOnValidThread will always return true.
//
#ifndef NDEBUG
class NonThreadSafe {
 public:
  NonThreadSafe();
  ~NonThreadSafe();

 protected:
  bool CalledOnValidThread() const;

 private:
  int32 valid_thread_id_;
};
#else
// Do nothing in release mode.
class NonThreadSafe {
 public:
  NonThreadSafe() {}
  ~NonThreadSafe() {}

 protected:
  bool CalledOnValidThread() const {
    return true;
  }
};
#endif  // NDEBUG

#endif  // BASE_NON_THREAD_SAFE_H__
