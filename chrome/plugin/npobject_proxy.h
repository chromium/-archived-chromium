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
//
// A proxy for NPObject that sends all calls to the object to an NPObjectStub
// running in a different process.

#ifndef CHROME_PLUGIN_NPOBJECT_PROXY_H__
#define CHROME_PLUGIN_NPOBJECT_PROXY_H__

#include "base/ref_counted.h"
#include "bindings/npruntime.h"
#include "chrome/common/ipc_channel.h"

class PluginChannelBase;
struct NPObject;
struct NPVariant_Param;

// When running a plugin in a different process from the renderer, we need to
// proxy calls to NPObjects across process boundaries.  This happens both ways,
// as a plugin can get an NPObject for the window, and a page can get an
// NPObject for the plugin.  In the process that interacts with the NPobject we
// give it an NPObjectProxy instead.  All calls to it are sent across an IPC
// channel (specifically, a PluginChannelBase).  The NPObjectStub on the other
// side translates the IPC messages into calls to the actual NPObject, and
// returns the marshalled result.
class NPObjectProxy : public IPC::Channel::Listener,
                      public IPC::Message::Sender {
 public:
  ~NPObjectProxy();

  static NPObject* Create(PluginChannelBase* channel,
                          int route_id,
                          void* npobject_ptr,
                          HANDLE modal_dialog_event);

  // IPC::Message::Sender implementation:
  bool Send(IPC::Message* msg);
  int route_id() { return route_id_; }
  PluginChannelBase* channel() { return channel_; }

  // Returns the real NPObject's pointer (obviously only valid in the other
  // process).
  void* npobject_ptr() { return npobject_ptr_; }

  // The next 8 functions are called on NPObjects from the plugin and browser.
  static bool NPHasMethod(NPObject *obj,
                          NPIdentifier name);
  static bool NPInvoke(NPObject *obj,
                       NPIdentifier name,
                       const NPVariant *args,
                       uint32_t arg_count,
                       NPVariant *result);
  static bool NPInvokeDefault(NPObject *npobj,
                              const NPVariant *args,
                              uint32_t arg_count,
                              NPVariant *result);
  static bool NPHasProperty(NPObject *obj,
                            NPIdentifier name);
  static bool NPGetProperty(NPObject *obj,
                            NPIdentifier name,
                            NPVariant *result);
  static bool NPSetProperty(NPObject *obj,
                            NPIdentifier name,
                            const NPVariant *value);
  static bool NPRemoveProperty(NPObject *obj,
                               NPIdentifier name);
  static bool NPNEnumerate(NPObject *obj,
                           NPIdentifier **value,
                           uint32_t *count);

  // The next two functions are only called on NPObjects from the browser.
  static bool NPNEvaluate(NPP npp,
                          NPObject *obj,
                          NPString *script,
                          NPVariant *result);

  static void NPNSetException(NPObject *obj,
                              const NPUTF8 *message);

  static bool NPInvokePrivate(NPP npp,
                              NPObject *obj,
                              bool is_default,
                              NPIdentifier name,
                              const NPVariant *args,
                              uint32_t arg_count,
                              NPVariant *result);

  static NPObjectProxy* GetProxy(NPObject* object);
  static const NPClass* npclass() { return &npclass_proxy_; }

 private:
  NPObjectProxy(PluginChannelBase* channel,
                int route_id,
                void* npobject_ptr,
                HANDLE modal_dialog_event);

  // IPC::Channel::Listener implementation:
  void OnMessageReceived(const IPC::Message& msg);
  void OnChannelError();

  static NPObject* NPAllocate(NPP, NPClass*);
  static void NPDeallocate(NPObject* npObj);

  // This function is only caled on NPObjects from the plugin.
  static void NPPInvalidate(NPObject *obj);
  static NPClass npclass_proxy_;

  int route_id_;
  void* npobject_ptr_;
  scoped_refptr<PluginChannelBase> channel_;
  HANDLE modal_dialog_event_;
};

#endif  // CHROME_PLUGIN_NPOBJECT_PROXY_H__
