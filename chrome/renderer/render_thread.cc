// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <algorithm>

#include "chrome/renderer/render_thread.h"

#include "base/shared_memory.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/ipc_logging.h"
#include "chrome/common/notification_service.h"
#include "chrome/plugin/plugin_channel.h"
#include "chrome/renderer/net/render_dns_master.h"
#include "chrome/renderer/render_process.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/user_script_slave.h"
#include "chrome/renderer/visitedlink_slave.h"
#include "webkit/glue/cache_manager.h"

RenderThread* g_render_thread;

static const unsigned int kCacheStatsDelayMS = 2000 /* milliseconds */;

// V8 needs a 1MB stack size.
static const size_t kStackSize = 1024 * 1024;

//-----------------------------------------------------------------------------
// Methods below are only called on the owner's thread:

RenderThread::RenderThread(const std::wstring& channel_name)
    : Thread("Chrome_RenderThread"),
      channel_name_(channel_name),
      owner_loop_(MessageLoop::current()),
      visited_link_slave_(NULL),
      user_script_slave_(NULL),
      render_dns_master_(NULL),
      in_send_(0) {
  DCHECK(owner_loop_);
  base::Thread::Options options;
  options.stack_size = kStackSize;
  // When we run plugins in process, we actually run them on the render thread,
  // which means that we need to make the render thread pump UI events.
  if (RenderProcess::ShouldLoadPluginsInProcess())
    options.message_loop_type = MessageLoop::TYPE_UI;
  StartWithOptions(options);
}

RenderThread::~RenderThread() {
  Stop();
}

void RenderThread::OnChannelError() {
  owner_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

bool RenderThread::Send(IPC::Message* msg) {
  in_send_++;
  bool rv = channel_->Send(msg);
  in_send_--;
  return rv;
}

void RenderThread::AddFilter(IPC::ChannelProxy::MessageFilter* filter) {
  channel_->AddFilter(filter);
}

void RenderThread::RemoveFilter(IPC::ChannelProxy::MessageFilter* filter) {
  channel_->RemoveFilter(filter);
}

void RenderThread::Resolve(const char* name, size_t length) {
  return render_dns_master_->Resolve(name, length);
}

void RenderThread::AddRoute(int32 routing_id,
                            IPC::Channel::Listener* listener) {
  DCHECK(MessageLoop::current() == message_loop());

  // This corresponds to the AddRoute call done in CreateView.
  router_.AddRoute(routing_id, listener);
}

void RenderThread::RemoveRoute(int32 routing_id) {
  DCHECK(MessageLoop::current() == message_loop());

  router_.RemoveRoute(routing_id);
}

void RenderThread::Init() {
  DCHECK(!g_render_thread);
  g_render_thread = this;

  notification_service_.reset(new NotificationService);

  cache_stats_factory_.reset(
      new ScopedRunnableMethodFactory<RenderThread>(this));

  channel_.reset(new IPC::SyncChannel(channel_name_,
      IPC::Channel::MODE_CLIENT, this, NULL, owner_loop_, true,
      RenderProcess::GetShutDownEvent()));

  // The renderer thread should wind-up COM.
  CoInitialize(0);

  visited_link_slave_ = new VisitedLinkSlave();
  user_script_slave_ = new UserScriptSlave();

  render_dns_master_.reset(new RenderDnsMaster());

#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging::current()->SetIPCSender(this);
#endif
}

void RenderThread::CleanUp() {
  DCHECK(g_render_thread == this);
  g_render_thread = NULL;

  // Need to destruct the SyncChannel to the browser before we go away because
  // it caches a pointer to this thread.
  channel_.reset();

  // Clean up plugin channels before this thread goes away.
  PluginChannelBase::CleanupChannels();

#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging::current()->SetIPCSender(NULL);
#endif

  notification_service_.reset();

  delete visited_link_slave_;
  visited_link_slave_ = NULL;

  delete user_script_slave_;
  user_script_slave_ = NULL;

  CoUninitialize();
}

void RenderThread::OnUpdateVisitedLinks(base::SharedMemoryHandle table) {
  DCHECK(table) << "Bad table handle";
  visited_link_slave_->Init(table);
}

void RenderThread::OnUpdateUserScripts(
    base::SharedMemoryHandle scripts) {
  DCHECK(scripts) << "Bad scripts handle";
  user_script_slave_->UpdateScripts(scripts);
}

void RenderThread::OnMessageReceived(const IPC::Message& msg) {
  // NOTE: We could subclass router_ to intercept OnControlMessageReceived, but
  // it seems simpler to just process any control messages that we care about
  // up-front and then send the rest of the messages onto router_.

  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    IPC_BEGIN_MESSAGE_MAP(RenderThread, msg)
      IPC_MESSAGE_HANDLER(ViewMsg_VisitedLink_NewTable, OnUpdateVisitedLinks)
      IPC_MESSAGE_HANDLER(ViewMsg_SetNextPageID, OnSetNextPageID)
      IPC_MESSAGE_HANDLER(ViewMsg_New, OnCreateNewView)
      IPC_MESSAGE_HANDLER(ViewMsg_SetCacheCapacities, OnSetCacheCapacities)
      IPC_MESSAGE_HANDLER(ViewMsg_GetCacheResourceStats,
                          OnGetCacheResourceStats)
      IPC_MESSAGE_HANDLER(ViewMsg_PluginMessage, OnPluginMessage)
      IPC_MESSAGE_HANDLER(ViewMsg_UserScripts_NewScripts,
                          OnUpdateUserScripts)
      // send the rest to the router
      IPC_MESSAGE_UNHANDLED(router_.OnMessageReceived(msg))
    IPC_END_MESSAGE_MAP()
  } else {
    router_.OnMessageReceived(msg);
  }
}

