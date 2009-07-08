// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class that receives IPC messages from an NPObjectProxy and calls the real
// NPObject.

#ifndef CHROME_PLUGIN_NPOBJECT_STUB_H_
#define CHROME_PLUGIN_NPOBJECT_STUB_H_

#include <vector>

#include "base/ref_counted.h"
#include "chrome/common/ipc_channel.h"
#include "googleurl/src/gurl.h"

namespace base {
class WaitableEvent;
}

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
  NPObjectStub(NPObject* npobject,
               PluginChannelBase* channel,
               int route_id,
               base::WaitableEvent* modal_dialog_event,
               const GURL& page_url);
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
  void OnConstruct(const std::vector<NPVariant_Param>& args,
                   IPC::Message* reply_msg);
  void OnEvaluate(const std::string& script, bool popups_allowed,
                  IPC::Message* reply_msg);
  void OnSetException(const std::string& message);

 private:
  NPObject* npobject_;
  scoped_refptr<PluginChannelBase> channel_;
  int route_id_;

  // These variables are used to ensure that the window script object is not
  // called after the plugin widget has gone away, as the frame manually
  // deallocates it and ignores the refcount to avoid leaks.
  bool valid_;
  WebPluginDelegateProxy* web_plugin_delegate_proxy_;

  base::WaitableEvent* modal_dialog_event_;

  // The url of the main frame hosting the plugin.
  GURL page_url_;
};

#endif  // CHROME_PLUGIN_NPOBJECT_STUB_H_
