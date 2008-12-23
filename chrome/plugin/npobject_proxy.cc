// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/plugin/npobject_proxy.h"

#include "chrome/common/plugin_messages.h"
#include "chrome/common/win_util.h"
#include "chrome/plugin/npobject_util.h"
#include "chrome/plugin/plugin_channel_base.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/plugins/plugin_instance.h"


struct NPObjectWrapper {
    NPObject object;
    NPObjectProxy* proxy;
};

NPClass NPObjectProxy::npclass_proxy_ = {
  2,
  NPObjectProxy::NPAllocate,
  NPObjectProxy::NPDeallocate,
  NPObjectProxy::NPPInvalidate,
  NPObjectProxy::NPHasMethod,
  NPObjectProxy::NPInvoke,
  NPObjectProxy::NPInvokeDefault,
  NPObjectProxy::NPHasProperty,
  NPObjectProxy::NPGetProperty,
  NPObjectProxy::NPSetProperty,
  NPObjectProxy::NPRemoveProperty,
  NPObjectProxy::NPNEnumerate
};

NPObjectProxy* NPObjectProxy::GetProxy(NPObject* object) {
  NPObjectProxy* proxy = NULL;

  // Wrapper exists only for NPObjects that we had created.
  if (&npclass_proxy_ == object->_class) {
    NPObjectWrapper* wrapper = reinterpret_cast<NPObjectWrapper*>(object);
    proxy = wrapper->proxy;
  }

  return proxy;
}

NPObjectProxy::NPObjectProxy(
    PluginChannelBase* channel,
    int route_id,
    void* npobject_ptr,
    HANDLE modal_dialog_event)
    : channel_(channel),
      route_id_(route_id),
      npobject_ptr_(npobject_ptr),
      modal_dialog_event_(modal_dialog_event) {
  channel_->AddRoute(route_id, this, true);
}

NPObjectProxy::~NPObjectProxy() {
  if (channel_.get()) {
    Send(new NPObjectMsg_Release(route_id_));
    if (channel_.get())
      channel_->RemoveRoute(route_id_);
  }
}

NPObject* NPObjectProxy::Create(PluginChannelBase* channel,
                                int route_id,
                                void* npobject_ptr,
                                HANDLE modal_dialog_event) {
  NPObjectWrapper* obj = reinterpret_cast<NPObjectWrapper*>(
      NPN_CreateObject(0, &npclass_proxy_));
  obj->proxy = new NPObjectProxy(
      channel, route_id, npobject_ptr, modal_dialog_event);

  return reinterpret_cast<NPObject*>(obj);
}

bool NPObjectProxy::Send(IPC::Message* msg) {
  if (channel_.get())
    return channel_->Send(msg);

  delete msg;
  return false;
}

NPObject* NPObjectProxy::NPAllocate(NPP, NPClass*) {
  return reinterpret_cast<NPObject*>(new NPObjectWrapper);
}

void NPObjectProxy::NPDeallocate(NPObject* npObj) {
  NPObjectWrapper* obj = reinterpret_cast<NPObjectWrapper*>(npObj);
  delete obj->proxy;
  delete obj;
}

void NPObjectProxy::OnMessageReceived(const IPC::Message& msg) {
  NOTREACHED();
}

void NPObjectProxy::OnChannelError() {
  // Release our ref count of the plugin channel object, as it addrefs the
  // process.
  channel_ = NULL;
}

bool NPObjectProxy::NPHasMethod(NPObject *obj,
                                NPIdentifier name) {
  bool result = false;
  NPObjectProxy* proxy = GetProxy(obj);

  if (!proxy) {
    return obj->_class->hasMethod(obj, name);
  }

  NPIdentifier_Param name_param;
  CreateNPIdentifierParam(name, &name_param);

  proxy->Send(new NPObjectMsg_HasMethod(proxy->route_id(), name_param, &result));
  return result;
}

bool NPObjectProxy::NPInvoke(NPObject *obj,
                             NPIdentifier name,
                             const NPVariant *args,
                             uint32_t arg_count,
                             NPVariant *result) {
  return NPInvokePrivate(0, obj, false, name, args, arg_count, result);
}

