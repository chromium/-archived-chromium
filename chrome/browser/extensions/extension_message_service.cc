// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_message_service.h"

#include "base/json_writer.h"
#include "base/singleton.h"
#include "base/stl_util-inl.h"
#include "base/values.h"
#include "chrome/browser/child_process_security_policy.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"

// Since we have 2 ports for every channel, we just index channels by half the
// port ID.
#define GET_CHANNEL_ID(port_id) ((port_id) / 2)
#define GET_CHANNEL_OPENER_ID(channel_id) ((channel_id) * 2)
#define GET_CHANNEL_RECEIVERS_ID(channel_id) ((channel_id) * 2 + 1)

// Port1 is always even, port2 is always odd.
#define IS_OPENER_PORT_ID(port_id) (((port_id) & 1) == 0)

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

static void DispatchOnConnect(IPC::Message::Sender* channel, int source_port_id,
                              const std::string& tab_json,
                              const std::string& extension_id) {
  ListValue args;
  args.Set(0, Value::CreateIntegerValue(source_port_id));
  args.Set(1, Value::CreateStringValue(tab_json));
  args.Set(2, Value::CreateStringValue(extension_id));
  channel->Send(new ViewMsg_ExtensionMessageInvoke(
      ExtensionMessageService::kDispatchOnConnect, args));
}

static void DispatchOnDisconnect(IPC::Message::Sender* channel,
                                 int source_port_id) {
  ListValue args;
  args.Set(0, Value::CreateIntegerValue(source_port_id));
  channel->Send(new ViewMsg_ExtensionMessageInvoke(
      ExtensionMessageService::kDispatchOnDisconnect, args));
}

static void DispatchOnMessage(IPC::Message::Sender* channel,
                              const std::string& message, int source_port_id) {
  ListValue args;
  args.Set(0, Value::CreateStringValue(message));
  args.Set(1, Value::CreateIntegerValue(source_port_id));
  channel->Send(new ViewMsg_ExtensionMessageInvoke(
      ExtensionMessageService::kDispatchOnMessage, args));
}

static void DispatchEvent(IPC::Message::Sender* channel,
                          const std::string& event_name,
                          const std::string& event_args) {
  ListValue args;
  args.Set(0, Value::CreateStringValue(event_name));
  args.Set(1, Value::CreateStringValue(event_args));
  channel->Send(new ViewMsg_ExtensionMessageInvoke(
      ExtensionMessageService::kDispatchEvent, args));
}

static std::string GetChannelConnectEvent(const std::string& extension_id) {
  return StringPrintf("channel-connect:%s", extension_id.c_str());
}

}  // namespace

// Since ExtensionMessageService is a collection of Singletons, we don't need to
// grab a reference to it when creating Tasks involving it.
template <> struct RunnableMethodTraits<ExtensionMessageService> {
  static void RetainCallee(ExtensionMessageService*) {}
  static void ReleaseCallee(ExtensionMessageService*) {}
};


const char ExtensionMessageService::kDispatchOnConnect[] =
    "Port.dispatchOnConnect";
const char ExtensionMessageService::kDispatchOnDisconnect[] =
    "Port.dispatchOnDisconnect";
const char ExtensionMessageService::kDispatchOnMessage[] =
    "Port.dispatchOnMessage";
const char ExtensionMessageService::kDispatchEvent[] =
    "Event.dispatchJSON";

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
    : ui_loop_(NULL), initialized_(false), next_port_id_(0) {
}

void ExtensionMessageService::Init() {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  if (initialized_)
    return;
  initialized_ = true;

  ui_loop_ = MessageLoop::current();

  registrar_.Add(this, NotificationType::RENDERER_PROCESS_TERMINATED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::RENDERER_PROCESS_CLOSED,
                 NotificationService::AllSources());
}

void ExtensionMessageService::AddEventListener(std::string event_name,
                                               int render_process_id) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);
  DCHECK(listeners_[event_name].count(render_process_id) == 0);
  listeners_[event_name].insert(render_process_id);
}

