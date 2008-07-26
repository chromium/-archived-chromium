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

#ifndef CHROME_PLUGIN_PLUGIN_CHANNEL_HOST_H__
#define CHROME_PLUGIN_PLUGIN_CHANNEL_HOST_H__

#include "chrome/plugin/plugin_channel_base.h"

class IsListeningFilter;

// Encapsulates an IPC channel between the renderer and one plugin process.
// On the plugin side there's a corresponding PluginChannel.
class PluginChannelHost : public PluginChannelBase {
 public:
  static PluginChannelHost* GetPluginChannelHost(
      const std::wstring& channel_name, MessageLoop* ipc_message_loop);

  ~PluginChannelHost();

  virtual bool Init(MessageLoop* ipc_message_loop, bool create_pipe_now);

  int GenerateRouteID();

  void AddRoute(int route_id, IPC::Channel::Listener* listener, bool npobject);
  void RemoveRoute(int route_id);

  // IPC::Channel::Listener override
  void OnChannelError();

  static void SetListening(bool flag);

 private:
  // Called on the render thread
  PluginChannelHost();

  static PluginChannelBase* ClassFactory() { return new PluginChannelHost(); }

  // Keep track of all the registered WebPluginDelegeProxies to
  // inform about OnChannelError
  typedef stdext::hash_map<int, IPC::Channel::Listener*> ProxyMap;
  ProxyMap proxies_;

  // An IPC MessageFilter that can be told to filter out all messages. This is
  // used when the JS debugger is attached in order to avoid browser hangs.
  scoped_refptr<IsListeningFilter> is_listening_filter_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginChannelHost);
};

#endif  // CHROME_PLUGIN_PLUGIN_CHANNEL_HOST_H__
