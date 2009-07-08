// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/plugin/npobject_stub.h"

#include "chrome/common/child_process_logging.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/plugin/npobject_util.h"
#include "chrome/plugin/plugin_channel_base.h"
#include "chrome/renderer/webplugin_delegate_proxy.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"

NPObjectStub::NPObjectStub(
    NPObject* npobject,
    PluginChannelBase* channel,
    int route_id,
    base::WaitableEvent* modal_dialog_event,
    const GURL& page_url)
    : npobject_(npobject),
      channel_(channel),
      route_id_(route_id),
      valid_(true),
      web_plugin_delegate_proxy_(NULL),
      modal_dialog_event_(modal_dialog_event),
      page_url_(page_url) {
  channel_->AddRoute(route_id, this, true);

  // We retain the object just as PluginHost does if everything was in-process.
  NPN_RetainObject(npobject_);
}

NPObjectStub::~NPObjectStub() {
  if (web_plugin_delegate_proxy_)
    web_plugin_delegate_proxy_->DropWindowScriptObject();

  channel_->RemoveRoute(route_id_);
  if (npobject_ && valid_)
    NPN_ReleaseObject(npobject_);
}

bool NPObjectStub::Send(IPC::Message* msg) {
  return channel_->Send(msg);
}

void NPObjectStub::OnMessageReceived(const IPC::Message& msg) {
  child_process_logging::ScopedActiveURLSetter url_setter(page_url_);

  if (!valid_) {
    if (msg.is_sync()) {
      // The object could be garbage because the frame has gone away, so
      // just send an error reply to the caller.
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
      reply->set_reply_error();
      Send(reply);
    }

    return;
  }

  IPC_BEGIN_MESSAGE_MAP(NPObjectStub, msg)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NPObjectMsg_Release, OnRelease);
    IPC_MESSAGE_HANDLER(NPObjectMsg_HasMethod, OnHasMethod);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NPObjectMsg_Invoke, OnInvoke);
    IPC_MESSAGE_HANDLER(NPObjectMsg_HasProperty, OnHasProperty);
    IPC_MESSAGE_HANDLER(NPObjectMsg_GetProperty, OnGetProperty);
    IPC_MESSAGE_HANDLER(NPObjectMsg_SetProperty, OnSetProperty);
    IPC_MESSAGE_HANDLER(NPObjectMsg_RemoveProperty, OnRemoveProperty);
    IPC_MESSAGE_HANDLER(NPObjectMsg_Invalidate, OnInvalidate);
    IPC_MESSAGE_HANDLER(NPObjectMsg_Enumeration, OnEnumeration);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NPObjectMsg_Construct, OnConstruct);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NPObjectMsg_Evaluate, OnEvaluate);
    IPC_MESSAGE_HANDLER(NPObjectMsg_SetException, OnSetException);
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

void NPObjectStub::OnChannelError() {
  // When the plugin process is shutting down, all the NPObjectStubs
  // destructors are called.  However the plugin dll might have already
  // been released, in which case the NPN_ReleaseObject will cause a crash.
  npobject_ = NULL;
  delete this;
}

void NPObjectStub::OnRelease(IPC::Message* reply_msg) {
  Send(reply_msg);
  delete this;
}

void NPObjectStub::OnHasMethod(const NPIdentifier_Param& name,
                               bool* result) {
  NPIdentifier id = CreateNPIdentifier(name);
  // If we're in the plugin process, then the stub is holding onto an NPObject
  // from the plugin, so all function calls on it need to go through the
  // functions in NPClass.  If we're in the renderer process, then we just call
  // the NPN_ functions.
  if (IsPluginProcess()) {
    if (npobject_->_class->hasMethod) {
      *result = npobject_->_class->hasMethod(npobject_, id);
    } else {
      *result = false;
    }
  } else {
    *result = NPN_HasMethod(0, npobject_, id);
  }
}

