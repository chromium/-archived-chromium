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
#define GET_CHANNEL_PORT1(channel_id) ((channel_id) * 2)
#define GET_CHANNEL_PORT2(channel_id) ((channel_id) * 2 + 1)

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

void ExtensionMessageService::RegisterExtension(
    const std::string& extension_id, int render_process_id) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  // Make sure we're initialized.
  Init();

  AutoLock lock(process_ids_lock_);
  DCHECK(process_ids_.find(extension_id) == process_ids_.end() ||
         process_ids_[extension_id] == render_process_id);
  process_ids_[extension_id] = render_process_id;
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

  DCHECK(IS_PORT1_ID(port1_id));
  DCHECK(GET_OPPOSITE_PORT_ID(port1_id) == port2_id);
  DCHECK(GET_OPPOSITE_PORT_ID(port2_id) == port1_id);
  DCHECK(GET_CHANNEL_ID(port1_id) == GET_CHANNEL_ID(port2_id));

  int channel_id = GET_CHANNEL_ID(port1_id);
  DCHECK(GET_CHANNEL_PORT1(channel_id) == port1_id);
  DCHECK(GET_CHANNEL_PORT2(channel_id) == port2_id);

  *port1 = port1_id;
  *port2 = port2_id;
}

int ExtensionMessageService::GetProcessIdForExtension(
    const std::string& extension_id) {
  AutoLock lock(process_ids_lock_);
  ProcessIDMap::iterator process_id_it = process_ids_.find(
      StringToLowerASCII(extension_id));
  if (process_id_it == process_ids_.end())
    return -1;
  return process_id_it->second;
}

RenderProcessHost* ExtensionMessageService::GetProcessForExtension(
    const std::string& extension_id) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  int process_id = GetProcessIdForExtension(extension_id);
  if (process_id == -1)
    return NULL;

  RenderProcessHost* host = RenderProcessHost::FromID(process_id);
  DCHECK(host);

  return host;
}

int ExtensionMessageService::OpenChannelToExtension(
    int routing_id, const std::string& extension_id,
    ResourceMessageFilter* source) {
  DCHECK_EQ(MessageLoop::current(),
            ChromeThread::GetMessageLoop(ChromeThread::IO));

  // Lookup the targeted extension process.
  int process_id = GetProcessIdForExtension(extension_id);
  if (process_id == -1)
    return -1;

  DCHECK(initialized_);

  // Create a channel ID for both sides of the channel.
  int port1_id = -1;
  int port2_id = -1;
  AllocatePortIdPair(&port1_id, &port2_id);

  ui_loop_->PostTask(FROM_HERE,
      NewRunnableMethod(this, &ExtensionMessageService::OpenChannelOnUIThread,
          routing_id, port1_id, source->GetProcessId(), port2_id, process_id,
          extension_id));

  return port2_id;
}

void ExtensionMessageService::OpenChannelOnUIThread(
    int source_routing_id, int source_port_id, int source_process_id,
    int dest_port_id, int dest_process_id, const std::string& extension_id) {
  RenderProcessHost* source = RenderProcessHost::FromID(source_process_id);
  OpenChannelOnUIThreadImpl(source_routing_id, source_port_id,
                            source_process_id, source, dest_port_id,
                            dest_process_id, extension_id);
}

void ExtensionMessageService::OpenChannelOnUIThreadImpl(
    int source_routing_id, int source_port_id, int source_process_id,
    IPC::Message::Sender* source, int dest_port_id, int dest_process_id,
    const std::string& extension_id) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  MessageChannel channel;
  channel.port1 = source;
  channel.port2 = RenderProcessHost::FromID(dest_process_id);
  if (!channel.port1 || !channel.port2) {
    // One of the processes could have been closed while posting this task.
    return;
  }

  channels_[GET_CHANNEL_ID(source_port_id)] = channel;

  std::string tab_json = "null";
  TabContents* contents = tab_util::GetTabContentsByID(source_process_id,
                                                       source_routing_id);
  if (contents) {
    DictionaryValue* tab_value = ExtensionTabUtil::CreateTabValue(contents);
    JSONWriter::Write(tab_value, false, &tab_json);
  }

  // Send the process the id for the opposite port.
  DispatchOnConnect(channel.port2, source_port_id, tab_json, extension_id);
}

int ExtensionMessageService::OpenAutomationChannelToExtension(
    int source_process_id, int routing_id, const std::string& extension_id,
    IPC::Message::Sender* source) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  // Lookup the targeted extension process.
  int process_id = GetProcessIdForExtension(extension_id);
  if (process_id == -1)
    return -1;

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
                            source, port2_id, process_id, extension_id);

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
    MessageChannelMap::iterator channel_iter, int port_id) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  // Notify the other side.
  if (port_id == GET_CHANNEL_PORT1(channel_iter->first)) {
    DispatchOnDisconnect(channel_iter->second.port2,
                         GET_OPPOSITE_PORT_ID(port_id));
  } else {
    DCHECK_EQ(port_id, GET_CHANNEL_PORT2(channel_iter->first));
    DispatchOnDisconnect(channel_iter->second.port1,
                         GET_OPPOSITE_PORT_ID(port_id));
  }

  channels_.erase(channel_iter);
}

void ExtensionMessageService::PostMessageFromRenderer(
    int port_id, const std::string& message) {
  DCHECK_EQ(MessageLoop::current()->type(), MessageLoop::TYPE_UI);

  MessageChannelMap::iterator iter =
      channels_.find(GET_CHANNEL_ID(port_id));
  if (iter == channels_.end())
    return;
  MessageChannel& channel = iter->second;

  // Figure out which port the ID corresponds to.
  IPC::Message::Sender* dest =
      IS_PORT1_ID(port_id) ? channel.port1 : channel.port2;

  int source_port_id = GET_OPPOSITE_PORT_ID(port_id);
  DispatchOnMessage(dest, message, source_port_id);
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

  {
    AutoLock lock(process_ids_lock_);
    for (ProcessIDMap::iterator it = process_ids_.begin();
         it != process_ids_.end(); ) {
      ProcessIDMap::iterator current = it++;
      if (current->second == renderer->pid()) {
        process_ids_.erase(current);
      }
    }
  }

  // Close any channels that share this renderer.  We notify the opposite
  // port that his pair has closed.
  for (MessageChannelMap::iterator it = channels_.begin();
       it != channels_.end(); ) {
    MessageChannelMap::iterator current = it++;
    if (current->second.port1 == renderer) {
      CloseChannelImpl(current, GET_CHANNEL_PORT1(current->first));
    } else if (current->second.port2 == renderer) {
      CloseChannelImpl(current, GET_CHANNEL_PORT2(current->first));
    }
  }
}
