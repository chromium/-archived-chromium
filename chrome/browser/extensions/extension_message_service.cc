// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_message_service.h"

#include "base/singleton.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extension.h"
#include "chrome/browser/extensions/extension_view.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/common/render_messages.h"

// This class acts as the port to an extension process.  It is basically just
// gymnastics to get access to the IPC::Channel (not the ChannelProxy) belonging
// to an ExtensionView.
// Created on the UI thread, but accessed fully on the IO thread.
class ExtensionMessageService::ExtensionFilter :
    public IPC::ChannelProxy::MessageFilter {
 public:
  ExtensionFilter(const std::string& extension_id, int routing_id) :
      extension_id_(extension_id), routing_id_(routing_id), channel_(NULL) {
  }
  ~ExtensionFilter() {
    ExtensionMessageService::GetInstance()->OnExtensionUnregistered(this);
  }

  virtual void OnFilterAdded(IPC::Channel* channel) {
    channel_ = channel;
    ExtensionMessageService::GetInstance()->OnExtensionRegistered(this);
  }
  virtual void OnChannelClosing() {
    channel_ = NULL;
  }

  bool Send(IPC::Message* message) {
    if (!channel_) {
      delete message;
      return false;
    }
    return channel_->Send(message);
  }

  IPC::Channel* channel() { return channel_; }
  const std::string& extension_id() { return extension_id_; }
  int routing_id() { return routing_id_; }

 private:
  std::string extension_id_;
  int routing_id_;
  IPC::Channel* channel_;
};

ExtensionMessageService* ExtensionMessageService::GetInstance() {
  return Singleton<ExtensionMessageService>::get();
}

ExtensionMessageService::ExtensionMessageService()
    : next_channel_id_(0) {
}

void ExtensionMessageService::RegisterExtensionView(ExtensionView* view) {
  view->render_view_host()->process()->channel()->AddFilter(
      new ExtensionFilter(view->extension()->id(),
                          view->render_view_host()->routing_id()));
}

void ExtensionMessageService::OnExtensionRegistered(ExtensionFilter* filter) {
  extensions_[filter->extension_id()] = filter;
}

void ExtensionMessageService::OnExtensionUnregistered(ExtensionFilter* filter) {
  // TODO(mpcomplete): support multiple filters per extension_id
  //DCHECK(extensions_[filter->extension_id()] == filter);
  extensions_.erase(filter->extension_id());

  // Close any channels that share this filter.
  for (MessageChannelMap::iterator it = channels_.begin();
       it != channels_.end(); ) {
    MessageChannelMap::iterator current = it++;
    if (current->second.extension_port == filter)
      channels_.erase(current);
  }
}

int ExtensionMessageService::OpenChannelToExtension(
    const std::string& extension_id, ResourceMessageFilter* renderer_port) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));

  ExtensionMap::iterator extension_port = extensions_.find(extension_id);
  if (extension_port == extensions_.end())
    return -1;

  int channel_id = next_channel_id_++;
  MessageChannel channel;
  channel.renderer_port = renderer_port;
  channel.extension_port = extension_port->second;
  channels_[channel_id] = channel;

  return channel_id;
}

void ExtensionMessageService::PostMessage(
    int channel_id, const std::string& message) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));

  MessageChannelMap::iterator iter = channels_.find(channel_id);
  if (iter == channels_.end())
    return;

  MessageChannel& channel = iter->second;
  channel.extension_port->Send(new ViewMsg_HandleExtensionMessage(
      channel.extension_port->routing_id(), message, channel_id));
}

void ExtensionMessageService::RendererShutdown(
    ResourceMessageFilter* renderer_port) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));

  // Close any channels that share this filter.
  // TODO(mpcomplete): should we notify the other side of the port?
  for (MessageChannelMap::iterator it = channels_.begin();
       it != channels_.end(); ) {
    MessageChannelMap::iterator current = it++;
    if (current->second.renderer_port == renderer_port)
      channels_.erase(current);
  }
}