void ExtensionMessageService::RemoveEventListener(std::string event_name,
                                                  int render_process_id) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);
  DCHECK(listeners_[event_name].count(render_process_id) == 1);
  listeners_[event_name].erase(render_process_id);
}

void ExtensionMessageService::AllocatePortIdPair(int* port1, int* port2) {
  AutoLock lock(next_port_id_lock_);

  // TODO(mpcomplete): what happens when this wraps?
  int port1_id = next_port_id_++;
  int port2_id = next_port_id_++;

  DCHECK(IS_OPENER_PORT_ID(port1_id));
  DCHECK(GET_OPPOSITE_PORT_ID(port1_id) == port2_id);
  DCHECK(GET_OPPOSITE_PORT_ID(port2_id) == port1_id);
  DCHECK(GET_CHANNEL_ID(port1_id) == GET_CHANNEL_ID(port2_id));

  int channel_id = GET_CHANNEL_ID(port1_id);
  DCHECK(GET_CHANNEL_OPENER_ID(channel_id) == port1_id);
  DCHECK(GET_CHANNEL_RECEIVERS_ID(channel_id) == port2_id);

  *port1 = port1_id;
  *port2 = port2_id;
}

int ExtensionMessageService::OpenChannelToExtension(
    int routing_id, const std::string& extension_id,
    ResourceMessageFilter* source) {
  DCHECK_EQ(MessageLoop::current(),
            ChromeThread::GetMessageLoop(ChromeThread::IO));
  DCHECK(initialized_);

  // Create a channel ID for both sides of the channel.
  int port1_id = -1;
  int port2_id = -1;
  AllocatePortIdPair(&port1_id, &port2_id);

  ui_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &ExtensionMessageService::OpenChannelOnUIThread,
          routing_id, port1_id, source->GetProcessId(), extension_id));

  return port2_id;
}

void ExtensionMessageService::OpenChannelOnUIThread(
    int source_routing_id, int source_port_id, int source_process_id,
    const std::string& extension_id) {
  RenderProcessHost* source = RenderProcessHost::FromID(source_process_id);
  OpenChannelOnUIThreadImpl(source_routing_id, source_port_id,
                            source_process_id, source, extension_id);
}

void ExtensionMessageService::OpenChannelOnUIThreadImpl(
    int source_routing_id, int source_port_id, int source_process_id,
    IPC::Message::Sender* source, const std::string& extension_id) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  if (!source)
    return;  // Source closed while task was in flight.

  linked_ptr<MessageChannel> channel(new MessageChannel);
  channel->opener.insert(source);

  // Get the list of processes that are listening for this extension's channel
  // connect event.
  std::string event_name = GetChannelConnectEvent(extension_id);
  std::set<int>& pids = listeners_[event_name];
  for (std::set<int>::iterator pid = pids.begin(); pid != pids.end(); ++pid) {
    RenderProcessHost* renderer = RenderProcessHost::FromID(*pid);
    if (!renderer)
      continue;
    channel->receivers.insert(renderer);
  }
  if (channel->receivers.empty()) {
    // Either no one is listening, or all listeners have since closed.
    // TODO(mpcomplete): should we notify the source?
    return;
  }

  channels_[GET_CHANNEL_ID(source_port_id)] = channel;

  // Include info about the opener's tab (if it was a tab).
  std::string tab_json = "null";
  TabContents* contents = tab_util::GetTabContentsByID(source_process_id,
                                                       source_routing_id);
  if (contents) {
    DictionaryValue* tab_value = ExtensionTabUtil::CreateTabValue(contents);
    JSONWriter::Write(tab_value, false, &tab_json);
  }

  // Broadcast the connect event to the receivers.  Give them the opener's
  // port ID (the opener has the opposite port ID).
  for (MessageChannel::Ports::iterator it = channel->receivers.begin();
       it != channel->receivers.end(); ++it) {
    DispatchOnConnect(*it, source_port_id, tab_json, extension_id);
  }
}

