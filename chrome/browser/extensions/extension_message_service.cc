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

// Since we have 2 ports for every channel, we just index channels by half the
// port ID.
#define GET_CHANNEL_ID(port_id) (port_id / 2)

// Port1 is always even, port2 is always odd.
#define IS_PORT1_ID(port_id) ((port_id & 1) == 0)

// Change even to odd and vice versa, to get the other side of a given channel.
#define GET_OPPOSITE_PORT_ID(source_port_id) (source_port_id ^ 1)

ExtensionMessageService* ExtensionMessageService::GetInstance() {
  return Singleton<ExtensionMessageService>::get();
}

ExtensionMessageService::ExtensionMessageService()
    : next_port_id_(0) {
}

void ExtensionMessageService::RegisterExtension(
    const std::string& extension_id, int render_process_id) {
  AutoLock lock(renderers_lock_);
  // TODO(mpcomplete): We need to ensure an extension always ends up in a single
  // process.  I think this means having an ExtensionProcessManager which holds
  // a BrowsingContext for each extension.
  //DCHECK(process_ids_.find(extension_id) == process_ids_.end());
  process_ids_[extension_id] = render_process_id;
}

int ExtensionMessageService::OpenChannelToExtension(
    const std::string& extension_id, ResourceMessageFilter* source) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));

  // Lookup the targeted extension process.
  ResourceMessageFilter* dest = NULL;
  {
    AutoLock lock(renderers_lock_);
    ProcessIDMap::iterator process_id = process_ids_.find(extension_id);
    if (process_id == process_ids_.end())
      return -1;

    RendererMap::iterator renderer = renderers_.find(process_id->second);
    if (renderer == renderers_.end())
      return -1;
    dest = renderer->second;
  }

  // Create a channel ID for both sides of the channel.
  int port1_id = next_port_id_++;
  int port2_id = next_port_id_++;
  DCHECK(IS_PORT1_ID(port1_id));
  DCHECK(GET_OPPOSITE_PORT_ID(port1_id) == port2_id);
  DCHECK(GET_OPPOSITE_PORT_ID(port2_id) == port1_id);
  DCHECK(GET_CHANNEL_ID(port1_id) == GET_CHANNEL_ID(port2_id));

  MessageChannel channel;
  channel.port1 = source;
  channel.port2 = dest;
  channels_[GET_CHANNEL_ID(port1_id)] = channel;

  // Send each process the id for the opposite port.
  dest->Send(new ViewMsg_ExtensionHandleConnect(port1_id));
  return port2_id;
}

void ExtensionMessageService::PostMessageFromRenderer(
    int port_id, const std::string& message, ResourceMessageFilter* source) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));

  // Look up the channel by port1's ID.
  MessageChannelMap::iterator iter =
      channels_.find(GET_CHANNEL_ID(port_id));
  if (iter == channels_.end())
    return;
  MessageChannel& channel = iter->second;

  // Figure out which port the ID corresponds to.
  ResourceMessageFilter* dest = NULL;
  if (IS_PORT1_ID(port_id)) {
    dest = channel.port1;
    DCHECK(source == channel.port2);
  } else {
    dest = channel.port2;
    DCHECK(source == channel.port1);
  }

  int source_port_id = GET_OPPOSITE_PORT_ID(port_id);
  dest->Send(new ViewMsg_ExtensionHandleMessage(message, source_port_id));
}

void ExtensionMessageService::RendererReady(ResourceMessageFilter* renderer) {
  AutoLock lock(renderers_lock_);
  DCHECK(renderers_.find(renderer->GetProcessId()) == renderers_.end());
  renderers_[renderer->GetProcessId()] = renderer;
}

void ExtensionMessageService::RendererShutdown(
    ResourceMessageFilter* renderer) {
  {
    AutoLock lock(renderers_lock_);
    DCHECK(renderers_.find(renderer->GetProcessId()) != renderers_.end());
    renderers_.erase(renderer->GetProcessId());
  }

  // Close any channels that share this filter.
  // TODO(mpcomplete): should we notify the other side of the port?
  for (MessageChannelMap::iterator it = channels_.begin();
       it != channels_.end(); ) {
    MessageChannelMap::iterator current = it++;
    if (current->second.port1 == renderer || current->second.port2 == renderer)
      channels_.erase(current);
  }
}
