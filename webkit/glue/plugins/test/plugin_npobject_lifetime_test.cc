// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/test/plugin_npobject_lifetime_test.h"

namespace NPAPIClient {

const int kNPObjectLifetimeTimer = 100;
const int kNPObjectLifetimeTimerElapse = 2000;

NPObject* NPObjectLifetimeTestInstance2::plugin_instance_object_ = NULL;

NPObjectDeletePluginInNPN_Evaluate*
    NPObjectDeletePluginInNPN_Evaluate::g_npn_evaluate_test_instance_ = NULL;

NPObjectLifetimeTest::NPObjectLifetimeTest(NPP id,
                                           NPNetscapeFuncs *host_functions)
    : PluginTest(id, host_functions),
      other_plugin_instance_object_(NULL) {
}

NPError NPObjectLifetimeTest::SetWindow(NPWindow* pNPWindow) {
  HWND window_handle = reinterpret_cast<HWND>(pNPWindow->window);
  if (!::GetProp(window_handle, L"Plugin_Instance")) {
    ::SetProp(window_handle, L"Plugin_Instance", this);
    // We attempt to retreive the NPObject for the plugin instance identified
    // by the NPObjectLifetimeTestInstance2 class as it may not have been
    // instantiated yet.
    SetTimer(window_handle, kNPObjectLifetimeTimer, kNPObjectLifetimeTimerElapse,
             TimerProc);
  }
  return NPERR_NO_ERROR;
}

void CALLBACK NPObjectLifetimeTest::TimerProc(
    HWND window, UINT message, UINT timer_id,
    unsigned long elapsed_milli_seconds) {

  KillTimer(window, kNPObjectLifetimeTimer);
  NPObjectLifetimeTest* this_instance =
      reinterpret_cast<NPObjectLifetimeTest*>
          (::GetProp(window, L"Plugin_Instance"));

  this_instance->other_plugin_instance_object_ =
      NPObjectLifetimeTestInstance2::plugin_instance_object_;
  this_instance->HostFunctions()->retainobject(
      this_instance->other_plugin_instance_object_);

  NPString script;
  script.UTF8Characters = "javascript:DeleteSecondPluginInstance()";
  script.UTF8Length = static_cast<uint32_t>(strlen(script.UTF8Characters));

  this_instance->HostFunctions()->geturlnotify(
      this_instance->id(), "javascript:DeleteSecondPluginInstance()", NULL,
      reinterpret_cast<void*>(1));
}

void NPObjectLifetimeTest::URLNotify(const char* url, NPReason reason,
                                     void* data) {
  // Create a "location" identifier.
  NPIdentifier identifier = HostFunctions()->getstringidentifier("location");
  // Declare a local variant value.
  NPVariant variantValue;
  // Get the location property from the window object (which is another object).
  bool b1 = HostFunctions()->getproperty(id(), other_plugin_instance_object_,
                                        identifier, &variantValue );
  HostFunctions()->releaseobject(other_plugin_instance_object_);
  other_plugin_instance_object_ = NULL;
  // If this test failed, then we'd have crashed by now.
  SignalTestCompleted();
}

NPObjectLifetimeTestInstance2::NPObjectLifetimeTestInstance2(
    NPP id, NPNetscapeFuncs *host_functions)
    : PluginTest(id, host_functions) {
}

NPObjectLifetimeTestInstance2::~NPObjectLifetimeTestInstance2() {
  if (plugin_instance_object_) {
    HostFunctions()->releaseobject(plugin_instance_object_);
    plugin_instance_object_ = NULL;
  }
}

NPError NPObjectLifetimeTestInstance2::SetWindow(NPWindow* pNPWindow) {
  if (!plugin_instance_object_) {
    if (!HostFunctions()->getvalue(id(), NPNVWindowNPObject,
                                   &plugin_instance_object_)) {
      SetError("Failed to get NPObject for plugin instance2");
      SignalTestCompleted();
      return NPERR_GENERIC_ERROR;
    }
  }

  return NPERR_NO_ERROR;
}


NPObjectDeletePluginInNPN_Evaluate::NPObjectDeletePluginInNPN_Evaluate(
    NPP id, NPNetscapeFuncs *host_functions)
    : PluginTest(id, host_functions),
      plugin_instance_object_(NULL),
      npn_evaluate_timer_proc_set_(false) {
  g_npn_evaluate_test_instance_ = this;
}

NPObjectDeletePluginInNPN_Evaluate::~NPObjectDeletePluginInNPN_Evaluate() {
  if (plugin_instance_object_) {
    HostFunctions()->releaseobject(plugin_instance_object_);
    plugin_instance_object_ = NULL;
  }
}

NPError NPObjectDeletePluginInNPN_Evaluate::SetWindow(NPWindow* np_window) {
  HWND window_handle = reinterpret_cast<HWND>(np_window->window);
  // We setup a timerproc to invoke NPN_Evaluate to destroy this plugin
  // instance. This is to ensure that we don't destroy the plugin instance
  // while it is being used in webkit as this leads to crashes and is a
  // more accurate representation of the renderer crash as described in
  // http://b/issue?id=1134683.
  if (!npn_evaluate_timer_proc_set_) {
    npn_evaluate_timer_proc_set_ = true;
    SetTimer(window_handle, kNPObjectLifetimeTimer, kNPObjectLifetimeTimerElapse,
             TimerProc);
  }

  return NPERR_NO_ERROR;
}

void CALLBACK NPObjectDeletePluginInNPN_Evaluate::TimerProc(
    HWND window, UINT message, UINT timer_id,
    unsigned long elapsed_milli_seconds) {

  KillTimer(window, kNPObjectLifetimeTimer);
  NPObject *window_obj = NULL;
  g_npn_evaluate_test_instance_->HostFunctions()->getvalue(
      g_npn_evaluate_test_instance_->id(), NPNVWindowNPObject,
      &window_obj);

  if (!window_obj) {
    g_npn_evaluate_test_instance_->SetError(
        "Failed to get NPObject for plugin instance2");
    g_npn_evaluate_test_instance_->SignalTestCompleted();
    return;
  }

  std::string script = "javascript:DeletePluginWithinScript()";
  NPString script_string;
  script_string.UTF8Characters = script.c_str();
  script_string.UTF8Length =
      static_cast<unsigned int>(script.length());

  NPVariant result_var;
  NPError result = g_npn_evaluate_test_instance_->HostFunctions()->evaluate(
                      g_npn_evaluate_test_instance_->id(), window_obj,
                      &script_string, &result_var);
  // If this test failed we would have crashed by now.
}

} // namespace NPAPIClient

