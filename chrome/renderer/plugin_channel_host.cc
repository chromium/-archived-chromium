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

#include "chrome/renderer/plugin_channel_host.h"

#include "chrome/common/plugin_messages.h"

// A simple MessageFilter that will ignore all messages and respond to sync
// messages with an error when is_listening_ is false.
class IsListeningFilter : public IPC::ChannelProxy::MessageFilter {
 public:
  IsListeningFilter() {}

  // MessageFilter overrides
  virtual void OnFilterRemoved() {}
  virtual void OnFilterAdded(IPC::Channel* channel) { channel_ = channel;  }
  virtual bool OnMessageReceived(const IPC::Message& message);

  static bool is_listening_;

 private:
  IPC::Channel* channel_;

  DISALLOW_EVIL_CONSTRUCTORS(IsListeningFilter);
};

bool IsListeningFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  if (!IsListeningFilter::is_listening_) {
    // reply to synchronous messages with an error (so they don't block while
    // we're not listening)
    if (message.is_sync()) {
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
      reply->set_reply_error();
      channel_->Send(reply);
    }
    handled = true;
  } else {
    handled = false;
  }
  return handled;
}

// static
bool IsListeningFilter::is_listening_ = true;

// static
void PluginChannelHost::SetListening(bool flag) {
  IsListeningFilter::is_listening_ = flag;
}

PluginChannelHost* PluginChannelHost::GetPluginChannelHost(
    const std::wstring& channel_name, MessageLoop* ipc_message_loop) {
  PluginChannelHost* result =
      static_cast<PluginChannelHost*>(PluginChannelBase::GetChannel(
          channel_name,
          IPC::Channel::MODE_CLIENT,
          ClassFactory,
          ipc_message_loop,
          true));
  return result;
}

PluginChannelHost::PluginChannelHost() {
}

PluginChannelHost::~PluginChannelHost() {
}

bool PluginChannelHost::Init(MessageLoop* ipc_message_loop,
                             bool create_pipe_now) {
  bool ret = PluginChannelBase::Init(ipc_message_loop, create_pipe_now);
  is_listening_filter_ = new IsListeningFilter;
  channel_->AddFilter(is_listening_filter_);
  return ret;
}

int PluginChannelHost::GenerateRouteID() {
  int route_id = MSG_ROUTING_NONE;
  Send(new PluginMsg_GenerateRouteID(&route_id));

  return route_id;
}

void PluginChannelHost::AddRoute(int route_id,
                                 IPC::Channel::Listener* listener,
                                 bool npobject) {
  PluginChannelBase::AddRoute(route_id, listener, npobject);

  if (!npobject)
    proxies_[route_id] = listener;
}

void PluginChannelHost::RemoveRoute(int route_id) {
  proxies_.erase(route_id);
  PluginChannelBase::RemoveRoute(route_id);
}

void PluginChannelHost::OnChannelError() {
  PluginChannelBase::OnChannelError();

  for (ProxyMap::iterator iter = proxies_.begin();
       iter != proxies_.end(); iter++) {
    iter->second->OnChannelError();
  }

  proxies_.clear();
}

