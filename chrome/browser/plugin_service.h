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

// This class responds to requests from renderers for the list of plugins, and
// also a proxy object for plugin instances.

#ifndef CHROME_BROWSER_PLUGIN_SERVICE_H__
#define CHROME_BROWSER_PLUGIN_SERVICE_H__

#include <vector>
#include <hash_map>

#include "base/basictypes.h"
#include "base/lock.h"
#include "chrome/browser/browser_process.h"
#include "webkit/glue/webplugin.h"

namespace IPC {
class Message;
}

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

  // Creates the PluginService object, but doesn't actually build the plugin
  // list yet.  It's generated lazily.
  // Note: don't call these directly - use GetInstance() above.  They are public
  // so Singleton can access them.
  PluginService();
  ~PluginService();

  // Gets the list of available plugins.
  void GetPlugins(bool refresh, std::vector<WebPluginInfo>* plugins);

  // Load all the plugins that should be loaded for the lifetime of the browser
  // (ie, with the LoadOnStartup flag set).
  void LoadChromePlugins(ResourceDispatcherHost* resource_dispatcher_host);

  // Sets/gets the data directory that Chrome plugins should use to store
  // persistent data.
  void SetChromePluginDataDir(const std::wstring& data_dir);
  const std::wstring& GetChromePluginDataDir();

  // Gets the browser's UI locale.
  const std::wstring& GetUILocale();

  // Returns the plugin process host corresponding to the plugin process that
  // has been started by this service. Returns NULL if no process has been
  // started.
  PluginProcessHost* FindPluginProcess(const std::wstring& dll);

  // Returns the plugin process host corresponding to the plugin process that
  // has been started by this service. This will start a process to host the
  // 'dll' if needed. If the process fails to start, the return value is NULL.
  // Must be called on the IO thread.
  PluginProcessHost* FindOrStartPluginProcess(const std::wstring& dll,
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

  // A PluginProcessHost object calls this before its process is shut down.
  void OnPluginProcessIsShuttingDown(PluginProcessHost* host);

  // A PluginProcessHost object calls this after its process has exited. This
  // call deletes the host instance.
  void OnPluginProcessExited(PluginProcessHost* host);

  bool HavePluginFor(const std::string& mime_type, bool allow_wildcard);

  std::wstring GetPluginPath(const GURL& url,
                             const std::string& mime_type,
                             const std::string& clsid,
                             std::string* actual_mime_type);

  // Get plugin info by matching full dll path.
  bool GetPluginInfoByDllPath(const std::wstring& dll_path,
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
  friend class PluginProcessHostIterator;

  // Removes a host from the plugin_hosts collection
  void RemoveHost(PluginProcessHost* host);

  // Shutdown handler which executes in the context of the IO thread.
  void OnShutdown();

  // mapping between plugin dll path and PluginProcessHost
  typedef stdext::hash_map<std::wstring, PluginProcessHost*> PluginMap;
  PluginMap plugin_hosts_;

  // The main thread's message loop.
  MessageLoop* main_message_loop_;

  // The IO thread's resource dispatcher host.
  ResourceDispatcherHost* resource_dispatcher_host_;

  // The data directory that Chrome plugins should use to store persistent data.
  std::wstring chrome_plugin_data_dir_;

  // The browser's UI locale.
  const std::wstring ui_locale_;

  // Need synchronization whenever we access the plugin_list singelton through
  // webkit_glue since this class is called on the main and IO thread.
  Lock lock_;

  // Handles plugin process shutdown.
  class ShutdownHandler :
      public base::RefCountedThreadSafe<ShutdownHandler> {
   public:
     ShutdownHandler() {}
     ~ShutdownHandler() {}

     // Initiates plugin process shutdown. Ensures that the actual shutdown
     // happens on the io thread.
     void InitiateShutdown();

   private:
     // Shutdown handler which runs on the io thread.
     void OnShutdown();

     DISALLOW_EVIL_CONSTRUCTORS(ShutdownHandler);
  };

  friend class ShutdownHandler;
  scoped_refptr<ShutdownHandler> plugin_shutdown_handler_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginService);
};

// The PluginProcessHostIterator allows to iterate through all the
// PluginProcessHosts. Note that this should be done from the IO thread and that
// the iterator should not be kept around as it may be invalidated on
// subsequent event processing in the event loop.
class PluginProcessHostIterator {
 public:
  PluginProcessHostIterator();
  PluginProcessHostIterator(const PluginProcessHostIterator& instance);

  PluginProcessHostIterator& operator=(
      const PluginProcessHostIterator& instance) {
      iterator_ = instance.iterator_;
  }

  const PluginProcessHost* operator->() const {
    return iterator_->second;
  }

  const PluginProcessHost* operator*() const {
    return iterator_->second;
  }

  const PluginProcessHost* operator++() { // ++preincrement
    ++iterator_;
    if (iterator_ == end_)
      return NULL;
    else
      return iterator_->second;
  }

  const PluginProcessHost* operator++(int) { // postincrement++
    const PluginProcessHost* r;
    if (iterator_ == end_)
      r = NULL;
    else
      r = iterator_->second;
    iterator_++;
    return r;
  }

  bool Done() {
    return (iterator_ == end_);
  }

 private:
  PluginService::PluginMap::const_iterator iterator_;
  PluginService::PluginMap::const_iterator end_;
};

#endif  // CHROME_BROWSER_PLUGIN_SERVICE_H__
