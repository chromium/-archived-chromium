// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_ACTIVEX_SHIM_NPP_IMPL_H__
#define WEBKIT_ACTIVEX_SHIM_NPP_IMPL_H__

#include "webkit/glue/plugins/nphostapi.h"

namespace activex_shim {

// Entry functions. To avoid name conflict when used in activex_shim_dll, they
// are additionally prefixed.
NPError WINAPI ActiveX_Shim_NP_GetEntryPoints(NPPluginFuncs* funcs);
NPError WINAPI ActiveX_Shim_NP_Initialize(NPNetscapeFuncs* funcs);
NPError WINAPI ActiveX_Shim_NP_Shutdown(void);

// Initialized in NP_Initialize.
extern NPNetscapeFuncs* g_browser;

}  // namespace activex_shim

#endif // #ifndef WEBKIT_ACTIVEX_SHIM_NPP_IMPL_H__