bool NPObjectProxy::NPInvokeDefault(NPObject *npobj,
                                    const NPVariant *args,
                                    uint32_t arg_count,
                                    NPVariant *result) {
  return NPInvokePrivate(0, npobj, true, 0, args, arg_count, result);
}

bool NPObjectProxy::NPInvokePrivate(NPP npp,
                                    NPObject *obj,
                                    bool is_default,
                                    NPIdentifier name,
                                    const NPVariant *args,
                                    uint32_t arg_count,
                                    NPVariant *np_result) {
  NPObjectProxy* proxy = GetProxy(obj);
  if (!proxy) {
    return obj->_class->invoke(obj, name, args, arg_count, np_result);
  }

  bool result = false;
  NPIdentifier_Param name_param;
  if (is_default) {
    // The data won't actually get used, but set it so we don't send random data.
    name_param.identifier = NULL;
  } else {
    CreateNPIdentifierParam(name, &name_param);
  }

  // Note: This instance can get destroyed in the context of
  // Send so addref the channel in this scope.
  scoped_refptr<PluginChannelBase> channel_copy = proxy->channel_;
  std::vector<NPVariant_Param> args_param;
  for (unsigned int i = 0; i < arg_count; ++i) {
    NPVariant_Param param;
    CreateNPVariantParam(args[i], channel_copy, &param, false);
    args_param.push_back(param);
  }

  NPVariant_Param param_result;
  NPObjectMsg_Invoke* msg = new NPObjectMsg_Invoke(
      proxy->route_id_, is_default, name_param, args_param, &param_result,
      &result);

  // If we're in the plugin process and this invoke leads to a dialog box, the
  // plugin will hang the window hierarchy unless we pump the window message
  // queue while waiting for a reply.  We need to do this to simulate what
  // happens when everything runs in-process (while calling MessageBox window
  // messages are pumped).
  msg->set_pump_messages_event(proxy->modal_dialog_event_);

  HANDLE modal_dialog_event_handle = proxy->modal_dialog_event_;

  proxy->Send(msg);

  // Send may delete proxy.
  proxy = NULL;

  if (!result)
    return false;

  CreateNPVariant(
      param_result, channel_copy, np_result, modal_dialog_event_handle);
  return true;
}

bool NPObjectProxy::NPHasProperty(NPObject *obj,
                                  NPIdentifier name) {
  bool result = false;
  NPObjectProxy* proxy = GetProxy(obj);
  if (!proxy) {
    return obj->_class->hasProperty(obj, name);
  }

  NPIdentifier_Param name_param;
  CreateNPIdentifierParam(name, &name_param);

  NPVariant_Param param;
  proxy->Send(new NPObjectMsg_HasProperty(
      proxy->route_id(), name_param, &result));

  // Send may delete proxy.
  proxy = NULL;

  return result;
}

bool NPObjectProxy::NPGetProperty(NPObject *obj,
                                  NPIdentifier name,
                                  NPVariant *np_result) {
  // Please refer to http://code.google.com/p/chromium/issues/detail?id=2556,
  // which was a crash in the XStandard plugin during plugin shutdown. The
  // crash occured because the plugin requests the plugin script object,
  // which fails. The plugin does not check the result of the operation and
  // invokes NPN_GetProperty on a NULL object which lead to the crash. If
  // we observe similar crashes in other methods in the future, these null
  // checks may have to be replicated in the other methods in this class.
  if (obj == NULL)
    return false;

  bool result = false;
  NPObjectProxy* proxy = GetProxy(obj);
  if (!proxy) {
    return obj->_class->getProperty(obj, name, np_result);
  }

  NPIdentifier_Param name_param;
  CreateNPIdentifierParam(name, &name_param);

  NPVariant_Param param;
  HANDLE modal_dialog_event_handle = proxy->modal_dialog_event_;
  scoped_refptr<PluginChannelBase> channel(proxy->channel_);
  proxy->Send(new NPObjectMsg_GetProperty(
      proxy->route_id(), name_param, &param, &result));
  // Send may delete proxy.
  proxy = NULL;
  if (!result)
    return false;

  CreateNPVariant(
      param, channel.get(), np_result, modal_dialog_event_handle);

  return true;
}

