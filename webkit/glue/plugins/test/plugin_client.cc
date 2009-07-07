// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "webkit/glue/plugins/test/plugin_client.h"
#include "webkit/glue/plugins/test/plugin_arguments_test.h"
#include "webkit/glue/plugins/test/plugin_delete_plugin_in_stream_test.h"
#include "webkit/glue/plugins/test/plugin_get_javascript_url_test.h"
#include "webkit/glue/plugins/test/plugin_geturl_test.h"
#include "webkit/glue/plugins/test/plugin_javascript_open_popup.h"
#include "webkit/glue/plugins/test/plugin_new_fails_test.h"
#include "webkit/glue/plugins/test/plugin_private_test.h"
#include "webkit/glue/plugins/test/plugin_npobject_lifetime_test.h"
#include "webkit/glue/plugins/test/plugin_npobject_proxy_test.h"
#include "webkit/glue/plugins/test/plugin_window_size_test.h"
#if defined(OS_WIN)
#include "webkit/glue/plugins/test/plugin_windowed_test.h"
#endif
#include "webkit/glue/plugins/test/plugin_windowless_test.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"

namespace NPAPIClient {

NPNetscapeFuncs* PluginClient::host_functions_;

NPError PluginClient::GetEntryPoints(NPPluginFuncs* pFuncs) {
  if (pFuncs == NULL)
    return NPERR_INVALID_FUNCTABLE_ERROR;

  if (pFuncs->size < sizeof(NPPluginFuncs))
    return NPERR_INVALID_FUNCTABLE_ERROR;

  pFuncs->version       = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
  pFuncs->newp          = NPP_New;
  pFuncs->destroy       = NPP_Destroy;
  pFuncs->setwindow     = NPP_SetWindow;
  pFuncs->newstream     = NPP_NewStream;
  pFuncs->destroystream = NPP_DestroyStream;
  pFuncs->asfile        = NPP_StreamAsFile;
  pFuncs->writeready    = NPP_WriteReady;
  pFuncs->write         = NPP_Write;
  pFuncs->print         = NPP_Print;
  pFuncs->event         = NPP_HandleEvent;
  pFuncs->urlnotify     = NPP_URLNotify;
  pFuncs->getvalue      = NPP_GetValue;
  pFuncs->setvalue      = NPP_SetValue;
  pFuncs->javaClass     = reinterpret_cast<JRIGlobalRef>(NPP_GetJavaClass);

  return NPERR_NO_ERROR;
}

NPError PluginClient::Initialize(NPNetscapeFuncs* pFuncs) {
  if (pFuncs == NULL) {
    return NPERR_INVALID_FUNCTABLE_ERROR;
  }

  if (static_cast<unsigned char>((pFuncs->version >> 8) & 0xff) >
      NP_VERSION_MAJOR) {
    return NPERR_INCOMPATIBLE_VERSION_ERROR;
  }

  host_functions_ = pFuncs;

  return NPERR_NO_ERROR;
}

NPError PluginClient::Shutdown() {
  return NPERR_NO_ERROR;
}

} // namespace NPAPIClient

