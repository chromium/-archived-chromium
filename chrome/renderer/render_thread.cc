// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/render_thread.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/shared_memory.h"
#include "base/stats_table.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/plugin/npobject_util.h"
// TODO(port)
#if defined(OS_WIN)
#include "chrome/plugin/plugin_channel.h"
#else
#include "base/scoped_handle.h"
#include "chrome/plugin/plugin_channel_base.h"
#include "webkit/glue/weburlrequest.h"
#endif
#include "chrome/renderer/extensions/extension_process_bindings.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/renderer/net/render_dns_master.h"
#include "chrome/renderer/render_process.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/renderer_webkitclient_impl.h"
#include "chrome/renderer/user_script_slave.h"
#include "chrome/renderer/visitedlink_slave.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCache.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "webkit/extensions/v8/gears_extension.h"
#include "webkit/extensions/v8/interval_extension.h"
#include "webkit/extensions/v8/playback_extension.h"

#if defined(OS_WIN)
#include <windows.h>
#include <objbase.h>
#endif

using WebKit::WebCache;
using WebKit::WebString;

static const unsigned int kCacheStatsDelayMS = 2000 /* milliseconds */;

//-----------------------------------------------------------------------------
// Methods below are only called on the owner's thread:

// When we run plugins in process, we actually run them on the render thread,
// which means that we need to make the render thread pump UI events.
RenderThread::RenderThread()
    : ChildThread(
          base::Thread::Options(RenderProcess::InProcessPlugins() ?
              MessageLoop::TYPE_UI : MessageLoop::TYPE_DEFAULT, kV8StackSize)) {
}

RenderThread::RenderThread(const std::wstring& channel_name)
    : ChildThread(
          base::Thread::Options(RenderProcess::InProcessPlugins() ?
              MessageLoop::TYPE_UI : MessageLoop::TYPE_DEFAULT, kV8StackSize)) {
  SetChannelName(channel_name);
}

RenderThread::~RenderThread() {
}

RenderThread* RenderThread::current() {
  DCHECK(!IsPluginProcess());
  return static_cast<RenderThread*>(ChildThread::current());
}

void RenderThread::AddFilter(IPC::ChannelProxy::MessageFilter* filter) {
  channel()->AddFilter(filter);
}

void RenderThread::RemoveFilter(IPC::ChannelProxy::MessageFilter* filter) {
  channel()->RemoveFilter(filter);
}

void RenderThread::Resolve(const char* name, size_t length) {
  return dns_master_->Resolve(name, length);
}

void RenderThread::SendHistograms() {
  return histogram_snapshots_->SendHistograms();
}

void RenderThread::Init() {
  // TODO(darin): Why do we need COM here?  This is probably bogus.
#if defined(OS_WIN)
  // The renderer thread should wind-up COM.
  CoInitialize(0);
#endif

  ChildThread::Init();
  notification_service_.reset(new NotificationService);
  cache_stats_factory_.reset(
      new ScopedRunnableMethodFactory<RenderThread>(this));

  visited_link_slave_.reset(new VisitedLinkSlave());
  user_script_slave_.reset(new UserScriptSlave());
  dns_master_.reset(new RenderDnsMaster());
  histogram_snapshots_.reset(new RendererHistogramSnapshots());
}

void RenderThread::CleanUp() {
  // Shutdown in reverse of the initialization order.

  histogram_snapshots_.reset();
  dns_master_.reset();
  user_script_slave_.reset();
  visited_link_slave_.reset();

  if (webkit_client_.get()) {
    WebKit::shutdown();
    webkit_client_.reset();
  }

  notification_service_.reset();

  ChildThread::CleanUp();

  // TODO(port)
#if defined(OS_WIN)
  // Clean up plugin channels before this thread goes away.
  PluginChannelBase::CleanupChannels();
#endif

#if defined(OS_WIN)
  CoUninitialize();
#endif
}

void RenderThread::OnUpdateVisitedLinks(base::SharedMemoryHandle table) {
  DCHECK(base::SharedMemory::IsHandleValid(table)) << "Bad table handle";
  visited_link_slave_->Init(table);
}

void RenderThread::OnUpdateUserScripts(
    base::SharedMemoryHandle scripts) {
  DCHECK(base::SharedMemory::IsHandleValid(scripts)) << "Bad scripts handle";
  user_script_slave_->UpdateScripts(scripts);
}

void RenderThread::OnSetExtensionFunctionNames(
    const std::vector<std::string>& names) {
  extensions_v8::ExtensionProcessBindings::SetFunctionNames(names);
}

