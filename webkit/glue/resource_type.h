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

#ifndef WEBKIT_GLUE_RESOURCE_TYPE_H__
#define WEBKIT_GLUE_RESOURCE_TYPE_H__


class ResourceType {
 public:
  enum Type {
    MAIN_FRAME = 0,  // top level page
    SUB_FRAME,       // frame or iframe
    SUB_RESOURCE,    // a resource like images, js, css
    OBJECT,          // an object (or embed) tag for a plugin,
                     // or a resource that a plugin requested.
  };

  static bool ValidType(int32 type) {
    return type >= MAIN_FRAME && type <= OBJECT;
  }

  static Type FromInt(int32 type) {
    return static_cast<Type>(type);
  }

  static bool IsFrame(ResourceType::Type type) {
    return type == MAIN_FRAME || type == SUB_FRAME;
  }

 private:
  // Don't instantiate this class.
  ResourceType();
  ~ResourceType();
};
#endif  // WEBKIT_GLUE_RESOURCE_TYPE_H__
