// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/plugin/plugin_channel_base.h"

#include "base/hash_tables.h"
#include "chrome/common/child_process.h"
#include "chrome/common/ipc_sync_message.h"

typedef base::hash_map<std::string, scoped_refptr<PluginChannelBase> >
    PluginChannelMap;

static PluginChannelMap g_plugin_channels_;


PluginChannelBase* PluginChannelBase::GetChannel(
    const std::string& channel_name, IPC::Channel::Mode mode,
    PluginChannelFactory factory, MessageLoop* ipc_message_loop,
    bool create_pipe_now) {
  scoped_refptr<PluginChannelBase> channel;

  PluginChannelMap::const_iterator iter = g_plugin_channels_.find(channel_name);
  if (iter == g_plugin_channels_.end()) {
    channel = factory();
  } else {
    channel = iter->second;
  }

  DCHECK(channel != NULL);

  if (!channel->channel_valid()) {
    channel->channel_name_ = channel_name;
    channel->mode_ = mode;
    if (channel->Init(ipc_message_loop, create_pipe_now)) {
      g_plugin_channels_[channel_name] = channel;
    } else {
      channel = NULL;
    }
  }

  return channel;
}

PluginChannelBase::PluginChannelBase()
    : plugin_count_(0),
      peer_pid_(0),
      in_remove_route_(false),
      channel_valid_(false),
      in_dispatch_(0),
      send_unblocking_only_during_dispatch_(false) {
}

PluginChannelBase::~PluginChannelBase() {
}

void PluginChannelBase::CleanupChannels() {
  // Make a copy of the references as we can't iterate the map since items will
  // be removed from it as we clean them up.
  std::vector<scoped_refptr<PluginChannelBase> > channels;
  for (PluginChannelMap::const_iterator iter = g_plugin_channels_.begin();
       iter != g_plugin_channels_.end();
       ++iter) {
    channels.push_back(iter->second);
  }

  for (size_t i = 0; i < channels.size(); ++i)
    channels[i]->CleanUp();

  // This will clean up channels added to the map for which subsequent
  // AddRoute wasn't called
  g_plugin_channels_.clear();
}

bool PluginChannelBase::Init(MessageLoop* ipc_message_loop,
                             bool create_pipe_now) {
  channel_.reset(new IPC::SyncChannel(
      channel_name_, mode_, this, NULL, ipc_message_loop, create_pipe_now,
      ChildProcess::current()->GetShutDownEvent()));
  channel_valid_ = true;
  return true;
}

bool PluginChannelBase::Send(IPC::Message* message) {
  if (!channel_.get()) {
    delete message;
    return false;
  }

  if (send_unblocking_only_during_dispatch_ && in_dispatch_ == 0 &&
      message->is_sync()) {
    message->set_unblock(false);
  }

  return channel_->Send(message);
}

int PluginChannelBase::Count() {
  return static_cast<int>(g_plugin_channels_.size());
}

void PluginChannelBase::OnMessageReceived(const IPC::Message& message) {
  // This call might cause us to be deleted, so keep an extra reference to
  // ourself so that we can send the reply and decrement back in_dispatch_.
  scoped_refptr<PluginChannelBase> me(this);

  in_dispatch_++;
  if (message.routing_id() == MSG_ROUTING_CONTROL) {
    OnControlMessageReceived(message);
  } else {
    bool routed = router_.RouteMessage(message);
    if (!routed && message.is_sync()) {
      // The listener has gone away, so we must respond or else the caller will
      // hang waiting for a reply.
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
      reply->set_reply_error();
      Send(reply);
    }
  }
  in_dispatch_--;
}

void PluginChannelBase::OnChannelConnected(int32 peer_pid) {
  peer_pid_ = peer_pid;
}

void PluginChannelBase::AddRoute(int route_id,
                                 IPC::Channel::Listener* listener,
                                 bool npobject) {
  if (npobject) {
    npobject_listeners_[route_id] = listener;
  } else {
    plugin_count_++;
  }

  router_.AddRoute(route_id, listener);
}

void PluginChannelBase::RemoveRoute(int route_id) {
  router_.RemoveRoute(route_id);

  ListenerMap::iterator iter = npobject_listeners_.find(route_id);
  if (iter != npobject_listeners_.end()) {
    // This was an NPObject proxy or stub, it's not involved in the refcounting.

    // If this RemoveRoute call from the NPObject is a result of us calling
    // OnChannelError below, don't call erase() here because that'll corrupt
    // the iterator below.
    if (!in_remove_route_)
      npobject_listeners_.erase(iter);

    return;
  }

  plugin_count_--;
  DCHECK(plugin_count_ >= 0);

  if (!plugin_count_) {
    ListenerMap::iterator npobj_iter = npobject_listeners_.begin();
    in_remove_route_ = true;
    while (npobj_iter != npobject_listeners_.end()) {
      npobj_iter->second->OnChannelError();
      npobj_iter++;
    }
    in_remove_route_ = false;

    PluginChannelMap::iterator iter = g_plugin_channels_.begin();
    while (iter != g_plugin_channels_.end()) {
      if (iter->second == this) {
        g_plugin_channels_.erase(iter);
        return;
      }

      iter++;
    }

    NOTREACHED();
  }
}

void PluginChannelBase::OnControlMessageReceived(const IPC::Message& msg) {
  NOTREACHED() <<
      "should override in subclass if you care about control messages";
}

void PluginChannelBase::OnChannelError() {
  channel_valid_ = false;
}

void PluginChannelBase::SendUnblockingOnlyDuringDispatch() {
  send_unblocking_only_during_dispatch_ = true;
}