int ExtensionMessageService::OpenAutomationChannelToExtension(
    int source_process_id, int routing_id, const std::string& extension_id,
    IPC::Message::Sender* source) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);
  DCHECK(initialized_);

  int port1_id = -1;
  int port2_id = -1;
  // Create a channel ID for both sides of the channel.
  AllocatePortIdPair(&port1_id, &port2_id);

  // TODO(siggi): The source process- and routing ids are used to
  //      describe the originating tab to the target extension.
  //      This isn't really appropriate here, the originating tab
  //      information should be supplied by the caller for
  //      automation-initiated ports.
  OpenChannelOnUIThreadImpl(routing_id, port1_id, source_process_id,
                            source, extension_id);

  return port2_id;
}

void ExtensionMessageService::CloseChannel(int port_id) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  // Note: The channel might be gone already, if the other side closed first.
  MessageChannelMap::iterator it = channels_.find(GET_CHANNEL_ID(port_id));
  if (it != channels_.end())
    CloseChannelImpl(it, port_id);
}

void ExtensionMessageService::CloseChannelImpl(
    MessageChannelMap::iterator channel_iter, int closing_port_id) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  // Notify the other side.
  MessageChannel::Ports* ports =
      IS_OPENER_PORT_ID(closing_port_id) ?
          &channel_iter->second->receivers : &channel_iter->second->opener;

  for (MessageChannel::Ports::iterator it = ports->begin();
       it != ports->end(); ++it) {
    DispatchOnDisconnect(*it, GET_OPPOSITE_PORT_ID(closing_port_id));
  }

  channels_.erase(channel_iter);
}

void ExtensionMessageService::PostMessageFromRenderer(
    int dest_port_id, const std::string& message) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  MessageChannelMap::iterator iter =
      channels_.find(GET_CHANNEL_ID(dest_port_id));
  if (iter == channels_.end())
    return;

  // Figure out which port the ID corresponds to.
  MessageChannel::Ports* ports =
      IS_OPENER_PORT_ID(dest_port_id) ?
          &iter->second->opener : &iter->second->receivers;
  int source_port_id = GET_OPPOSITE_PORT_ID(dest_port_id);

  for (MessageChannel::Ports::iterator it = ports->begin();
       it != ports->end(); ++it) {
    DispatchOnMessage(*it, message, source_port_id);
  }
}

void ExtensionMessageService::DispatchEventToRenderers(
    const std::string& event_name, const std::string& event_args) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  std::set<int>& pids = listeners_[event_name];

  // Send the event only to renderers that are listening for it.
  for (std::set<int>::iterator pid = pids.begin(); pid != pids.end(); ++pid) {
    RenderProcessHost* renderer = RenderProcessHost::FromID(*pid);
    if (!renderer)
      continue;
    if (!ChildProcessSecurityPolicy::GetInstance()->
            HasExtensionBindings(*pid)) {
      // Don't send browser-level events to unprivileged processes.
      continue;
    }

    DispatchEvent(renderer, event_name, event_args);
  }
}

void ExtensionMessageService::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  DCHECK(type.value == NotificationType::RENDERER_PROCESS_TERMINATED ||
         type.value == NotificationType::RENDERER_PROCESS_CLOSED);

  RenderProcessHost* renderer = Source<RenderProcessHost>(source).ptr();

  // Close any channels that share this renderer.  We notify the opposite
  // port that his pair has closed.
  for (MessageChannelMap::iterator it = channels_.begin();
       it != channels_.end(); ) {
    MessageChannelMap::iterator current = it++;
    if (current->second->opener.count(renderer) > 0) {
      CloseChannelImpl(current, GET_CHANNEL_OPENER_ID(current->first));
    } else if (current->second->receivers.count(renderer) > 0) {
      CloseChannelImpl(current, GET_CHANNEL_RECEIVERS_ID(current->first));
    }
  }

  // Remove this renderer from our listener maps.
  for (ListenerMap::iterator it = listeners_.begin();
       it != listeners_.end(); ) {
    ListenerMap::iterator current = it++;
    current->second.erase(renderer->pid());
    if (current->second.empty())
      listeners_.erase(current);
  }
}