extern "C" {
NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16 mode,
                int16 argc, char* argn[], char* argv[], NPSavedData* saved) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  // We look at the test name requested via the plugin arguments.  We match
  // that against a given test and try to instantiate it.

  // lookup the name parameter
  std::string test_name;
  for (int name_index = 0; name_index < argc; name_index++)
    if (base::strcasecmp(argn[name_index], "name") == 0) {
      test_name = argv[name_index];
      break;
    }

  if (test_name.empty())
    return NPERR_GENERIC_ERROR;  // no name found

  NPError ret = NPERR_GENERIC_ERROR;
  bool windowless_plugin = false;

  NPAPIClient::PluginTest *new_test = NULL;
  if (test_name == "arguments") {
    new_test = new NPAPIClient::PluginArgumentsTest(instance,
      NPAPIClient::PluginClient::HostFunctions());
  } else if (test_name == "geturl") {
    new_test = new NPAPIClient::PluginGetURLTest(instance,
      NPAPIClient::PluginClient::HostFunctions());
  } else if (test_name == "npobject_proxy") {
    new_test = new NPAPIClient::NPObjectProxyTest(instance,
      NPAPIClient::PluginClient::HostFunctions());
#if defined(OS_WIN)
  // TODO(port): plugin_windowless_test.*.
  } else if (test_name == "execute_script_delete_in_paint" ||
             test_name == "execute_script_delete_in_mouse_move" ||
             test_name == "delete_frame_test" ||
             test_name == "multiple_instances_sync_calls") {
    new_test = new NPAPIClient::WindowlessPluginTest(instance,
      NPAPIClient::PluginClient::HostFunctions(), test_name);
    windowless_plugin = true;
#endif
  } else if (test_name == "getjavascripturl") {
    new_test = new NPAPIClient::ExecuteGetJavascriptUrlTest(instance,
      NPAPIClient::PluginClient::HostFunctions());
#if defined(OS_WIN)
  // TODO(port): plugin_window_size_test.*.
  } else if (test_name == "checkwindowrect") {
    new_test = new NPAPIClient::PluginWindowSizeTest(instance,
      NPAPIClient::PluginClient::HostFunctions());
#endif
  } else if (test_name == "self_delete_plugin_stream") {
    new_test = new NPAPIClient::DeletePluginInStreamTest(instance,
      NPAPIClient::PluginClient::HostFunctions());
#if defined(OS_WIN)
  // TODO(port): plugin_npobject_lifetime_test.*.
  } else if (test_name == "npobject_lifetime_test") {
    new_test = new NPAPIClient::NPObjectLifetimeTest(instance,
      NPAPIClient::PluginClient::HostFunctions());
  } else if (test_name == "npobject_lifetime_test_second_instance") {
    new_test = new NPAPIClient::NPObjectLifetimeTestInstance2(instance,
      NPAPIClient::PluginClient::HostFunctions());
  } else if (test_name == "new_fails") {
    new_test = new NPAPIClient::NewFailsTest(instance,
        NPAPIClient::PluginClient::HostFunctions());
  } else if (test_name == "npobject_delete_plugin_in_evaluate") {
    new_test = new NPAPIClient::NPObjectDeletePluginInNPN_Evaluate(instance,
      NPAPIClient::PluginClient::HostFunctions());
#endif
  } else if (test_name == "plugin_javascript_open_popup_with_plugin") {
    new_test = new NPAPIClient::ExecuteJavascriptOpenPopupWithPluginTest(
        instance, NPAPIClient::PluginClient::HostFunctions());
  } else if (test_name == "plugin_popup_with_plugin_target") {
    new_test = new NPAPIClient::ExecuteJavascriptPopupWindowTargetPluginTest(
        instance, NPAPIClient::PluginClient::HostFunctions());
  } else if (test_name == "private") {
    new_test = new NPAPIClient::PrivateTest(instance,
      NPAPIClient::PluginClient::HostFunctions());
#if defined(OS_WIN)
  // TODO(port): plugin_windowed_test.*.
  } else if (test_name == "hidden_plugin" ||
             test_name == "create_instance_in_paint" ||
             test_name == "alert_in_window_message") {
    new_test = new NPAPIClient::WindowedPluginTest(instance,
        NPAPIClient::PluginClient::HostFunctions());
#endif
  } else {
    // If we don't have a test case for this, create a
    // generic one which basically never fails.
    LOG(WARNING) << "Unknown test name '" << test_name
                 << "'; using default test.";
    new_test = new NPAPIClient::PluginTest(instance,
      NPAPIClient::PluginClient::HostFunctions());
  }

  if (new_test) {
    ret = new_test->New(mode, argc, (const char**)argn,
                       (const char**)argv, saved);
    if ((ret == NPERR_NO_ERROR) && windowless_plugin) {
      NPAPIClient::PluginClient::HostFunctions()->setvalue(
            instance, NPPVpluginWindowBool, NULL);
    }
  }

  return ret;
}

NPError NPP_Destroy(NPP instance, NPSavedData** save) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  NPAPIClient::PluginTest *plugin =
    (NPAPIClient::PluginTest*)instance->pdata;
  delete plugin;

  // XXXMB - do work here.
  return NPERR_GENERIC_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow* pNPWindow) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if (pNPWindow->window == NULL) {
    return NPERR_NO_ERROR;
  }

  NPAPIClient::PluginTest *plugin =
    (NPAPIClient::PluginTest*)instance->pdata;

  return plugin->SetWindow(pNPWindow);
}

NPError NPP_NewStream(NPP instance, NPMIMEType type,
                      NPStream* stream, NPBool seekable, uint16* stype) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  NPAPIClient::PluginTest *plugin =
    (NPAPIClient::PluginTest*)instance->pdata;

  return plugin->NewStream(type, stream, seekable, stype);
}

int32 NPP_WriteReady(NPP instance, NPStream *stream) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  NPAPIClient::PluginTest *plugin =
    (NPAPIClient::PluginTest*)instance->pdata;

  return plugin->WriteReady(stream);
}

int32 NPP_Write(NPP instance, NPStream *stream, int32 offset,
                 int32 len, void *buffer) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  NPAPIClient::PluginTest *plugin =
    (NPAPIClient::PluginTest*)instance->pdata;

  return plugin->Write(stream, offset, len, buffer);
}

NPError NPP_DestroyStream(NPP instance, NPStream *stream, NPError reason) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  NPAPIClient::PluginTest *plugin =
    (NPAPIClient::PluginTest*)instance->pdata;

  return plugin->DestroyStream(stream, reason);
}

void NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname) {
  if (instance == NULL)
    return;

  NPAPIClient::PluginTest *plugin =
    (NPAPIClient::PluginTest*)instance->pdata;

  return plugin->StreamAsFile(stream, fname);
}

void NPP_Print(NPP instance, NPPrint* printInfo) {
  if (instance == NULL)
    return;

  // XXXMB - do work here.
}

void NPP_URLNotify(NPP instance, const char* url, NPReason reason,
                   void* notifyData) {
  if (instance == NULL)
    return;

  NPAPIClient::PluginTest *plugin =
    (NPAPIClient::PluginTest*)instance->pdata;

  return plugin->URLNotify(url, reason, notifyData);
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  // XXXMB - do work here.
  return NPERR_GENERIC_ERROR;
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value) {
  if (instance == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  // XXXMB - do work here.
  return NPERR_GENERIC_ERROR;
}

int16 NPP_HandleEvent(NPP instance, void* event) {
  if (instance == NULL)
    return 0;

  NPAPIClient::PluginTest *plugin =
    (NPAPIClient::PluginTest*)instance->pdata;

  return plugin->HandleEvent(event);
}

void* NPP_GetJavaClass(void) {
  // XXXMB - do work here.
  return NULL;
}
} // extern "C"