void NPObjectStub::OnInvoke(bool is_default,
                            const NPIdentifier_Param& method,
                            const std::vector<NPVariant_Param>& args,
                            IPC::Message* reply_msg) {
  scoped_refptr<PluginChannelBase> local_channel = channel_;
  bool return_value = false;
  NPVariant_Param result_param;
  NPVariant result_var;

  VOID_TO_NPVARIANT(result_var);

  int arg_count = static_cast<int>(args.size());
  NPVariant* args_var = new NPVariant[arg_count];
  for (int i = 0; i < arg_count; ++i) {
    CreateNPVariant(
        args[i], local_channel, &(args_var[i]), modal_dialog_event_,
        page_url_);
  }

  if (is_default) {
    if (IsPluginProcess()) {
      if (npobject_->_class->invokeDefault) {
        return_value = npobject_->_class->invokeDefault(
            npobject_, args_var, arg_count, &result_var);
      } else {
        return_value = false;
      }
    } else {
      return_value = NPN_InvokeDefault(
          0, npobject_, args_var, arg_count, &result_var);
    }
  } else {
    NPIdentifier id = CreateNPIdentifier(method);
    if (IsPluginProcess()) {
      if (npobject_->_class->invoke) {
        return_value = npobject_->_class->invoke(
            npobject_, id, args_var, arg_count, &result_var);
      } else {
        return_value = false;
      }
    } else {
      return_value = NPN_Invoke(
          0, npobject_, id, args_var, arg_count, &result_var);
    }
  }

  for (int i = 0; i < arg_count; ++i)
    NPN_ReleaseVariantValue(&(args_var[i]));

  delete[] args_var;

  CreateNPVariantParam(
      result_var, local_channel, &result_param, true, modal_dialog_event_,
      page_url_);
  NPObjectMsg_Invoke::WriteReplyParams(reply_msg, result_param, return_value);
  local_channel->Send(reply_msg);
}

void NPObjectStub::OnHasProperty(const NPIdentifier_Param& name,
                                 bool* result) {
  NPIdentifier id = CreateNPIdentifier(name);
  if (IsPluginProcess()) {
    if (npobject_->_class->hasProperty) {
      *result = npobject_->_class->hasProperty(npobject_, id);
    } else {
      *result = false;
    }
  } else {
    *result = NPN_HasProperty(0, npobject_, id);
  }
}

void NPObjectStub::OnGetProperty(const NPIdentifier_Param& name,
                                 NPVariant_Param* property,
                                 bool* result) {
  NPVariant result_var;
  VOID_TO_NPVARIANT(result_var);
  NPIdentifier id = CreateNPIdentifier(name);

  if (IsPluginProcess()) {
    if (npobject_->_class->getProperty) {
      *result = npobject_->_class->getProperty(npobject_, id, &result_var);
    } else {
      *result = false;
    }
  } else {
    *result = NPN_GetProperty(0, npobject_, id, &result_var);
  }

  CreateNPVariantParam(
      result_var, channel_, property, true, modal_dialog_event_, page_url_);
}

void NPObjectStub::OnSetProperty(const NPIdentifier_Param& name,
                                 const NPVariant_Param& property,
                                 bool* result) {
  NPVariant result_var;
  VOID_TO_NPVARIANT(result_var);
  NPIdentifier id = CreateNPIdentifier(name);
  NPVariant property_var;
  CreateNPVariant(
      property, channel_, &property_var, modal_dialog_event_, page_url_);

  if (IsPluginProcess()) {
    if (npobject_->_class->setProperty) {
      *result = npobject_->_class->setProperty(npobject_, id, &property_var);
    } else {
      *result = false;
    }
  } else {
    *result = NPN_SetProperty(0, npobject_, id, &property_var);
  }

  NPN_ReleaseVariantValue(&property_var);
}

void NPObjectStub::OnRemoveProperty(const NPIdentifier_Param& name,
                                    bool* result) {
  NPIdentifier id = CreateNPIdentifier(name);
  if (IsPluginProcess()) {
    if (npobject_->_class->removeProperty) {
      *result = npobject_->_class->removeProperty(npobject_, id);
    } else {
      *result = false;
    }
  } else {
    *result = NPN_RemoveProperty(0, npobject_, id);
  }
}

