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