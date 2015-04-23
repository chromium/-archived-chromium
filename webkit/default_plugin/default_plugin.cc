// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/default_plugin/plugin_main.h"

extern "C" {
NPError API_CALL NP_GetEntryPoints(NPPluginFuncs* funcs) {
  return default_plugin::NP_GetEntryPoints(funcs);
}

NPError API_CALL NP_Initialize(NPNetscapeFuncs* funcs) {
  return default_plugin::NP_Initialize(funcs);
}

NPError API_CALL NP_Shutdown() {
  return default_plugin::NP_Shutdown();
}
} // extern "C"
