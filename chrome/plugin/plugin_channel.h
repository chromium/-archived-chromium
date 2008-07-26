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

#ifndef CHROME_PLUGIN_PLUGIN_CHANNEL_H__
#define CHROME_PLUGIN_PLUGIN_CHANNEL_H__

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

  DISALLOW_EVIL_CONSTRUCTORS(PluginChannel);
};

#endif  // CHROME_PLUGIN_PLUGIN_CHANNEL_H__