void RenderThread::OnPluginMessage(const FilePath& plugin_path,
                                   const std::vector<uint8>& data) {
  CHECK(ChromePluginLib::IsPluginThread());
  ChromePluginLib *chrome_plugin = ChromePluginLib::Find(plugin_path);
  if (chrome_plugin) {
    void *data_ptr = const_cast<void*>(reinterpret_cast<const void*>(&data[0]));
    uint32 data_len = static_cast<uint32>(data.size());
    chrome_plugin->functions().on_message(data_ptr, data_len);
  }
}

void RenderThread::OnSetNextPageID(int32 next_page_id) {
  // This should only be called at process initialization time, so we shouldn't
  // have to worry about thread-safety.
  RenderView::SetNextPageID(next_page_id);
}

void RenderThread::OnCreateNewView(HWND parent_hwnd,
                                   HANDLE modal_dialog_event,
                                   const WebPreferences& webkit_prefs,
                                   int32 view_id) {
  // TODO(darin): once we have a RenderThread per RenderView, this will need to
  // change to assert that we are not creating more than one view.
  RenderView::Create(
      this, parent_hwnd, modal_dialog_event, MSG_ROUTING_NONE, webkit_prefs,
      new SharedRenderViewCounter(0), view_id);
}

void RenderThread::OnSetCacheCapacities(size_t min_dead_capacity,
                                        size_t max_dead_capacity,
                                        size_t capacity) {
  CacheManager::SetCapacities(min_dead_capacity, max_dead_capacity, capacity);
}

void RenderThread::OnGetCacheResourceStats() {
  CacheManager::ResourceTypeStats stats;
  CacheManager::GetResourceTypeStats(&stats);
  Send(new ViewHostMsg_ResourceTypeStats(stats));
}

void RenderThread::InformHostOfCacheStats() {
  CacheManager::UsageStats stats;
  CacheManager::GetUsageStats(&stats);
  Send(new ViewHostMsg_UpdatedCacheStats(stats));
}

void RenderThread::InformHostOfCacheStatsLater() {
  // Rate limit informing the host of our cache stats.
  if (!cache_stats_factory_->empty())
    return;

  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      cache_stats_factory_->NewRunnableMethod(
          &RenderThread::InformHostOfCacheStats),
      kCacheStatsDelayMS);
}
