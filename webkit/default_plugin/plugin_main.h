// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "third_party/npapi/bindings/npapi.h"
#include "webkit/glue/plugins/nphostapi.h"

namespace default_plugin {

extern NPNetscapeFuncs* g_browser;

// Standard NPAPI functions.
NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc,
                char* argn[], char* argv[], NPSavedData* saved);
NPError NPP_Destroy(NPP instance, NPSavedData** save);
NPError NPP_SetWindow(NPP instance, NPWindow* window);
NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream,
                      NPBool seekable, uint16* stype);
NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason);
int32 NPP_WriteReady(NPP instance, NPStream* stream);
int32 NPP_Write(NPP instance, NPStream* stream, int32 offset, int32 len,
                void* buffer);
void NPP_URLNotify(NPP instance, const char* url, NPReason reason,
                   void* notifyData);
int16 NPP_HandleEvent(NPP instance, void* event);

NPError API_CALL NP_GetEntryPoints(NPPluginFuncs* funcs);
NPError API_CALL NP_Initialize(NPNetscapeFuncs* funcs);
NPError API_CALL NP_Shutdown(void);

}  // default_plugin
