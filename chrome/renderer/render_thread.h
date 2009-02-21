// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDER_THREAD_H_
#define CHROME_RENDERER_RENDER_THREAD_H_

#include <vector>

#include "base/gfx/native_widget_types.h"
#include "base/shared_memory.h"
#include "base/task.h"
#include "build/build_config.h"
#include "chrome/common/child_thread.h"

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
class RenderThreadBase {
 public:
  virtual ~RenderThreadBase() {}

  virtual bool Send(IPC::Message* msg) = 0;

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
class RenderThread : public RenderThreadBase,
                     public ChildThread {
 public:
  // Grabs the IPC channel name from the command line.
  RenderThread();
  // Constructor that's used when running in single process mode.
  RenderThread(const std::wstring& channel_name);
  virtual ~RenderThread();

  // Returns the one render thread for this process.  Note that this should only
  // be accessed when running on the render thread itself
  static RenderThread* current();

  // Overridded from RenderThreadBase.
  virtual bool Send(IPC::Message* msg) {
    return ChildThread::Send(msg);
  }

  virtual bool InSend() const {
    return ChildThread::InSend();
  }

  virtual void AddRoute(int32 routing_id, IPC::Channel::Listener* listener) {
    return ChildThread::AddRoute(routing_id, listener);
  }
  virtual void RemoveRoute(int32 routing_id) {
    return ChildThread::RemoveRoute(routing_id);
  }

  virtual void AddFilter(IPC::ChannelProxy::MessageFilter* filter);
  virtual void RemoveFilter(IPC::ChannelProxy::MessageFilter* filter);

  // Gets the VisitedLinkSlave instance for this thread
  VisitedLinkSlave* visited_link_slave() const { return visited_link_slave_; }

  // Gets the UserScriptSlave instance for this thread
  UserScriptSlave* user_script_slave() const { return user_script_slave_; }

  // Do DNS prefetch resolution of a hostname.
  void Resolve(const char* name, size_t length);

  // Invokes InformHostOfCacheStats after a short delay.  Used to move this
  // bookkeeping operation off the critical latency path.
  void InformHostOfCacheStatsLater();

 private:
  virtual void OnControlMessageReceived(const IPC::Message& msg);

  // Called by the thread base class
  virtual void Init();
  virtual void CleanUp();

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

  // These objects live solely on the render thread.
  VisitedLinkSlave* visited_link_slave_;
  UserScriptSlave* user_script_slave_;

  scoped_ptr<RenderDnsMaster> render_dns_master_;

  scoped_ptr<ScopedRunnableMethodFactory<RenderThread> > cache_stats_factory_;

  scoped_ptr<NotificationService> notification_service_;

  DISALLOW_COPY_AND_ASSIGN(RenderThread);
};

#endif  // CHROME_RENDERER_RENDER_THREAD_H_
