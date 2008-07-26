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
// A class that receives IPC messages from an NPObjectProxy and calls the real
// NPObject.

#ifndef CHROME_PLUGIN_NPOBJECT_STUB_H__
#define CHROME_PLUGIN_NPOBJECT_STUB_H__

#include "base/ref_counted.h"
#include "chrome/common/ipc_channel.h"

class PluginChannelBase;
class WebPluginDelegateProxy;
struct NPIdentifier_Param;
struct NPObject;
struct NPVariant_Param;

// This wraps an NPObject and converts IPC messages from NPObjectProxy to calls
// to the object.  The results are marshalled back.  See npobject_proxy.h for
// more information.
class NPObjectStub : public IPC::Channel::Listener,
                     public IPC::Message::Sender {
 public:
  NPObjectStub(NPObject* npobject, PluginChannelBase* channel, int route_id);
  ~NPObjectStub();

  // IPC::Message::Sender implementation:
  bool Send(IPC::Message* msg);

  // Called when the plugin widget that this NPObject came from is destroyed.
  // This is needed because the renderer calls NPN_DeallocateObject on the
  // window script object on destruction to avoid leaks.
  void set_invalid() { valid_ = false; }
  void set_proxy(WebPluginDelegateProxy* proxy) {
    web_plugin_delegate_proxy_ = proxy;
  }

 private:
  // IPC::Channel::Listener implementation:
  void OnMessageReceived(const IPC::Message& message);
  void OnChannelError();

  // message handlers
  void OnRelease(IPC::Message* reply_msg);
  void OnHasMethod(const NPIdentifier_Param& name,
                   bool* result);
  void OnInvoke(bool is_default,
                const NPIdentifier_Param& method,
                const std::vector<NPVariant_Param>& args,
                IPC::Message* reply_msg);
  void OnHasProperty(const NPIdentifier_Param& name,
                     bool* result);
  void OnGetProperty(const NPIdentifier_Param& name,
                     NPVariant_Param* property,
                     bool* result);
  void OnSetProperty(const NPIdentifier_Param& name,
                     const NPVariant_Param& property,
                     bool* result);
  void OnRemoveProperty(const NPIdentifier_Param& name,
                        bool* result);
  void OnInvalidate();
  void OnEnumeration(std::vector<NPIdentifier_Param>* value,
                     bool* result);
  void OnEvaluate(const std::string& script, IPC::Message* reply_msg);
  void OnSetException(const std::string& message);

 private:
  NPObject* npobject_;
  int route_id_;
  scoped_refptr<PluginChannelBase> channel_;

  // These variables are used to ensure that the window script object is not
  // called after the plugin widget has gone away, as the frame manually
  // deallocates it and ignores the refcount to avoid leaks.
  bool valid_;
  WebPluginDelegateProxy* web_plugin_delegate_proxy_;
};

#endif  // CHROME_PLUGIN_NPOBJECT_STUB_H__
