// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_NPOBJECT_LIFETIME_TEST_H__
#define WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_NPOBJECT_LIFETIME_TEST_H__

#include "webkit/glue/plugins/test/plugin_test.h"

namespace NPAPIClient {

// The NPObjectLifeTime class tests the case where a plugin has an NPObject
// which points to a different plugin instance on a different frame in the 
// page and whether refcounts on this npobject are valid when the source frame
// is destroyed.
class NPObjectLifetimeTest : public PluginTest {
 public:
  // Constructor.
  NPObjectLifetimeTest(NPP id, NPNetscapeFuncs *host_functions);

  // NPAPI SetWindow handler.
  virtual NPError SetWindow(NPWindow* pNPWindow);

  virtual void  URLNotify(const char* url, NPReason reason, void* data);

 protected:
  NPObject* other_plugin_instance_object_;
  static void CALLBACK TimerProc(HWND window, UINT message, UINT timer_id, 
                                 unsigned long elapsed_milli_seconds);
};

// The NPObjectLifetimeTestInstance2 class represents the plugin instance
// which is deleted by the NPObjectLifeTime class via a javascript function.
class NPObjectLifetimeTestInstance2 : public PluginTest {
 public:
  // Constructor.
  NPObjectLifetimeTestInstance2(NPP id, NPNetscapeFuncs *host_functions);
  ~NPObjectLifetimeTestInstance2();

  // NPAPI SetWindow handler.
  virtual NPError SetWindow(NPWindow* pNPWindow);
 protected:
  static NPObject* plugin_instance_object_;
  friend class NPObjectLifetimeTest;

};

// The NPObjectLifeTime class tests the case where a plugin instance is
// destroyed in NPN_Evaluate
class NPObjectDeletePluginInNPN_Evaluate : public PluginTest {
 public:
  // Constructor.
  NPObjectDeletePluginInNPN_Evaluate(NPP id, NPNetscapeFuncs *host_functions);
  ~NPObjectDeletePluginInNPN_Evaluate();

  // NPAPI SetWindow handler.
  virtual NPError SetWindow(NPWindow* pNPWindow);

 protected:
  NPObject* plugin_instance_object_;
  static void CALLBACK TimerProc(HWND window, UINT message, UINT timer_id, 
                                 unsigned long elapsed_milli_seconds);
 private:
  bool npn_evaluate_timer_proc_set_;
  static NPObjectDeletePluginInNPN_Evaluate* g_npn_evaluate_test_instance_;
};

} // namespace NPAPIClient

#endif  // WEBKIT_GLUE_PLUGINS_TEST_PLUGIN_NPOBJECT_LIFETIME_TEST_H__

