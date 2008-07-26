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

#ifndef BASE_SCOPED_CFTYPEREF_H__
#define BASE_SCOPED_CFTYPEREF_H__

#include <CoreFoundation/CoreFoundation.h>

// scoped_cftyperef<> is patterned after scoped_ptr<>, but maintains ownership
// of a CoreFoundation object: any object that can be represented as a
// CFTypeRef.  Style deviations here are solely for compatibility with
// scoped_ptr<>'s interface, with which everyone is already familiar.
template<typename CFT>
class scoped_cftyperef {
 public:
  typedef CFT element_type;

  explicit scoped_cftyperef(CFT object = NULL)
      : object_(object) {
  }

  ~scoped_cftyperef() {
    if (object_)
      CFRelease(object_);
  }

  void reset(CFT object = NULL) {
    if (object_ && object_ != object) {
      CFRelease(object_);
      object_ = object;
    }
  }

  bool operator==(CFT that) const {
    return object_ == that;
  }

  bool operator!=(CFT that) const {
    return object_ != that;
  }

  operator CFT() const {
    return object_;
  }

  CFT get() const {
    return object_;
  }

  void swap(scoped_cftyperef& that) {
    CFT temp = that.object_;
    that.object_ = object_;
    object_ = temp;
  }

  // scoped_cftyperef<>::release() is like scoped_ptr<>::release.  It is NOT
  // a wrapper for CFRelease().  To force a scoped_cftyperef<> object to call
  // CFRelease(), use scoped_cftyperef<>::reset().
  CFT release() {
    CFT temp = object_;
    object_ = NULL;
    return temp;
  }

 private:
  CFT object_;

  DISALLOW_EVIL_CONSTRUCTORS(scoped_cftyperef);
};

#endif  // BASE_SCOPED_CFTYPEREF_H__
