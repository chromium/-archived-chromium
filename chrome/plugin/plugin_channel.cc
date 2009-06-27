// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/plugin/plugin_channel.h"

#include "base/command_line.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "build/build_config.h"
#include "chrome/common/child_process.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/plugin/plugin_thread.h"

#if defined(OS_POSIX)
#include "chrome/common/ipc_channel_posix.h"
#endif

PluginChannel* PluginChannel::GetPluginChannel(
    int process_id, MessageLoop* ipc_message_loop) {
  // map renderer's process id to a (single) channel to that process
  std::string channel_name = StringPrintf(
      "%d.r%d", base::GetCurrentProcId(), process_id);

  return static_cast<PluginChannel*>(PluginChannelBase::GetChannel(
      channel_name,
      IPC::Channel::MODE_SERVER,
      ClassFactory,
      ipc_message_loop,
      false));
}

PluginChannel::PluginChannel()
    : renderer_handle_(0),
#if defined(OS_POSIX)
      renderer_fd_(-1),
#endif
      in_send_(0),
      off_the_record_(false) {
  SendUnblockingOnlyDuringDispatch();
  ChildProcess::current()->AddRefProcess();
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  log_messages_ = command_line->HasSwitch(switches::kLogPluginMessages);
}

PluginChannel::~PluginChannel() {
  if (renderer_handle_)
    base::CloseProcessHandle(renderer_handle_);
#if defined(OS_POSIX)
  // If we still have the FD, close it.
  if (renderer_fd_ != -1) {
    close(renderer_fd_);
  }
#endif
  ChildProcess::current()->ReleaseProcess();
}

bool PluginChannel::Send(IPC::Message* msg) {
  in_send_++;
  if (log_messages_) {
    LOG(INFO) << "sending message @" << msg << " on channel @" << this
              << " with type " << msg->type();
  }
  bool result = PluginChannelBase::Send(msg);
  in_send_--;
  return result;
}

void PluginChannel::OnMessageReceived(const IPC::Message& msg) {
  if (log_messages_) {
    LOG(INFO) << "received message @" << &msg << " on channel @" << this
              << " with type " << msg.type();
  }
  PluginChannelBase::OnMessageReceived(msg);
}

void PluginChannel::OnControlMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PluginChannel, msg)
    IPC_MESSAGE_HANDLER(PluginMsg_CreateInstance, OnCreateInstance)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PluginMsg_DestroyInstance,
                                    OnDestroyInstance)
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
  static int last_id = 0;
  return ++last_id;
}

void PluginChannel::OnChannelConnected(int32 peer_pid) {
  base::ProcessHandle handle;
  if (!base::OpenProcessHandle(peer_pid, &handle)) {
    NOTREACHED();
  }
  renderer_handle_ = handle;
  PluginChannelBase::OnChannelConnected(peer_pid);
}

void PluginChannel::OnChannelError() {
  base::CloseProcessHandle(renderer_handle_);
  renderer_handle_ = 0;
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

bool PluginChannel::Init(MessageLoop* ipc_message_loop, bool create_pipe_now) {
#if defined(OS_POSIX)
  // This gets called when the PluginChannel is initially created. At this
  // point, create the socketpair and assign the plugin side FD to the channel
  // name. Keep the renderer side FD as a member variable in the PluginChannel
  // to be able to transmit it through IPC.
  int plugin_fd;
  IPC::SocketPair(&plugin_fd, &renderer_fd_);
  IPC::AddChannelSocket(channel_name(), plugin_fd);
#endif
  return PluginChannelBase::Init(ipc_message_loop, create_pipe_now);
}
