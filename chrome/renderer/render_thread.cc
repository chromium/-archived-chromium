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

#include <windows.h>
#include <algorithm>

#include "chrome/renderer/render_thread.h"

#include "base/shared_memory.h"
#include "chrome/common/ipc_logging.h"
#include "chrome/common/notification_service.h"
#include "chrome/plugin/plugin_channel.h"
#include "chrome/renderer/net/render_dns_master.h"
#include "chrome/renderer/render_process.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/visitedlink_slave.h"
#include "webkit/glue/cache_manager.h"

static const unsigned int kCacheStatsDelayMS = 2000 /* milliseconds */;

// V8 needs a 1MB stack size.
static const size_t kStackSize = 1024 * 1024;

/*static*/
DWORD RenderThread::tls_index_ = ThreadLocalStorage::Alloc();

//-----------------------------------------------------------------------------
// Methods below are only called on the owner's thread:

RenderThread::RenderThread(const std::wstring& channel_name)
    : Thread("Chrome_RenderThread"),
      channel_name_(channel_name),
      owner_loop_(MessageLoop::current()),
      visited_link_slave_(NULL),
      render_dns_master_(NULL),
      in_send_(0) {
  DCHECK(owner_loop_);
  StartWithStackSize(kStackSize);
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
  DCHECK(tls_index_) << "static initializer failed";
  DCHECK(!current()) << "should only have one RenderThread per thread";

  notification_service_.reset(new NotificationService);

  cache_stats_factory_.reset(
      new ScopedRunnableMethodFactory<RenderThread>(this));

  channel_.reset(new IPC::SyncChannel(channel_name_,
      IPC::Channel::MODE_CLIENT, this, NULL, owner_loop_, true,
      RenderProcess::GetShutDownEvent()));

  ThreadLocalStorage::Set(tls_index_, this);

  // The renderer thread should wind-up COM.
  CoInitialize(0);

  // TODO(darin): We should actually try to share this object between
  // RenderThread instances.
  visited_link_slave_ = new VisitedLinkSlave();

  render_dns_master_.reset(new RenderDnsMaster());

#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging::current()->SetIPCSender(this);
#endif
}

void RenderThread::CleanUp() {
  DCHECK(current() == this);

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

  CoUninitialize();
}

void RenderThread::OnUpdateVisitedLinks(SharedMemoryHandle table) {
  DCHECK(table) << "Bad table handle";
  visited_link_slave_->Init(table);
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
      // send the rest to the router
      IPC_MESSAGE_UNHANDLED(router_.OnMessageReceived(msg))
    IPC_END_MESSAGE_MAP()
  } else {
    router_.OnMessageReceived(msg);
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
      parent_hwnd, modal_dialog_event, MSG_ROUTING_NONE, webkit_prefs, view_id);
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