void NPObjectStub::OnInvalidate() {
  if (!IsPluginProcess()) {
    NOTREACHED() << "Should only be called on NPObjects in the plugin";
    return;
  }

  if (!npobject_->_class->invalidate)
    return;

  npobject_->_class->invalidate(npobject_);
}

void NPObjectStub::OnEnumeration(std::vector<NPIdentifier_Param>* value,
                                 bool* result) {
  NPIdentifier* value_np = NULL;
  unsigned int count = 0;
  if (!IsPluginProcess()) {
    *result = NPN_Enumerate(0, npobject_, &value_np, &count);
  } else {
    if (!npobject_->_class->enumerate) {
      *result = false;
      return;
    }

    *result = npobject_->_class->enumerate(npobject_, &value_np, &count);
  }

  if (!*result)
    return;

  for (unsigned int i = 0; i < count; ++i) {
    NPIdentifier_Param param;
    CreateNPIdentifierParam(value_np[i], &param);
    value->push_back(param);
  }

  NPN_MemFree(value_np);
}

void NPObjectStub::OnConstruct(const std::vector<NPVariant_Param>& args,
                               IPC::Message* reply_msg) {
  scoped_refptr<PluginChannelBase> local_channel = channel_;
  bool return_value = false;
  NPVariant_Param result_param;
  NPVariant result_var;

  VOID_TO_NPVARIANT(result_var);

  int arg_count = static_cast<int>(args.size());
  NPVariant* args_var = new NPVariant[arg_count];
  for (int i = 0; i < arg_count; ++i) {
    CreateNPVariant(
        args[i], local_channel, &(args_var[i]), modal_dialog_event_, page_url_);
  }

  if (IsPluginProcess()) {
    if (npobject_->_class->construct) {
      return_value = npobject_->_class->construct(
          npobject_, args_var, arg_count, &result_var);
    } else {
      return_value = false;
    }
  } else {
    return_value = NPN_Construct(
        0, npobject_, args_var, arg_count, &result_var);
  }

  for (int i = 0; i < arg_count; ++i)
    NPN_ReleaseVariantValue(&(args_var[i]));

  delete[] args_var;

  CreateNPVariantParam(
      result_var, local_channel, &result_param, true, modal_dialog_event_,
      page_url_);
  NPObjectMsg_Invoke::WriteReplyParams(reply_msg, result_param, return_value);
  local_channel->Send(reply_msg);
}

void NPObjectStub::OnEvaluate(const std::string& script,
                              bool popups_allowed,
                              IPC::Message* reply_msg) {
  if (IsPluginProcess()) {
    NOTREACHED() << "Should only be called on NPObjects in the renderer";
    return;
  }

  // Grab a reference to the underlying channel, as the NPObjectStub
  // instance can be destroyed in the context of NPN_Evaluate. This
  // can happen if the containing plugin instance is destroyed in
  // NPN_Evaluate.
  scoped_refptr<PluginChannelBase> local_channel = channel_;

  NPVariant result_var;
  NPString script_string;
  script_string.UTF8Characters = script.c_str();
  script_string.UTF8Length = static_cast<unsigned int>(script.length());

  bool return_value = NPN_EvaluateHelper(0, popups_allowed, npobject_,
                                         &script_string, &result_var);

  NPVariant_Param result_param;
  CreateNPVariantParam(
      result_var, local_channel, &result_param, true, modal_dialog_event_,
      page_url_);
  NPObjectMsg_Evaluate::WriteReplyParams(reply_msg, result_param, return_value);
  local_channel->Send(reply_msg);
}

void NPObjectStub::OnSetException(const std::string& message) {
  if (IsPluginProcess()) {
    NOTREACHED() << "Should only be called on NPObjects in the renderer";
    return;
  }

  NPN_SetException(npobject_, message.c_str());
}
