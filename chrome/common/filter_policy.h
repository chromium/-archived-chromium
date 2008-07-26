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

#ifndef CHROME_COMMON_FILTER_POLICY_H__
#define CHROME_COMMON_FILTER_POLICY_H__

#include "base/basictypes.h"

// When an insecure resource (mixed content or bad HTTPS) is loaded, the browser
// can decide to filter it.  The filtering is done in the renderer.  This class
// enumerates the different policy that can be used for the filtering.  It is
// passed along with resource response messages.
class FilterPolicy {
 public:
  enum Type {
    // Pass all types of resources through unmodified.
    DONT_FILTER = 0,

    // Block all types of resources, except images.  For images, modify them to
    // indicate that they have been filtered.
    // TODO(abarth): This is a misleading name for this enum value.  We should
    //               change it to something more suggestive of what this
    //               actually does.
    FILTER_ALL_EXCEPT_IMAGES,

    // Block all types of resources.
    FILTER_ALL
  };

  static bool ValidType(int32 type) {
    return type >= DONT_FILTER && type <= FILTER_ALL;
  }

  static Type FromInt(int32 type) {
    return static_cast<Type>(type);
  }

 private:
  // Don't instantiate this class.
  FilterPolicy();
  ~FilterPolicy();

};

#endif  // CHROME_COMMON_FILTER_POLICY_H__
