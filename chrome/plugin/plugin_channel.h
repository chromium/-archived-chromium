// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PLUGIN_PLUGIN_CHANNEL_H_
#define CHROME_PLUGIN_PLUGIN_CHANNEL_H_

#include <vector>
#include "base/scoped_handle.h"
#include "chrome/plugin/plugin_channel_base.h"
#include "chrome/plugin/webplugin_delegate_stub.h"

// Encapsulates an IPC channel between the plugin process and one renderer
// process.  On the renderer side there's a corresponding PluginChannelHost.
class PluginChannel : public PluginChannelBase {
 public:
  // renderer_handle is the the handle to the renderer process requesting the
  // channel. The handle has to be valid in the context of the plugin process.
  static PluginChannel* GetPluginChannel(
      int process_id, HANDLE renderer_handle, MessageLoop* ipc_message_loop);

  ~PluginChannel();

  virtual bool Send(IPC::Message* msg);
  virtual void OnMessageReceived(const IPC::Message& message);

  HANDLE renderer_handle() { return renderer_handle_.Get(); }
  int GenerateRouteID();

  bool in_send() { return in_send_ != 0; }

 protected:
  // IPC::Channel::Listener implementation:
  virtual void OnChannelError();

  virtual void CleanUp();

 private:
  // Called on the plugin thread
  PluginChannel();

  void OnControlMessageReceived(const IPC::Message& msg);

  static PluginChannelBase* ClassFactory() { return new PluginChannel(); }

  void OnCreateInstance(const std::string& mime_type, int* instance_id);
  void OnDestroyInstance(int instance_id, IPC::Message* reply_msg);
  void OnGenerateRouteID(int* route_id);

  std::vector<scoped_refptr<WebPluginDelegateStub>> plugin_stubs_;

  // Handle to the renderer process who is on the other side of the channel.
  ScopedHandle renderer_handle_;

  int in_send_;  // Tracks if we're in a Send call.
  bool log_messages_;  // True if we should log sent and received messages.

  DISALLOW_EVIL_CONSTRUCTORS(PluginChannel);
};

#endif  // CHROME_PLUGIN_PLUGIN_CHANNEL_H_
