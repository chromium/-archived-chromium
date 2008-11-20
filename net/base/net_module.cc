// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_module.h"

namespace net {

static NetModule::ResourceProvider resource_provider;

// static
void NetModule::SetResourceProvider(ResourceProvider func) {
  resource_provider = func;
}

// static
std::string NetModule::GetResource(int key) {
  // avoid thread safety issues by copying provider address to a local var
  ResourceProvider func = resource_provider;
  return func ? func(key) : std::string();
}

}  // namespace net

