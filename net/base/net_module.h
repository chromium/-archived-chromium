// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_MODULE_H__
#define NET_BASE_NET_MODULE_H__

#include "base/basictypes.h"
#include "base/string_piece.h"

namespace net {

// Defines global initializers and associated methods for the net module.
//
// The network module does not have direct access to the way application
// resources are stored and fetched by the embedding application (e.g., it
// cannot see the ResourceBundle class used by Chrome), so it uses this API to
// get access to such resources.
//
class NetModule {
 public:
  typedef StringPiece (*ResourceProvider)(int key);

  // Set the function to call when the net module needs resources
  static void SetResourceProvider(ResourceProvider func);

  // Call the resource provider (if one exists) to get the specified resource.
  // Returns an empty string if the resource does not exist or if there is no
  // resource provider.
  static StringPiece GetResource(int key);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(NetModule);
};

}  // namespace net

#endif  // NET_BASE_NET_MODULE_H__
