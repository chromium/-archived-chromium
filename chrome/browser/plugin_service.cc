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

#include "chrome/browser/plugin_service.h"

#include "base/singleton.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_plugin_host.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/plugin_process_host.h"
#include "chrome/browser/render_process_host.h"
#include "chrome/browser/resource_message_filter.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/logging_chrome.h"
#include "webkit/glue/plugins/plugin_list.h"

// static
PluginService* PluginService::GetInstance() {
  return Singleton<PluginService>::get();
}

PluginService::PluginService()
    : main_message_loop_(MessageLoop::current()),
      ui_locale_(g_browser_process->GetApplicationLocale()),
      resource_dispatcher_host_(NULL),
      plugin_shutdown_handler_(new ShutdownHandler) {
  // Have the NPAPI plugin list search for Chrome plugins as well.
  ChromePluginLib::RegisterPluginsWithNPAPI();
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

void PluginService::SetChromePluginDataDir(const std::wstring& data_dir) {
  AutoLock lock(lock_);
  chrome_plugin_data_dir_ = data_dir;
}

const std::wstring& PluginService::GetChromePluginDataDir() {
  AutoLock lock(lock_);
  return chrome_plugin_data_dir_;
}

const std::wstring& PluginService::GetUILocale() {
  return ui_locale_;
}

PluginProcessHost* PluginService::FindPluginProcess(const std::wstring& dll) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));

  if (dll.empty()) {
    NOTREACHED() << "should only be called if we have a plugin dll to load";
    return NULL;
  }

  PluginMap::iterator found = plugin_hosts_.find(dll);
  if (found != plugin_hosts_.end())
    return found->second;
  return NULL;
}

PluginProcessHost* PluginService::FindOrStartPluginProcess(
    const std::wstring& dll,
    const std::string& clsid) {
  DCHECK(MessageLoop::current() ==
         ChromeThread::GetMessageLoop(ChromeThread::IO));

  PluginProcessHost *plugin_host = FindPluginProcess(dll);
  if (plugin_host)
    return plugin_host;

  // This plugin isn't loaded by any plugin process, so create a new process.
  plugin_host = new PluginProcessHost(this);
  if (!plugin_host->Init(dll, clsid, ui_locale_)) {
    DCHECK(false);  // Init is not expected to fail
    delete plugin_host;
    return NULL;
  }
  plugin_hosts_[dll] = plugin_host;
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
  std::wstring dll = GetPluginPath(url, mime_type, clsid, NULL);
  PluginProcessHost* plugin_host = FindOrStartPluginProcess(dll, clsid);
  if (plugin_host) {
    plugin_host->OpenChannelToPlugin(renderer_msg_filter, mime_type, reply_msg);
  } else {
    PluginProcessHost::ReplyToRenderer(renderer_msg_filter,
                                       std::wstring(),
                                       std::wstring(),
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
  // Search for the instance rather than lookup by dll path,
  // there is a small window where two instances for the same
  // dll path can co-exists.
  PluginMap::iterator i = plugin_hosts_.begin();
  while (i != plugin_hosts_.end()) {
    if (i->second == host) {
      plugin_hosts_.erase(i);
      return;
    }
    i++;
  }
}

std::wstring PluginService::GetPluginPath(const GURL& url,
                                          const std::string& mime_type,
                                          const std::string& clsid,
                                          std::string* actual_mime_type) {
  AutoLock lock(lock_);
  bool allow_wildcard = true;
  WebPluginInfo info;
  NPAPI::PluginList::Singleton()->GetPluginInfo(url, mime_type, clsid,
                                                allow_wildcard, &info,
                                                actual_mime_type);
  return info.file;
}

bool PluginService::GetPluginInfoByDllPath(const std::wstring& dll_path,
                                           WebPluginInfo* info) {
  AutoLock lock(lock_);
  return NPAPI::PluginList::Singleton()->GetPluginInfoByDllPath(dll_path, info);
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
