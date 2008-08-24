// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "chrome/plugin/plugin_channel.h"

#include "chrome/common/plugin_messages.h"
#include "base/string_util.h"
#include "chrome/plugin/plugin_thread.h"
#include "chrome/plugin/plugin_process.h"

PluginChannel* PluginChannel::GetPluginChannel(
    int process_id, HANDLE renderer_handle, MessageLoop* ipc_message_loop) {
  // map renderer's process id to a (single) channel to that process
  std::wstring channel_name = StringPrintf(
      L"%d.r%d", GetCurrentProcessId(), process_id);

  PluginChannelBase* result = PluginChannelBase::GetChannel(
      channel_name,
      IPC::Channel::MODE_SERVER,
      ClassFactory,
      ipc_message_loop,
      false);

  PluginChannel* channel = static_cast<PluginChannel*>(result);
  if (channel && !channel->renderer_handle())
    channel->renderer_handle_.Set(renderer_handle);

  return channel;
}

PluginChannel::PluginChannel() : in_send_(0) {
  SendUnblockingOnlyDuringDispatch();
  PluginProcess::AddRefProcess();
}

PluginChannel::~PluginChannel() {
  PluginProcess::ReleaseProcess();
}

bool PluginChannel::Send(IPC::Message* msg) {
  in_send_++;
  bool result = PluginChannelBase::Send(msg);
  in_send_--;
  return result;
}

void PluginChannel::OnControlMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PluginChannel, msg)
    IPC_MESSAGE_HANDLER(PluginMsg_CreateInstance, OnCreateInstance)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PluginMsg_DestroyInstance, OnDestroyInstance)
    IPC_MESSAGE_HANDLER(PluginMsg_GenerateRouteID, OnGenerateRouteID)
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

void PluginChannel::OnCreateInstance(const std::string& mime_type,
                                     int* instance_id) {
  *instance_id = GenerateRouteID();
  scoped_refptr<WebPluginDelegateStub> stub = new WebPluginDelegateStub(
      mime_type, *instance_id, this);
  AddRoute(*instance_id, stub, false);
  plugin_stubs_.push_back(stub);
}

void PluginChannel::OnDestroyInstance(int instance_id,
                                      IPC::Message* reply_msg) {
  for (size_t i = 0; i < plugin_stubs_.size(); ++i) {
    if (plugin_stubs_[i]->instance_id() == instance_id) {
      plugin_stubs_.erase(plugin_stubs_.begin() + i);
      RemoveRoute(instance_id);
      Send(reply_msg);
      return;
    }
  }

  NOTREACHED() << "Couldn't find WebPluginDelegateStub to destroy";
}

void PluginChannel::OnGenerateRouteID(int* route_id) {
  *route_id = GenerateRouteID();
}

int PluginChannel::GenerateRouteID() {
  static LONG last_id = 0;			
  return InterlockedIncrement(&last_id);
}

void PluginChannel::OnChannelError() {
  renderer_handle_.Set(NULL);
  PluginChannelBase::OnChannelError();
  CleanUp();
}

void PluginChannel::CleanUp() {
  // We need to clean up the stubs so that they call NPPDestroy.  This will
  // also lead to them releasing their reference on this object so that it can
  // be deleted.
  for (size_t i = 0; i < plugin_stubs_.size(); ++i)
    RemoveRoute(plugin_stubs_[i]->instance_id());

  // Need to addref this object temporarily because otherwise removing the last
  // stub will cause the destructor of this object to be called, however at
  // that point plugin_stubs_ will have one element and its destructor will be
  // called twice.
  scoped_refptr<PluginChannel> me(this);

  plugin_stubs_.clear();
}
