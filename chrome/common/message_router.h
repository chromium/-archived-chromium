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

#ifndef CHROME_COMMON_MESSAGE_ROUTER_H__
#define CHROME_COMMON_MESSAGE_ROUTER_H__

#include "chrome/common/ipc_channel.h"

// The MessageRouter handles all incoming messages sent to it by routing them
// to the correct listener.  Routing is based on the Message's routing ID.
// Since routing IDs are typically assigned asynchronously by the browser
// process, the MessageRouter has the notion of pending IDs for listeners that
// have not yet been assigned a routing ID.
//
// When a message arrives, the routing ID is used to index the set of routes to
// find a listener.  If a listener is found, then the message is passed to it.
// Otherwise, the message is ignored if its routing ID is not equal to
// MSG_ROUTING_CONTROL.
//
// The MessageRouter supports the IPC::Message::Sender interface for outgoing
// messages, but does not define a meaningful implementation of it.  The
// subclass of MessageRouter is intended to provide that if appropriate.
//
// The MessageRouter can be used as a concrete class provided its Send method
// is not called and it does not receive any control messages.

class MessageRouter : public IPC::Channel::Listener,
                      public IPC::Message::Sender {
 public:
  MessageRouter() {}
  virtual ~MessageRouter() {}

  // Implemented by subclasses to handle control messages
  virtual void OnControlMessageReceived(const IPC::Message& msg);

  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(const IPC::Message& msg);

  // Like OnMessageReceived, except it only handles routed messages.  Returns
  // true if the message was dispatched, or false if there was no listener for
  // that route id.
  virtual bool RouteMessage(const IPC::Message& msg);

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

  // Called to add/remove a listener for a particular message routing ID.
  void AddRoute(int32 routing_id, IPC::Channel::Listener* listener);
  void RemoveRoute(int32 routing_id);

 private:
  // A list of all listeners with assigned routing IDs.
  IDMap<IPC::Channel::Listener> routes_;

  DISALLOW_EVIL_CONSTRUCTORS(MessageRouter);
};

#endif  // CHROME_COMMON_MESSAGE_ROUTER_H__
