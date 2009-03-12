// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PLUGIN_PLUGIN_THREAD_H_
#define CHROME_PLUGIN_PLUGIN_THREAD_H_

#include "base/file_path.h"
#include "chrome/common/child_thread.h"
#include "chrome/common/resource_dispatcher.h"
#include "chrome/plugin/plugin_channel.h"

class NotificationService;

// The PluginThread class represents a background thread where plugin instances
// live.  Communication occurs between WebPluginDelegateProxy in the renderer
// process and WebPluginDelegateStub in this thread through IPC messages.
class PluginThread : public ChildThread {
 public:
  PluginThread();
  ~PluginThread();

  // Returns the one plugin thread.
  static PluginThread* current();

  // Returns the one true dispatcher.
  ResourceDispatcher* resource_dispatcher() {
    return resource_dispatcher_.get();
  }

 private:
  virtual void OnControlMessageReceived(const IPC::Message& msg);

  // Thread implementation:
  virtual void Init();
  virtual void CleanUp();

  void OnCreateChannel();
  void OnShutdownResponse(bool ok_to_shutdown);
  void OnPluginMessage(const std::vector<uint8> &data);
  void OnBrowserShutdown();

  scoped_ptr<NotificationService> notification_service_;

  // Handles resource loads for this view.
  // NOTE: this object lives on the owner thread.
  scoped_refptr<ResourceDispatcher> resource_dispatcher_;

  // The plugin module which is preloaded in Init
  HMODULE preloaded_plugin_module_;

  // Points to the plugin file that this process hosts.
  FilePath plugin_path_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginThread);
};

#endif  // CHROME_PLUGIN_PLUGIN_THREAD_H_
