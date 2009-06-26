/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "plugin/cross/main.h"

using glue::_o3d::PluginObject;
using glue::StreamManager;

int BreakpadEnabler::scope_count_ = 0;

// Used for breakpad crash handling
ExceptionManager *g_exception_manager = NULL;

extern "C" {
  char *NP_GetMIMEDescription(void) {
    return O3D_PLUGIN_MIME_TYPE "::O3D MIME";
  }

  NPError NP_GetValue(void *instance, NPPVariable variable, void *value) {
    switch (variable) {
      case NPPVpluginNameString:
        *static_cast<char **>(value) = O3D_PLUGIN_NAME;
        break;
      case NPPVpluginDescriptionString:
        *static_cast<char **>(value) = O3D_PLUGIN_DESCRIPTION;
        break;
      default:
        return NPERR_INVALID_PARAM;
        break;
    }
    return NPERR_NO_ERROR;
  }

  NPError OSCALL NP_GetEntryPoints(NPPluginFuncs *pluginFuncs) {
    HANDLE_CRASHES;
    pluginFuncs->version = 11;
    pluginFuncs->size = sizeof(*pluginFuncs);
    pluginFuncs->newp = NPP_New;
    pluginFuncs->destroy = NPP_Destroy;
    pluginFuncs->setwindow = NPP_SetWindow;
    pluginFuncs->newstream = NPP_NewStream;
    pluginFuncs->destroystream = NPP_DestroyStream;
    pluginFuncs->asfile = NPP_StreamAsFile;
    pluginFuncs->writeready = NPP_WriteReady;
    pluginFuncs->write = NPP_Write;
    pluginFuncs->print = NPP_Print;
    pluginFuncs->event = NPP_HandleEvent;
    pluginFuncs->urlnotify = NPP_URLNotify;
    pluginFuncs->getvalue = NPP_GetValue;
    pluginFuncs->setvalue = NPP_SetValue;

    return NPERR_NO_ERROR;
  }

  NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream *stream,
                        NPBool seekable, uint16 *stype) {
    HANDLE_CRASHES;
    PluginObject *obj = static_cast<PluginObject*>(instance->pdata);
    StreamManager *stream_manager = obj->stream_manager();
    if (stream_manager->NewStream(stream, stype)) {
      return NPERR_NO_ERROR;
    } else {
      // TODO: find out which error we should return
      return NPERR_INVALID_PARAM;
    }
  }

  NPError NPP_DestroyStream(NPP instance, NPStream *stream, NPReason reason) {
    HANDLE_CRASHES;
    PluginObject *obj = static_cast<PluginObject*>(instance->pdata);
    StreamManager *stream_manager = obj->stream_manager();
    if (stream_manager->DestroyStream(stream, reason)) {
      return NPERR_NO_ERROR;
    } else {
      // TODO: find out which error we should return
      return NPERR_INVALID_PARAM;
    }
  }

  int32 NPP_WriteReady(NPP instance, NPStream *stream) {
    HANDLE_CRASHES;
    PluginObject *obj = static_cast<PluginObject*>(instance->pdata);
    StreamManager *stream_manager = obj->stream_manager();
    return stream_manager->WriteReady(stream);
  }

  int32 NPP_Write(NPP instance, NPStream *stream, int32 offset, int32 len,
                  void *buffer) {
    HANDLE_CRASHES;
    PluginObject *obj = static_cast<PluginObject*>(instance->pdata);
    StreamManager *stream_manager = obj->stream_manager();
    return stream_manager->Write(stream, offset, len, buffer);
  }

  void NPP_Print(NPP instance, NPPrint *platformPrint) {
    HANDLE_CRASHES;
  }

  void NPP_URLNotify(NPP instance, const char *url, NPReason reason,
                     void *notifyData) {
    HANDLE_CRASHES;
    PluginObject *obj = static_cast<PluginObject*>(instance->pdata);
    StreamManager *stream_manager = obj->stream_manager();
    stream_manager->URLNotify(url, reason, notifyData);
  }

  NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value) {
    HANDLE_CRASHES;
    switch (variable) {
      case NPPVpluginScriptableNPObject: {
        void **v = static_cast<void **>(value);
        PluginObject *obj = static_cast<PluginObject *>(instance->pdata);
        // Return value is expected to be retained
        GLUE_PROFILE_START(instance, "retainobject");
        NPN_RetainObject(obj);
        GLUE_PROFILE_STOP(instance, "retainobject");
        *v = obj;
        break;
      }
      default:
        return NP_GetValue(instance, variable, value);
        break;
    }
    return NPERR_NO_ERROR;
  }

  NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value) {
    HANDLE_CRASHES;
    return NPERR_GENERIC_ERROR;
  }
}  // extern "C"
