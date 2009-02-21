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
#include "chrome/browser/browser_process.h"
#include "webkit/glue/webplugin.h"

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
class PluginService {
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

  // Add an extra plugin directory to scan when we actually do the loading.
  // This will force a refresh of the plugin list the next time plugins are
  // requested.
  void AddExtraPluginDir(const FilePath& plugin_dir);

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

  FilePath GetPluginPath(const GURL& url,
                         const std::string& mime_type,
                         const std::string& clsid,
                         std::string* actual_mime_type);

  // Get plugin info by matching full path.
  bool GetPluginInfoByPath(const FilePath& plugin_path,
                           WebPluginInfo* info);

  // Returns true if the plugin's mime-type supports a given mime-type.
  // Checks for absolute matching and wildcards.  mime-types should be in
  // lower case.
  bool SupportsMimeType(const std::wstring& plugin_mime_type,
                        const std::wstring& mime_type);

  // The UI thread's message loop
  MessageLoop* main_message_loop() { return main_message_loop_; }

  ResourceDispatcherHost* resource_dispatcher_host() const {
    return resource_dispatcher_host_;
  }

  // Initiates shutdown on all running PluginProcessHost instances.
  // Can be invoked on the main thread.
  void Shutdown();

 private:
  friend struct DefaultSingletonTraits<PluginService>;

  // Creates the PluginService object, but doesn't actually build the plugin
  // list yet.  It's generated lazily.
  PluginService();
  ~PluginService();

  // Shutdown handler which executes in the context of the IO thread.
  void OnShutdown();

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

  // Need synchronization whenever we access the plugin_list singelton through
  // webkit_glue since this class is called on the main and IO thread.
  Lock lock_;

  // Handles plugin process shutdown.
  class ShutdownHandler : public base::RefCountedThreadSafe<ShutdownHandler> {
   public:
     ShutdownHandler() {}
     ~ShutdownHandler() {}

     // Initiates plugin process shutdown. Ensures that the actual shutdown
     // happens on the io thread.
     void InitiateShutdown();

   private:
    // Shutdown handler which runs on the io thread.
    void OnShutdown();

    DISALLOW_COPY_AND_ASSIGN(ShutdownHandler);
  };

  friend class ShutdownHandler;
  scoped_refptr<ShutdownHandler> plugin_shutdown_handler_;

  DISALLOW_COPY_AND_ASSIGN(PluginService);
};

#endif  // CHROME_BROWSER_PLUGIN_SERVICE_H_
