// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_service.h"

#include "base/command_line.h"
#include "base/singleton.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_plugin_host.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/plugin_process_host.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "webkit/glue/plugins/plugin_list.h"

// static
PluginService* PluginService::GetInstance() {
  return Singleton<PluginService>::get();
}

PluginService::PluginService()
    : main_message_loop_(MessageLoop::current()),
      resource_dispatcher_host_(NULL),
      ui_locale_(g_browser_process->GetApplicationLocale()),
      plugin_shutdown_handler_(new ShutdownHandler) {
  // Have the NPAPI plugin list search for Chrome plugins as well.
  ChromePluginLib::RegisterPluginsWithNPAPI();

  // Load the one specified on the command line as well.
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  std::wstring path = command_line->GetSwitchValue(switches::kLoadPlugin);
  if (!path.empty())
    NPAPI::PluginList::AddExtraPluginPath(FilePath::FromWStringHack(path));
}

PluginService::~PluginService() {
}

void PluginService::GetPlugins(bool refresh,
                               std::vector<WebPluginInfo>* plugins) {
  AutoLock lock(lock_);
  NPAPI::PluginList::Singleton()->GetPlugins(refresh, plugins);
}

void PluginService::LoadChromePlugins(
    ResourceDispatcherHost* resource_dispatcher_host) {
  resource_dispatcher_host_ = resource_dispatcher_host;
  ChromePluginLib::LoadChromePlugins(GetCPBrowserFuncsForBrowser());
}

void PluginService::SetChromePluginDataDir(const FilePath& data_dir) {
  AutoLock lock(lock_);
  chrome_plugin_data_dir_ = data_dir;
}

const FilePath& PluginService::GetChromePluginDataDir() {
  AutoLock lock(lock_);
  return chrome_plugin_data_dir_;
}

const std::wstring& PluginService::GetUILocale() {
  return ui_locale_;
}

PluginProcessHost* PluginService::FindPluginProcess(
    const FilePath& plugin_path) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));

  if (plugin_path.value().empty()) {
    NOTREACHED() << "should only be called if we have a plugin to load";
    return NULL;
  }

  PluginMap::iterator found = plugin_hosts_.find(plugin_path);
  if (found != plugin_hosts_.end())
    return found->second;
  return NULL;
}

PluginProcessHost* PluginService::FindOrStartPluginProcess(
    const FilePath& plugin_path,
    const std::string& clsid) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));

  PluginProcessHost *plugin_host = FindPluginProcess(plugin_path);
  if (plugin_host)
    return plugin_host;

  WebPluginInfo info;
  if (!GetPluginInfoByPath(plugin_path, &info)) {
    DCHECK(false);
    return NULL;
  }

  // This plugin isn't loaded by any plugin process, so create a new process.
  plugin_host = new PluginProcessHost(this);
  if (!plugin_host->Init(info, clsid, ui_locale_)) {
    DCHECK(false);  // Init is not expected to fail
    delete plugin_host;
    return NULL;
  }
  plugin_hosts_[plugin_path] = plugin_host;
  return plugin_host;

  // TODO(jabdelmalek): adding a new channel means we can have one less
  // renderer process (since each child process uses one handle in the
  // IPC thread and main thread's WaitForMultipleObjects call).  Limit the
  // number of plugin processes.
}

void PluginService::OpenChannelToPlugin(
    ResourceMessageFilter* renderer_msg_filter, const GURL& url,
    const std::string& mime_type, const std::string& clsid,
    const std::wstring& locale, IPC::Message* reply_msg) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));
  FilePath plugin_path = GetPluginPath(url, mime_type, clsid, NULL);
  PluginProcessHost* plugin_host = FindOrStartPluginProcess(plugin_path, clsid);
  if (plugin_host) {
    plugin_host->OpenChannelToPlugin(renderer_msg_filter, mime_type, reply_msg);
  } else {
    PluginProcessHost::ReplyToRenderer(renderer_msg_filter,
                                       std::wstring(),
                                       FilePath(),
                                       reply_msg);
  }
}

void PluginService::OnPluginProcessIsShuttingDown(PluginProcessHost* host) {
  RemoveHost(host);
}

void PluginService::OnPluginProcessExited(PluginProcessHost* host) {
  RemoveHost(host);  // in case shutdown was not graceful
  delete host;
}

void PluginService::RemoveHost(PluginProcessHost* host) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));
  // Search for the instance rather than lookup by plugin path,
  // there is a small window where two instances for the same
  // plugin path can co-exists.
  PluginMap::iterator i = plugin_hosts_.begin();
  while (i != plugin_hosts_.end()) {
    if (i->second == host) {
      plugin_hosts_.erase(i);
      return;
    }
    i++;
  }
}

FilePath PluginService::GetPluginPath(const GURL& url,
                                      const std::string& mime_type,
                                      const std::string& clsid,
                                      std::string* actual_mime_type) {
  AutoLock lock(lock_);
  bool allow_wildcard = true;
  WebPluginInfo info;
  NPAPI::PluginList::Singleton()->GetPluginInfo(url, mime_type, clsid,
                                                allow_wildcard, &info,
                                                actual_mime_type);
  return info.path;
}

bool PluginService::GetPluginInfoByPath(const FilePath& plugin_path,
                                        WebPluginInfo* info) {
  AutoLock lock(lock_);
  return NPAPI::PluginList::Singleton()->GetPluginInfoByPath(plugin_path, info);
}

bool PluginService::HavePluginFor(const std::string& mime_type,
                                  bool allow_wildcard) {
  AutoLock lock(lock_);

  GURL url;
  WebPluginInfo info;
  return NPAPI::PluginList::Singleton()->GetPluginInfo(url, mime_type, "",
                                                       allow_wildcard, &info,
                                                       NULL);
}

void PluginService::Shutdown() {
  plugin_shutdown_handler_->InitiateShutdown();
}

void PluginService::OnShutdown() {
  PluginMap::iterator host_index;
  for (host_index = plugin_hosts_.begin(); host_index != plugin_hosts_.end();
          ++host_index) {
    host_index->second->Shutdown();
  }
}

PluginProcessHostIterator::PluginProcessHostIterator()
    : iterator_(PluginService::GetInstance()->plugin_hosts_.begin()),
      end_(PluginService::GetInstance()->plugin_hosts_.end()) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO)) <<
             "PluginProcessHostIterator must be used on the IO thread.";
}

PluginProcessHostIterator::PluginProcessHostIterator(
      const PluginProcessHostIterator& instance)
    : iterator_(instance.iterator_) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO)) <<
             "PluginProcessHostIterator must be used on the IO thread.";
}

void PluginService::ShutdownHandler::InitiateShutdown() {
  g_browser_process->io_thread()->message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &ShutdownHandler::OnShutdown));
}

void PluginService::ShutdownHandler::OnShutdown() {
  PluginService* plugin_service = PluginService::GetInstance();
  if (plugin_service) {
    plugin_service->OnShutdown();
  }
}

