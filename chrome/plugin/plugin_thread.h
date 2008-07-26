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

#ifndef CHROME_PLUGIN_PLUGIN_THREAD_H__
#define CHROME_PLUGIN_PLUGIN_THREAD_H__

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
                     public Thread {
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
  ResourceDispatcher* resource_dispatcher() { return resource_dispatcher_.get(); }

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

#endif  // CHROME_PLUGIN_PLUGIN_THREAD_H__