bool NPObjectProxy::NPSetProperty(NPObject *obj,
                                  NPIdentifier name,
                                  const NPVariant *value) {
  bool result = false;
  NPObjectProxy* proxy = GetProxy(obj);
  if (!proxy) {
    return obj->_class->setProperty(obj, name, value);
  }

  NPIdentifier_Param name_param;
  CreateNPIdentifierParam(name, &name_param);

  NPVariant_Param value_param;
  CreateNPVariantParam(*value, proxy->channel(), &value_param, false);

  proxy->Send(new NPObjectMsg_SetProperty(
      proxy->route_id(), name_param, value_param, &result));
  // Send may delete proxy.
  proxy = NULL;

  return result;
}

bool NPObjectProxy::NPRemoveProperty(NPObject *obj,
                                     NPIdentifier name) {
  bool result = false;
  NPObjectProxy* proxy = GetProxy(obj);
  if (!proxy) {
    return obj->_class->removeProperty(obj, name);
  }

  NPIdentifier_Param name_param;
  CreateNPIdentifierParam(name, &name_param);

  NPVariant_Param param;
  proxy->Send(new NPObjectMsg_RemoveProperty(
      proxy->route_id(), name_param, &result));
  // Send may delete proxy.
  proxy = NULL;

  return result;
}

void NPObjectProxy::NPPInvalidate(NPObject *obj) {
  bool result = false;
  NPObjectProxy* proxy = GetProxy(obj);
  if (!proxy) {
    return obj->_class->invalidate(obj);
  }

  proxy->Send(new NPObjectMsg_Invalidate(proxy->route_id()));
  // Send may delete proxy.
  proxy = NULL;
}

bool NPObjectProxy::NPNEnumerate(NPObject *obj,
                                 NPIdentifier **value,
                                 uint32_t *count) {
  bool result = false;
  NPObjectProxy* proxy = GetProxy(obj);
  if (!proxy) {
    return obj->_class->enumerate(obj, value, count);
  }

  std::vector<NPIdentifier_Param> value_param;
  proxy->Send(new NPObjectMsg_Enumeration(
      proxy->route_id(), &value_param, &result));
  // Send may delete proxy.
  proxy = NULL;

  if (!result)
    return false;

  *count = static_cast<unsigned int>(value_param.size());
  *value = static_cast<NPIdentifier *>(
      NPN_MemAlloc(sizeof(NPIdentifier) * *count));
  for (unsigned int i = 0; i < *count; ++i)
    (*value)[i] = CreateNPIdentifier(value_param[i]);

  return true;
}

bool NPObjectProxy::NPNEvaluate(NPP npp,
                                NPObject *obj,
                                NPString *script,
                                NPVariant *result_var) {
  bool result = false;
  NPObjectProxy* proxy = GetProxy(obj);
  if (!proxy) {
    return false;
  }

  bool popups_allowed = false;

  if (npp) {
    NPAPI::PluginInstance* plugin_instance =
        reinterpret_cast<NPAPI::PluginInstance*>(npp->ndata);
    if (plugin_instance)
      popups_allowed = plugin_instance->popups_allowed();
  }

  NPVariant_Param result_param;
  std::string script_str = std::string(
      script->UTF8Characters, script->UTF8Length);

  NPObjectMsg_Evaluate* msg = new NPObjectMsg_Evaluate(proxy->route_id(),
                                                       script_str,
                                                       popups_allowed,
                                                       &result_param,
                                                       &result);

  // Please refer to the comments in NPObjectProxy::NPInvokePrivate for
  // the reasoning behind setting the pump messages event in the sync message.
  msg->set_pump_messages_event(proxy->modal_dialog_event_);
  scoped_refptr<PluginChannelBase> channel(proxy->channel_);
  HANDLE modal_dialog_event_handle = proxy->modal_dialog_event_;
  proxy->Send(msg);
  // Send may delete proxy.
  proxy = NULL;
  if (!result)
    return false;

  CreateNPVariant(
      result_param, channel.get(), result_var, modal_dialog_event_handle);
  return true;
}

void NPObjectProxy::NPNSetException(NPObject *obj,
                                    const NPUTF8 *message) {
  NPObjectProxy* proxy = GetProxy(obj);
  if (!proxy) {
    return;
  }

  NPVariant_Param result_param;
  std::string message_str(message);

  proxy->Send(new NPObjectMsg_SetException(proxy->route_id(), message_str));
  // Send may delete proxy.
  proxy = NULL;
}

