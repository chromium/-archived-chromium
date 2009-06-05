// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class responds to requests from renderers for the list of plugins, and
// also a proxy object for plugin instances.

#ifndef CHROME_BROWSER_PLUGIN_SERVICE_H_
#define CHROME_BROWSER_PLUGIN_SERVICE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/lock.h"
#include "base/ref_counted.h"
#include "base/singleton.h"
#include "base/waitable_event_watcher.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/webplugininfo.h"

#if defined(OS_WIN)
#include "base/registry.h"
#endif

namespace IPC {
class Message;
}

class MessageLoop;
class PluginProcessHost;
class URLRequestContext;
class ResourceDispatcherHost;
class ResourceMessageFilter;

// This can be called on the main thread and IO thread.  However it must
// be created on the main thread.
class PluginService
    : public base::WaitableEventWatcher::Delegate,
      public NotificationObserver {
 public:
  // Returns the PluginService singleton.
  static PluginService* GetInstance();

  // Gets the list of available plugins.
  void GetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins);

  // Load all the plugins that should be loaded for the lifetime of the browser
  // (ie, with the LoadOnStartup flag set).
  void LoadChromePlugins(ResourceDispatcherHost* resource_dispatcher_host);

  // Sets/gets the data directory that Chrome plugins should use to store
  // persistent data.
  void SetChromePluginDataDir(const FilePath& data_dir);
  const FilePath& GetChromePluginDataDir();

  // Gets the browser's UI locale.
  const std::wstring& GetUILocale();

  // Returns the plugin process host corresponding to the plugin process that
  // has been started by this service. Returns NULL if no process has been
  // started.
  PluginProcessHost* FindPluginProcess(const FilePath& plugin_path);

  // Returns the plugin process host corresponding to the plugin process that
  // has been started by this service. This will start a process to host the
  // 'plugin_path' if needed. If the process fails to start, the return value
  // is NULL. Must be called on the IO thread.
  PluginProcessHost* FindOrStartPluginProcess(const FilePath& plugin_path,
                                              const std::string& clsid);

  // Opens a channel to a plugin process for the given mime type, starting
  // a new plugin process if necessary.  This must be called on the IO thread
  // or else a deadlock can occur.
  void OpenChannelToPlugin(ResourceMessageFilter* renderer_msg_filter,
                           const GURL& url,
                           const std::string& mime_type,
                           const std::string& clsid,
                           const std::wstring& locale,
                           IPC::Message* reply_msg);

  bool HavePluginFor(const std::string& mime_type, bool allow_wildcard);

  // Get the path to the plugin specified.  policy_url is the URL of the page
  // requesting the plugin, so we can verify whether the plugin is allowed
  // on that page.
  FilePath GetPluginPath(const GURL& url,
                         const GURL& policy_url,
                         const std::string& mime_type,
                         const std::string& clsid,
                         std::string* actual_mime_type);

  // The UI thread's message loop
  MessageLoop* main_message_loop() { return main_message_loop_; }

  ResourceDispatcherHost* resource_dispatcher_host() const {
    return resource_dispatcher_host_;
  }

 private:
  friend struct DefaultSingletonTraits<PluginService>;

  // Creates the PluginService object, but doesn't actually build the plugin
  // list yet.  It's generated lazily.
  PluginService();
  ~PluginService();

  // base::WaitableEventWatcher::Delegate implementation.
  virtual void OnWaitableEventSignaled(base::WaitableEvent* waitable_event);

  // NotificationObserver implementation
  virtual void Observe(NotificationType type, const NotificationSource& source,
                       const NotificationDetails& details);

  // Get plugin info by matching full path.
  bool GetPluginInfoByPath(const FilePath& plugin_path, WebPluginInfo* info);

  // Returns true if the given plugin is allowed to be used by a page with
  // the given URL.
  bool PluginAllowedForURL(const FilePath& plugin_path, const GURL& url);

  // mapping between plugin path and PluginProcessHost
  typedef base::hash_map<FilePath, PluginProcessHost*> PluginMap;
  PluginMap plugin_hosts_;

  // The main thread's message loop.
  MessageLoop* main_message_loop_;

  // The IO thread's resource dispatcher host.
  ResourceDispatcherHost* resource_dispatcher_host_;

  // The data directory that Chrome plugins should use to store persistent data.
  FilePath chrome_plugin_data_dir_;

  // The browser's UI locale.
  const std::wstring ui_locale_;

  // Map of plugin paths to the origin they are restricted to.  Used for
  // extension-only plugins.
  typedef base::hash_map<FilePath, GURL> PrivatePluginMap;
  PrivatePluginMap private_plugins_;

  // Need synchronization whenever we access the plugin_list singelton through
  // webkit_glue since this class is called on the main and IO thread.
  Lock lock_;

  NotificationRegistrar registrar_;

#if defined(OS_WIN)
  // Registry keys for getting notifications when new plugins are installed.
  RegKey hkcu_key_;
  RegKey hklm_key_;
  scoped_ptr<base::WaitableEvent> hkcu_event_;
  scoped_ptr<base::WaitableEvent> hklm_event_;
  base::WaitableEventWatcher hkcu_watcher_;
  base::WaitableEventWatcher hklm_watcher_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PluginService);
};

#endif  // CHROME_BROWSER_PLUGIN_SERVICE_H_
