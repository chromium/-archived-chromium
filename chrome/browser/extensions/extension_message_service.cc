// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_message_service.h"

#include "base/singleton.h"
#include "base/values.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extension.h"
#include "chrome/browser/extensions/extension_view.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/stl_util-inl.h"

// Since we have 2 ports for every channel, we just index channels by half the
// port ID.
#define GET_CHANNEL_ID(port_id) ((port_id) / 2)

// Port1 is always even, port2 is always odd.
#define IS_PORT1_ID(port_id) (((port_id) & 1) == 0)

// Change even to odd and vice versa, to get the other side of a given channel.
#define GET_OPPOSITE_PORT_ID(source_port_id) ((source_port_id) ^ 1)

namespace {
typedef std::map<URLRequestContext*, ExtensionMessageService*> InstanceMap;
struct SingletonData {
  ~SingletonData() {
    STLDeleteContainerPairSecondPointers(map.begin(), map.end());
  }
  Lock lock;
  InstanceMap map;
};
}  // namespace

// Since ExtensionMessageService is a collection of Singletons, we don't need to
// grab a reference to it when creating Tasks involving it.
template <> struct RunnableMethodTraits<ExtensionMessageService> {
  static void RetainCallee(ExtensionMessageService*) {}
  static void ReleaseCallee(ExtensionMessageService*) {}
};

// static
ExtensionMessageService* ExtensionMessageService::GetInstance(
    URLRequestContext* context) {
  SingletonData* data = Singleton<SingletonData>::get();
  AutoLock lock(data->lock);

  ExtensionMessageService* instance = data->map[context];
  if (!instance) {
    instance = new ExtensionMessageService();
    data->map[context] = instance;
  }
  return instance;
}

ExtensionMessageService::ExtensionMessageService()
    : next_port_id_(0), observing_renderer_shutdown_(false) {
}

void ExtensionMessageService::RegisterExtension(
    const std::string& extension_id, int render_process_id) {
  AutoLock lock(process_ids_lock_);
  DCHECK(process_ids_.find(extension_id) == process_ids_.end() ||
         process_ids_[extension_id] == render_process_id);
  process_ids_[extension_id] = render_process_id;
}

int ExtensionMessageService::OpenChannelToExtension(
    const std::string& extension_id, ResourceMessageFilter* source) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));

  // Lookup the targeted extension process.
  int process_id;
  {
    AutoLock lock(process_ids_lock_);
    ProcessIDMap::iterator process_id_it = process_ids_.find(
        StringToLowerASCII(extension_id));
    if (process_id_it == process_ids_.end())
      return -1;
    process_id = process_id_it->second;
  }

  RendererMap::iterator renderer = renderers_.find(process_id);
  if (renderer == renderers_.end())
    return -1;

  ResourceMessageFilter* dest = renderer->second;

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

void ExtensionMessageService::DispatchEventToRenderers(
    const std::string& event_name, const std::string& event_args) {
  MessageLoop* io_thread = ChromeThread::GetMessageLoop(ChromeThread::IO);
  if (MessageLoop::current() != io_thread) {
    // Do the actual work on the IO thread.
    io_thread->PostTask(FROM_HERE, NewRunnableMethod(this,
        &ExtensionMessageService::DispatchEventToRenderers,
        event_name, event_args));
    return;
  }

  // TODO(mpcomplete): we should only send messages to extension process
  // renderers.
  std::set<ResourceMessageFilter*>::iterator renderer;
  for (renderer = renderers_unique_.begin();
       renderer != renderers_unique_.end(); ++renderer) {
    (*renderer)->Send(new ViewMsg_ExtensionHandleEvent(event_name, event_args));
  }
}

void ExtensionMessageService::RendererReady(ResourceMessageFilter* renderer) {
  DCHECK(MessageLoop::current() ==
       ChromeThread::GetMessageLoop(ChromeThread::IO));

  DCHECK(renderers_.find(renderer->GetProcessId()) == renderers_.end());
  renderers_[renderer->GetProcessId()] = renderer;
  renderers_unique_.insert(renderer);

  if (!observing_renderer_shutdown_) {
    observing_renderer_shutdown_ = true;
    NotificationService::current()->AddObserver(
        this,
        NotificationType::RESOURCE_MESSAGE_FILTER_SHUTDOWN,
        NotificationService::AllSources());
  }
}

void ExtensionMessageService::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  DCHECK(MessageLoop::current() ==
       ChromeThread::GetMessageLoop(ChromeThread::IO));

  DCHECK(type.value == NotificationType::RESOURCE_MESSAGE_FILTER_SHUTDOWN);
  ResourceMessageFilter* renderer = Source<ResourceMessageFilter>(source).ptr();

  renderers_.erase(renderer->GetProcessId());
  renderers_unique_.erase(renderer);

  // Close any channels that share this renderer.
  // TODO(mpcomplete): should we notify the other side of the port?
  for (MessageChannelMap::iterator it = channels_.begin();
       it != channels_.end(); ) {
    MessageChannelMap::iterator current = it++;
    if (current->second.port1 == renderer || current->second.port2 == renderer)
      channels_.erase(current);
  }
}
