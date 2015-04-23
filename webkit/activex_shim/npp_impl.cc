// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/activex_shim/npp_impl.h"

#include <algorithm>

#include "webkit/activex_shim/activex_plugin.h"
#include "webkit/activex_shim/activex_util.h"

#pragma warning(disable: 4267 4312)

using std::string;
using std::wstring;

namespace activex_shim {

NPNetscapeFuncs* g_browser;

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
void NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname);
void NPP_Print(NPP instance, NPPrint* platformPrint);
int16 NPP_HandleEvent(NPP instance, void* event);
void NPP_URLNotify(NPP instance, const char* url, NPReason reason,
                   void* notifyData);
NPError NPP_GetValue(NPP instance, NPPVariable variable, void* value);
NPError NPP_SetValue(NPP instance, NPNVariable variable, void* value);

NPError WINAPI ActiveX_Shim_NP_GetEntryPoints(NPPluginFuncs* funcs) {
  funcs->version = 11;
  funcs->size = sizeof(*funcs);
  funcs->newp = NPP_New;
  funcs->destroy = NPP_Destroy;
  funcs->setwindow = NPP_SetWindow;
  funcs->newstream = NPP_NewStream;
  funcs->destroystream = NPP_DestroyStream;
  funcs->asfile = NPP_StreamAsFile;
  funcs->writeready = NPP_WriteReady;
  funcs->write = NPP_Write;
  funcs->print = NPP_Print;
  funcs->event = NPP_HandleEvent;
  funcs->urlnotify = NPP_URLNotify;
  funcs->getvalue = NPP_GetValue;
  funcs->setvalue = NPP_SetValue;

  return NPERR_NO_ERROR;
}

NPError WINAPI ActiveX_Shim_NP_Initialize(NPNetscapeFuncs* funcs) {
  g_browser = funcs;

  // Not all browsers call CoInitialize during startup. Do this to make sure we
  // won't have problem later on creating ActiveX controls.
  // Right now the object creation request comes from the same thread as the
  // one calling NP_Initialize. We should watch it if it comes from a different
  // thread.
  CoInitialize(NULL);
  return 0;
}

NPError WINAPI ActiveX_Shim_NP_Shutdown(void) {
  CoUninitialize();
  return 0;
}

NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc,
                char* argn[], char* argv[], NPSavedData* saved) {
  ActiveXPlugin* plugin = new ActiveXPlugin(instance);
  instance->pdata = plugin;
  return plugin->NPP_New(pluginType, argc, argn, argv, saved);
}

NPError NPP_Destroy(NPP instance, NPSavedData** save) {
  ActiveXPlugin* plugin = static_cast<ActiveXPlugin*>(instance->pdata);
  delete plugin;
  return 0;
}

NPError NPP_SetWindow(NPP instance, NPWindow* window) {
  ActiveXPlugin* plugin = static_cast<ActiveXPlugin*>(instance->pdata);
  return plugin->NPP_SetWindow(window);
}

NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream,
                      NPBool seekable, uint16* stype) {
  ActiveXPlugin* plugin = static_cast<ActiveXPlugin*>(instance->pdata);
  return plugin->NPP_NewStream(type, stream, seekable, stype);
}

NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason) {
  ActiveXPlugin* plugin = static_cast<ActiveXPlugin*>(instance->pdata);
  return plugin->NPP_DestroyStream(stream, reason);
}

int32 NPP_WriteReady(NPP instance, NPStream* stream) {
  ActiveXPlugin* plugin = static_cast<ActiveXPlugin*>(instance->pdata);
  return plugin->NPP_WriteReady(stream);
}

int32 NPP_Write(NPP instance, NPStream* stream, int32 offset, int32 len,
                void* buffer) {
  ActiveXPlugin* plugin = static_cast<ActiveXPlugin*>(instance->pdata);
  return plugin->NPP_Write(stream, offset, len, buffer);
}

void NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname) {
  ActiveXPlugin* plugin = static_cast<ActiveXPlugin*>(instance->pdata);
  plugin->NPP_StreamAsFile(stream, fname);
}

void NPP_Print(NPP instance, NPPrint* platformPrint) {
  ActiveXPlugin* plugin = static_cast<ActiveXPlugin*>(instance->pdata);
  plugin->NPP_Print(platformPrint);
}

int16 NPP_HandleEvent(NPP instance, void* event) {
  ActiveXPlugin* plugin = static_cast<ActiveXPlugin*>(instance->pdata);
  return plugin->NPP_HandleEvent(event);
}

void NPP_URLNotify(NPP instance, const char* url, NPReason reason,
                   void* notifyData) {
  ActiveXPlugin* plugin = static_cast<ActiveXPlugin*>(instance->pdata);
  plugin->NPP_URLNotify(url, reason, notifyData);
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void* value) {
  ActiveXPlugin* plugin = static_cast<ActiveXPlugin*>(instance->pdata);
  return plugin->NPP_GetValue(variable, value);
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void* value) {
  ActiveXPlugin* plugin = static_cast<ActiveXPlugin*>(instance->pdata);
  return plugin->NPP_SetValue(variable, value);
}

}  // namespace activex_shim
