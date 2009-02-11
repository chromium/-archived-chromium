// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDER_THREAD_H_
#define CHROME_RENDERER_RENDER_THREAD_H_

#include <vector>

#include "base/gfx/native_widget_types.h"
#include "base/shared_memory.h"
#include "base/task.h"
#include "base/thread.h"
#include "build/build_config.h"
#include "chrome/common/ipc_sync_channel.h"
#include "chrome/common/message_router.h"

class FilePath;
class NotificationService;
class RenderDnsMaster;
class SkBitmap;
class UserScriptSlave;
class VisitedLinkSlave;
struct ModalDialogEvent;
struct WebPreferences;

// The RenderThreadBase is the minimal interface that a RenderView/Widget
// expects from a render thread. The interface basically abstracts a way to send
// and receive messages.
class RenderThreadBase : public IPC::Message::Sender {
 public:
  virtual ~RenderThreadBase() {}

  // True if currently sending a message.
  virtual bool InSend() const = 0;

  // Called to add or remove a listener for a particular message routing ID.
  // These methods normally get delegated to a MessageRouter.
  virtual void AddRoute(int32 routing_id, IPC::Channel::Listener* listener) = 0;
  virtual void RemoveRoute(int32 routing_id) = 0;

  virtual void AddFilter(IPC::ChannelProxy::MessageFilter* filter) = 0;
  virtual void RemoveFilter(IPC::ChannelProxy::MessageFilter* filter) = 0;
};

// The RenderThread class represents a background thread where RenderView
// instances live.  The RenderThread supports an API that is used by its
// consumer to talk indirectly to the RenderViews and supporting objects.
// Likewise, it provides an API for the RenderViews to talk back to the main
// process (i.e., their corresponding WebContents).
//
// Most of the communication occurs in the form of IPC messages.  They are
// routed to the RenderThread according to the routing IDs of the messages.
// The routing IDs correspond to RenderView instances.
class RenderThread : public IPC::Channel::Listener,
                     public RenderThreadBase,
                     public base::Thread {
 public:
  explicit RenderThread(const std::wstring& channel_name);
  virtual ~RenderThread();

  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelError();

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

  // Overridded from RenderThreadBase.
  virtual void AddFilter(IPC::ChannelProxy::MessageFilter* filter);
  virtual void RemoveFilter(IPC::ChannelProxy::MessageFilter* filter);

  // Gets the VisitedLinkSlave instance for this thread
  VisitedLinkSlave* visited_link_slave() const { return visited_link_slave_; }

  // Gets the UserScriptSlave instance for this thread
  UserScriptSlave* user_script_slave() const { return user_script_slave_; }

  // Do DNS prefetch resolution of a hostname.
  void Resolve(const char* name, size_t length);

  // See documentation on MessageRouter for AddRoute and RemoveRoute
  virtual void AddRoute(int32 routing_id, IPC::Channel::Listener* listener);
  virtual void RemoveRoute(int32 routing_id);

  // Invokes InformHostOfCacheStats after a short delay.  Used to move this
  // bookkeeping operation off the critical latency path.
  void InformHostOfCacheStatsLater();

  MessageLoop* owner_loop() { return owner_loop_; }

  // Indicates if RenderThread::Send() is on the call stack.
  virtual bool InSend() const { return in_send_ != 0; }

 protected:
  // Called by the thread base class
  virtual void Init();
  virtual void CleanUp();

 private:
  void OnUpdateVisitedLinks(base::SharedMemoryHandle table);
  void OnUpdateUserScripts(base::SharedMemoryHandle table);

  void OnPluginMessage(const FilePath& plugin_path,
                       const std::vector<uint8>& data);
  void OnSetNextPageID(int32 next_page_id);
  void OnCreateNewView(gfx::NativeViewId parent_hwnd,
                       ModalDialogEvent modal_dialog_event,
                       const WebPreferences& webkit_prefs,
                       int32 view_id);
  void OnTransferBitmap(const SkBitmap& bitmap, int resource_id);
  void OnSetCacheCapacities(size_t min_dead_capacity,
                            size_t max_dead_capacity,
                            size_t capacity);
  void OnGetCacheResourceStats();

  // Gather usage statistics from the in-memory cache and inform our host.
  // These functions should be call periodically so that the host can make
  // decisions about how to allocation resources using current information.
  void InformHostOfCacheStats();

  // The message loop used to run tasks on the thread that started this thread.
  MessageLoop* owner_loop_;

  // Used only on the background render thread to implement message routing
  // functionality to the consumers of the RenderThread.
  MessageRouter router_;

  std::wstring channel_name_;
  scoped_ptr<IPC::SyncChannel> channel_;

  // These objects live solely on the render thread.
  VisitedLinkSlave* visited_link_slave_;
  UserScriptSlave* user_script_slave_;

  scoped_ptr<RenderDnsMaster> render_dns_master_;

  scoped_ptr<ScopedRunnableMethodFactory<RenderThread> > cache_stats_factory_;

  scoped_ptr<NotificationService> notification_service_;

  int in_send_;

  DISALLOW_COPY_AND_ASSIGN(RenderThread);
};

// The global RenderThread object for this process. Note that this should only
// be accessed when running on the render thread itself.
extern RenderThread* g_render_thread;

#endif  // CHROME_RENDERER_RENDER_THREAD_H_