void RenderThread::OnControlMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(RenderThread, msg)
    IPC_MESSAGE_HANDLER(ViewMsg_VisitedLink_NewTable, OnUpdateVisitedLinks)
    IPC_MESSAGE_HANDLER(ViewMsg_SetNextPageID, OnSetNextPageID)
    // TODO(port): removed from render_messages_internal.h;
    // is there a new non-windows message I should add here?
    IPC_MESSAGE_HANDLER(ViewMsg_New, OnCreateNewView)
    IPC_MESSAGE_HANDLER(ViewMsg_SetCacheCapacities, OnSetCacheCapacities)
    IPC_MESSAGE_HANDLER(ViewMsg_GetRendererHistograms,
                          OnGetRendererHistograms)
    IPC_MESSAGE_HANDLER(ViewMsg_GetCacheResourceStats,
                        OnGetCacheResourceStats)
    IPC_MESSAGE_HANDLER(ViewMsg_UserScripts_NewScripts,
                        OnUpdateUserScripts)
    IPC_MESSAGE_HANDLER(ViewMsg_Extension_SetFunctionNames,
                        OnSetExtensionFunctionNames)
  IPC_END_MESSAGE_MAP()
}

void RenderThread::OnSetNextPageID(int32 next_page_id) {
  // This should only be called at process initialization time, so we shouldn't
  // have to worry about thread-safety.
  // TODO(port)
#if !defined(OS_LINUX)
  RenderView::SetNextPageID(next_page_id);
#endif
}

void RenderThread::OnCreateNewView(gfx::NativeViewId parent_hwnd,
                                   ModalDialogEvent modal_dialog_event,
                                   const WebPreferences& webkit_prefs,
                                   int32 view_id) {
  EnsureWebKitInitialized();

  // When bringing in render_view, also bring in webkit's glue and jsbindings.
  base::WaitableEvent* waitable_event = new base::WaitableEvent(
#if defined(OS_WIN)
      modal_dialog_event.event);
#else
      true, false);
#endif

  // TODO(darin): once we have a RenderThread per RenderView, this will need to
  // change to assert that we are not creating more than one view.
  RenderView::Create(
      this, parent_hwnd, waitable_event, MSG_ROUTING_NONE, webkit_prefs,
      new SharedRenderViewCounter(0), view_id);
}

void RenderThread::OnSetCacheCapacities(size_t min_dead_capacity,
                                        size_t max_dead_capacity,
                                        size_t capacity) {
  EnsureWebKitInitialized();
  WebCache::setCapacities(
      min_dead_capacity, max_dead_capacity, capacity);
}

void RenderThread::OnGetCacheResourceStats() {
  EnsureWebKitInitialized();
  WebCache::ResourceTypeStats stats;
  WebCache::getResourceTypeStats(&stats);
  Send(new ViewHostMsg_ResourceTypeStats(stats));
}

void RenderThread::OnGetRendererHistograms() {
  SendHistograms();
}

void RenderThread::InformHostOfCacheStats() {
  EnsureWebKitInitialized();
  WebCache::UsageStats stats;
  WebCache::getUsageStats(&stats);
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

void RenderThread::EnsureWebKitInitialized() {
  if (webkit_client_.get())
    return;

  v8::V8::SetCounterFunction(StatsTable::FindLocation);

  webkit_client_.reset(new RendererWebKitClientImpl);
  WebKit::initialize(webkit_client_.get());

  // chrome-ui pages should not be accessible by normal content, and should
  // also be unable to script anything but themselves (to help limit the damage
  // that a corrupt chrome-ui page could cause).
  WebString chrome_ui_scheme(ASCIIToUTF16(chrome::kChromeUIScheme));
  WebKit::registerURLSchemeAsLocal(chrome_ui_scheme);
  WebKit::registerURLSchemeAsNoAccess(chrome_ui_scheme);

  WebKit::registerExtension(extensions_v8::GearsExtension::Get());
  WebKit::registerExtension(extensions_v8::IntervalExtension::Get());
  WebKit::registerExtension(extensions_v8::RendererExtensionBindings::Get());

  WebKit::registerExtension(extensions_v8::ExtensionProcessBindings::Get(),
      WebKit::WebString::fromUTF8(chrome::kExtensionScheme));

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kPlaybackMode) ||
      command_line.HasSwitch(switches::kRecordMode)) {
    WebKit::registerExtension(extensions_v8::PlaybackExtension::Get());
  }

  if (command_line.HasSwitch(switches::kEnableWebWorkers)) {
    WebKit::enableWebWorkers();
  }
}
