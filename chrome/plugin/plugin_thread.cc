// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/plugin/plugin_thread.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <objbase.h>
#endif

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/process_util.h"
#include "base/thread_local.h"
#include "chrome/common/child_process.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/plugin/chrome_plugin_host.h"
#include "chrome/plugin/npobject_util.h"
#include "chrome/renderer/render_thread.h"
#include "net/base/net_errors.h"
#include "webkit/glue/plugins/plugin_lib.h"
#include "webkit/glue/webkit_glue.h"

static base::LazyInstance<base::ThreadLocalPointer<PluginThread> > lazy_tls(
    base::LINKER_INITIALIZED);

PluginThread::PluginThread()
    : ChildThread(base::Thread::Options(MessageLoop::TYPE_UI, 0)),
      preloaded_plugin_module_(NULL) {
  plugin_path_ = FilePath::FromWStringHack(
      CommandLine::ForCurrentProcess()->GetSwitchValue(switches::kPluginPath));
}

PluginThread::~PluginThread() {
}

PluginThread* PluginThread::current() {
  return lazy_tls.Pointer()->Get();
}

void PluginThread::OnControlMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PluginThread, msg)
    IPC_MESSAGE_HANDLER(PluginProcessMsg_CreateChannel, OnCreateChannel)
    IPC_MESSAGE_HANDLER(PluginProcessMsg_PluginMessage, OnPluginMessage)
  IPC_END_MESSAGE_MAP()
}

void PluginThread::Init() {
  lazy_tls.Pointer()->Set(this);
  ChildThread::Init();

  PatchNPNFunctions();
#if defined(OS_WIN)
  CoInitialize(NULL);
#endif

  notification_service_.reset(new NotificationService);

  // Preload the library to avoid loading, unloading then reloading
  preloaded_plugin_module_ = base::LoadNativeLibrary(plugin_path_);

  ChromePluginLib::Create(plugin_path_, GetCPBrowserFuncsForPlugin());

  scoped_refptr<NPAPI::PluginLib> plugin =
      NPAPI::PluginLib::CreatePluginLib(plugin_path_);
  if (plugin.get()) {
    plugin->NP_Initialize();
  }

  // Certain plugins, such as flash, steal the unhandled exception filter
  // thus we never get crash reports when they fault. This call fixes it.
  message_loop()->set_exception_restoration(true);
}

void PluginThread::CleanUp() {
  if (preloaded_plugin_module_) {
    base::UnloadNativeLibrary(preloaded_plugin_module_);
    preloaded_plugin_module_ = NULL;
  }
  PluginChannelBase::CleanupChannels();
  NPAPI::PluginLib::UnloadAllPlugins();
  ChromePluginLib::UnloadAllPlugins();
  notification_service_.reset();
#if defined(OS_WIN)
  CoUninitialize();
#endif

  if (webkit_glue::ShouldForcefullyTerminatePluginProcess())
    base::KillProcess(base::GetCurrentProcessHandle(), 0, /* wait= */ false);

  // Call this last because it deletes the ResourceDispatcher, which is used
  // in some of the above cleanup.
  // See http://code.google.com/p/chromium/issues/detail?id=8980
  ChildThread::CleanUp();
  lazy_tls.Pointer()->Set(NULL);
}

void PluginThread::OnCreateChannel(
    int process_id,
    bool off_the_record) {
  scoped_refptr<PluginChannel> channel =
      PluginChannel::GetPluginChannel(process_id, owner_loop());
  IPC::ChannelHandle channel_handle;
  if (channel.get()) {
    channel_handle.name = channel->channel_name();
#if defined(OS_POSIX)
    // On POSIX, pass the renderer-side FD. Also mark it as auto-close so that
    // it gets closed after it has been sent.
    int renderer_fd = channel->DisownRendererFd();
    channel_handle.socket = base::FileDescriptor(renderer_fd, true);
#endif
    channel->set_off_the_record(off_the_record);
  }

  Send(new PluginProcessHostMsg_ChannelCreated(channel_handle));
}

void PluginThread::OnPluginMessage(const std::vector<unsigned char> &data) {
  // We Add/Release ref here to ensure that something will trigger the
  // shutdown mechanism for processes started in the absence of renderer's
  // opening a plugin channel.
  ChildProcess::current()->AddRefProcess();
  ChromePluginLib *chrome_plugin = ChromePluginLib::Find(plugin_path_);
  if (chrome_plugin) {
    void *data_ptr = const_cast<void*>(reinterpret_cast<const void*>(&data[0]));
    uint32 data_len = static_cast<uint32>(data.size());
    chrome_plugin->functions().on_message(data_ptr, data_len);
  }
  ChildProcess::current()->ReleaseProcess();
}

namespace webkit_glue {

#if defined(OS_WIN)
bool DownloadUrl(const std::string& url, HWND caller_window) {
  PluginThread* plugin_thread = PluginThread::current();
  if (!plugin_thread) {
    return false;
  }

  IPC::Message* message =
      new PluginProcessHostMsg_DownloadUrl(MSG_ROUTING_NONE, url,
                                           ::GetCurrentProcessId(),
                                           caller_window);
  return plugin_thread->Send(message);
}
#endif

bool GetPluginFinderURL(std::string* plugin_finder_url) {
  if (!plugin_finder_url) {
    NOTREACHED();
    return false;
  }

  PluginThread* plugin_thread = PluginThread::current();
  if (!plugin_thread)
    return false;

  plugin_thread->Send(
      new PluginProcessHostMsg_GetPluginFinderUrl(plugin_finder_url));
  DCHECK(!plugin_finder_url->empty());
  return true;
}

bool IsDefaultPluginEnabled() {
#if defined(OS_WIN)
  return true;
#elif defined(OS_LINUX)
  // http://code.google.com/p/chromium/issues/detail?id=10952
  return false;
#elif defined(OS_MACOSX)
  NOTIMPLEMENTED();
  return false;
#endif
}

// Dispatch the resolve proxy resquest to the right code, depending on which
// process the plugin is running in {renderer, browser, plugin}.
bool FindProxyForUrl(const GURL& url, std::string* proxy_list) {
  int net_error;
  std::string proxy_result;

  bool result;
  if (IsPluginProcess()) {
    result = PluginThread::current()->Send(
        new PluginProcessHostMsg_ResolveProxy(url, &net_error, &proxy_result));
  } else {
    result = RenderThread::current()->Send(
        new ViewHostMsg_ResolveProxy(url, &net_error, &proxy_result));
  }

  if (!result || net_error != net::OK)
    return false;

  *proxy_list = proxy_result;
  return true;
}

} // namespace webkit_glue
