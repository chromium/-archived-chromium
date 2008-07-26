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

#include "chrome/common/message_router.h"
#include "chrome/common/render_messages.h"

void MessageRouter::OnControlMessageReceived(const IPC::Message& msg) {
  NOTREACHED() << "should override in subclass if you care about control messages";
}

bool MessageRouter::Send(IPC::Message* msg) {
  NOTREACHED() << "should override in subclass if you care about sending messages";
  return false;
}

void MessageRouter::AddRoute(int32 routing_id,
                             IPC::Channel::Listener* listener) {
  routes_.AddWithID(listener, routing_id);
}

void MessageRouter::RemoveRoute(int32 routing_id) {
  routes_.Remove(routing_id);
}

void MessageRouter::OnMessageReceived(const IPC::Message& msg) {
  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    OnControlMessageReceived(msg);
  } else {
    RouteMessage(msg);
  }
}

bool MessageRouter::RouteMessage(const IPC::Message& msg) {
  IPC::Channel::Listener* listener = routes_.Lookup(msg.routing_id());
  if (!listener)
    return false;

  listener->OnMessageReceived(msg);
  return true;
}
