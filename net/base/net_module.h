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

#ifndef NET_BASE_NET_MODULE_H__
#define NET_BASE_NET_MODULE_H__

#include <string>

#include "base/basictypes.h"

// Defines global initializers and associated methods for the net module.
//
// The network module does not have direct access to the way application
// resources are stored and fetched by the embedding application (e.g., it
// cannot see the ResourceBundle class used by Chrome), so it uses this API to
// get access to such resources.
//
class NetModule {
 public:
  typedef std::string (*ResourceProvider)(int key);

  // Set the function to call when the net module needs resources
  static void SetResourceProvider(ResourceProvider func);

  // Call the resource provider (if one exists) to get the specified resource.
  // Returns an empty string if the resource does not exist or if there is no
  // resource provider.
  static std::string GetResource(int key);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(NetModule);
};

#endif  // NET_BASE_NET_MODULE_H__
