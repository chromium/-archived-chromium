// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/default_plugin/plugin_main.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "webkit/activex_shim/npp_impl.h"
#include "webkit/default_plugin/plugin_impl.h"
#include "webkit/glue/webkit_glue.h"

namespace default_plugin {
//
// Forward declare the linker-provided pseudo variable for the
// current module handle.
//
extern "C" IMAGE_DOS_HEADER __ImageBase;

// get the handle to the currently executing module
inline HMODULE GetCurrentModuleHandle() {
  return reinterpret_cast<HINSTANCE>(&__ImageBase);
}

// Initialized in NP_Initialize.
NPNetscapeFuncs* g_browser = NULL;

NPError API_CALL NP_GetEntryPoints(NPPluginFuncs* funcs) {
  funcs->version = 11;
  funcs->size = sizeof(*funcs);
  funcs->newp = NPP_New;
  funcs->destroy = NPP_Destroy;
  funcs->setwindow = NPP_SetWindow;
  funcs->newstream = NPP_NewStream;
  funcs->destroystream = NPP_DestroyStream;
  funcs->writeready = NPP_WriteReady;
  funcs->write = NPP_Write;
  funcs->asfile = NULL;
  funcs->print = NULL;
  funcs->event = NPP_HandleEvent;
  funcs->urlnotify = NPP_URLNotify;
  funcs->getvalue = NULL;
  funcs->setvalue = NULL;
  return NPERR_NO_ERROR;
}

NPError API_CALL NP_Initialize(NPNetscapeFuncs* funcs) {
  g_browser = funcs;
  activex_shim::g_browser = funcs;
  return 0;
}

NPError API_CALL NP_Shutdown(void) {
  g_browser = NULL;
  return 0;
}

namespace {
// This function is only invoked when the default plugin is invoked
// with a special mime type application/chromium-test-default-plugin
void SignalTestResult(NPP instance) {
  NPObject *window_obj = NULL;
  g_browser->getvalue(instance, NPNVWindowNPObject, &window_obj);
  if (!window_obj) {
    NOTREACHED();
    return;
  }

  std::string script = "javascript:onSuccess()";
  NPString script_string;
  script_string.UTF8Characters = script.c_str();
  script_string.UTF8Length =
      static_cast<unsigned int>(script.length());

  NPVariant result_var;
  NPError result = g_browser->evaluate(instance, window_obj,
                                       &script_string, &result_var);
  g_browser->releaseobject(window_obj);
}

}  // namespace CHROMIUM_DefaultPluginTest

NPError NPP_New(NPMIMEType plugin_type, NPP instance, uint16 mode, int16 argc,
                char* argn[], char* argv[], NPSavedData* saved) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  // We don't want the null plugin to work in the following cases:-
  // 1. Test-shell
  // 2. The plugin is running in the renderer process.
  if (webkit_glue::IsPluginRunningInRendererProcess()) {
    if (!base::strcasecmp(plugin_type,
        "application/chromium-test-default-plugin")) {
      SignalTestResult(instance);
      return NPERR_NO_ERROR;
    }
    return NPERR_GENERIC_ERROR;
  }


  PluginInstallerImpl* plugin_impl = new PluginInstallerImpl(mode);
  plugin_impl->Initialize(GetCurrentModuleHandle(), instance, plugin_type, argc,
                          argn, argv);
  instance->pdata = reinterpret_cast<void*>(plugin_impl);
  return NPERR_NO_ERROR;
}

NPError NPP_Destroy(NPP instance, NPSavedData** save) {
  PluginInstallerImpl* plugin_impl =
    reinterpret_cast<PluginInstallerImpl*>(instance->pdata);

  if (plugin_impl) {
    plugin_impl->Shutdown();
    delete plugin_impl;
  }

  return NPERR_NO_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow* window_info) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if (window_info == NULL) {
    NOTREACHED();
    return NPERR_GENERIC_ERROR;
  }

  // We may still get a NPP_SetWindow call from webkit in the
  // single-process/test_shell case, as this gets invoked in the plugin
  // destruction code path.
  if (webkit_glue::IsPluginRunningInRendererProcess()) {
    return NPERR_GENERIC_ERROR;
  }

  PluginInstallerImpl* plugin_impl =
      reinterpret_cast<PluginInstallerImpl*>(instance->pdata);

  if (plugin_impl == NULL) {
    NOTREACHED();
    return NPERR_GENERIC_ERROR;
  }

  HWND window_handle = reinterpret_cast<HWND>(window_info->window);
  if (!plugin_impl->SetWindow(window_handle)) {
    delete plugin_impl;
    return NPERR_GENERIC_ERROR;
  }

  return NPERR_NO_ERROR;
}

NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream,
                      NPBool seekable, uint16* stype) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  PluginInstallerImpl* plugin_impl =
    reinterpret_cast<PluginInstallerImpl*>(instance->pdata);

  if (!plugin_impl) {
    NOTREACHED();
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  plugin_impl->NewStream(stream);
  return NPERR_NO_ERROR;
}

NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  PluginInstallerImpl* plugin_impl =
      reinterpret_cast<PluginInstallerImpl*>(instance->pdata);

  if (!plugin_impl) {
    NOTREACHED();
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  plugin_impl->DestroyStream(stream, reason);
  return NPERR_NO_ERROR;
}

int32 NPP_WriteReady(NPP instance, NPStream* stream) {
  if (instance == NULL)
    return 0;

  PluginInstallerImpl* plugin_impl =
      reinterpret_cast<PluginInstallerImpl*>(instance->pdata);

  if (!plugin_impl) {
    NOTREACHED();
    return 0;
  }

  if (plugin_impl->WriteReady(stream))
    return 0x7FFFFFFF;
  return 0;
}

int32 NPP_Write(NPP instance, NPStream* stream, int32 offset, int32 len,
                void* buffer) {
  if (instance == NULL)
    return 0;

  PluginInstallerImpl* plugin_impl =
      reinterpret_cast<PluginInstallerImpl*>(instance->pdata);

  if (!plugin_impl) {
    NOTREACHED();
    return 0;
  }

  return plugin_impl->Write(stream, offset, len, buffer);
}

void NPP_URLNotify(NPP instance, const char* url, NPReason reason,
                   void* notifyData) {
  if (instance != NULL) {
    PluginInstallerImpl* plugin_impl =
        reinterpret_cast<PluginInstallerImpl*>(instance->pdata);

    if (!plugin_impl) {
      NOTREACHED();
      return;
    }

    plugin_impl->URLNotify(url, reason);
  }
}

int16 NPP_HandleEvent(NPP instance, void* event) {
  if (instance == NULL)
    return 0;

  PluginInstallerImpl* plugin_impl =
      reinterpret_cast<PluginInstallerImpl*>(instance->pdata);

  return plugin_impl->NPP_HandleEvent(event);
}

}   // default_plugin
