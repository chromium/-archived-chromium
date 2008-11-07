// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PLUGIN_PLUGIN_THREAD_H_
#define CHROME_PLUGIN_PLUGIN_THREAD_H_

#include "base/thread.h"
#include "chrome/common/ipc_sync_channel.h"
#include "chrome/common/message_router.h"
#include "chrome/common/resource_dispatcher.h"
#include "chrome/plugin/plugin_channel.h"

class PluginProcess;
class NotificationService;

// The PluginThread class represents a background thread where plugin instances
// live.  Communication occurs between WebPluginDelegateProxy in the renderer
// process and WebPluginDelegateStub in this thread through IPC messages.
class PluginThread : public IPC::Channel::Listener,
                     public IPC::Message::Sender,
                     public base::Thread {
 public:
  PluginThread(PluginProcess *process, const std::wstring& channel_name);
  ~PluginThread();

  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelError();

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

  // Returns the one plugin thread.
  static PluginThread* GetPluginThread() { return plugin_thread_; }

  // Returns the one true dispatcher.
  ResourceDispatcher* resource_dispatcher() {
    return resource_dispatcher_.get();
  }

 private:
   // Thread implementation:
  void Init();
  void CleanUp();

  void OnCreateChannel(int process_id, HANDLE renderer_handle);
  void OnShutdownResponse(bool ok_to_shutdown);
  void OnPluginMessage(const std::vector<uint8> &data);
  void OnBrowserShutdown();

  // The process that has created this thread
  PluginProcess *plugin_process_;

  // The message loop used to run tasks on the thread that started this thread.
  MessageLoop* owner_loop_;

  std::wstring channel_name_;
  scoped_ptr<IPC::SyncChannel> channel_;

  scoped_ptr<NotificationService> notification_service_;

  // Handles resource loads for this view.
  // NOTE: this object lives on the owner thread.
  scoped_refptr<ResourceDispatcher> resource_dispatcher_;

  // The plugin module which is preloaded in Init
  HMODULE preloaded_plugin_module_;

  // Points to the one PluginThread object in the process.
  static PluginThread* plugin_thread_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginThread);
};

#endif  // CHROME_PLUGIN_PLUGIN_THREAD_H_
