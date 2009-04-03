// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_WEBKIT_GLUE_PLUGIN_CHANNEL_BASE_H__
#define CHROME_WEBKIT_GLUE_PLUGIN_CHANNEL_BASE_H__

#include <string>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/common/ipc_sync_channel.h"
#include "chrome/common/message_router.h"

// Encapsulates an IPC channel between a renderer and a plugin process.
class PluginChannelBase : public IPC::Channel::Listener,
                          public IPC::Message::Sender,
                          public base::RefCountedThreadSafe<PluginChannelBase> {
 public:
  virtual ~PluginChannelBase();

  // WebPlugin[Delegate] call these on construction and destruction to setup
  // the routing and manage lifetime of this object.  This is also called by
  // NPObjectProxy and NPObjectStub.  However the latter don't control the
  // lifetime of this object (by passing true for npobject) because we don't
  // want a leak of an NPObject in a plugin to keep the channel around longer
  // than necessary.
  void AddRoute(int route_id, IPC::Channel::Listener* listener, bool npobject);
  void RemoveRoute(int route_id);

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

  int peer_pid() { return peer_pid_; }
  std::wstring channel_name() const { return channel_name_; }

  // Returns the number of open plugin channels in this process.
  static int Count();

  // Returns a new route id.
  virtual int GenerateRouteID() = 0;

  // Returns whether the channel is valid or not. A channel is invalid
  // if it is disconnected due to a channel error.
  bool channel_valid() {
    return channel_valid_;
  }

  static void CleanupChannels();

 protected:
  typedef PluginChannelBase* (*PluginChannelFactory)();

  // Returns a PluginChannelBase derived object for the given channel name.
  // If an existing channel exists returns that object, otherwise creates a
  // new one.  Even though on creation the object is refcounted, each caller
  // must still ref count the returned value.  When there are no more routes
  // on the channel and its ref count is 0, the object deletes itself.
  static PluginChannelBase* GetChannel(
      const std::wstring& channel_name, IPC::Channel::Mode mode,
      PluginChannelFactory factory, MessageLoop* ipc_message_loop,
      bool create_pipe_now);

  // Called on the worker thread
  PluginChannelBase();

  virtual void CleanUp() { }

  // Implemented by derived classes to handle control messages
  virtual void OnControlMessageReceived(const IPC::Message& msg);

  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

  // If this is set, sync messages that are sent will only unblock the receiver
  // if this channel is in the middle of a dispatch.
  void SendUnblockingOnlyDuringDispatch();

  virtual bool Init(MessageLoop* ipc_message_loop, bool create_pipe_now);

  scoped_ptr<IPC::SyncChannel> channel_;

 private:

  IPC::Channel::Mode mode_;
  std::wstring channel_name_;
  int plugin_count_;
  int peer_pid_;

  // true when in the middle of a RemoveRoute call
  bool in_remove_route_;

  // Keep track of all the registered NPObjects proxies/stubs so that when the
  // channel is closed we can inform them.
  typedef base::hash_map<int, IPC::Channel::Listener*> ListenerMap;
  ListenerMap npobject_listeners_;

  // Used to implement message routing functionality to WebPlugin[Delegate]
  // objects
  MessageRouter router_;

  // A channel is invalid if it is disconnected as a result of a channel
  // error. This flag is used to indicate the same.
  bool channel_valid_;

  // If true, sync messages will only be marked as unblocking if the channel is
  // in the middle of dispatching a message.
  bool send_unblocking_only_during_dispatch_;
  int in_dispatch_;

  DISALLOW_EVIL_CONSTRUCTORS(PluginChannelBase);
};

#endif  // CHROME_WEBKIT_GLUE_PLUGIN_CHANNEL_BASE_H__
