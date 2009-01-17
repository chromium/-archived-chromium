// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/mock_render_thread.h"

#include "base/logging.h"
#include "chrome/common/ipc_message_utils.h"
#include "chrome/common/render_messages.h"
#include "testing/gtest/include/gtest/gtest.h"

MockRenderThread::MockRenderThread()
    : routing_id_(0),
      opener_id_(0),
      widget_(NULL),
      reply_deserializer_(NULL) {
}

MockRenderThread::~MockRenderThread() {
}

// Called by the Widget. The routing_id must match the routing id assigned
// to the Widget in reply to ViewHostMsg_CreateWidget message.
void MockRenderThread::AddRoute(int32 routing_id,
                                IPC::Channel::Listener* listener) {
  EXPECT_EQ(routing_id_, routing_id);
  widget_ = listener;
}

// Called by the Widget. The routing id must match the routing id of AddRoute.
void MockRenderThread::RemoveRoute(int32 routing_id) {
  EXPECT_EQ(routing_id_, routing_id);
  widget_ = NULL;
}

// Called by the Widget. Used to send messages to the browser.
// We short-circuit the mechanim and handle the messages right here on this
// class.
bool MockRenderThread::Send(IPC::Message* msg) {
  // We need to simulate a synchronous channel, thus we are going to receive
  // through this function messages, messages with reply and reply messages.
  // We can only handle one synchronous message at a time.
  if (msg->is_reply()) {
    if (reply_deserializer_.get()) {
      reply_deserializer_->SerializeOutputParameters(*msg);
      reply_deserializer_.reset();
    }
  } else {
    if (msg->is_sync()) {
      // We actually need to handle deleting the reply dersializer for sync
      // messages.
      reply_deserializer_.reset(
          static_cast<IPC::SyncMessage*>(msg)->GetReplyDeserializer());
    }
    OnMessageReceived(*msg);
  }
  delete msg;
  return true;
}

void MockRenderThread::SendCloseMessage() {
  ViewMsg_Close msg(routing_id_);
  widget_->OnMessageReceived(msg);
}

void MockRenderThread::OnMessageReceived(const IPC::Message& msg) {
  // Save the message in the sink.
  sink_.OnMessageReceived(msg);

  // Some messages we do special handling.
  bool handled = true;
  bool msg_is_ok = true;
  IPC_BEGIN_MESSAGE_MAP_EX(MockRenderThread, msg, msg_is_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWidget, OnMsgCreateWidget);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
}

// The Widget expects to be returned valid route_id.
void MockRenderThread::OnMsgCreateWidget(int opener_id,
                                         bool activatable,
                                         int* route_id) {
  opener_id_ = opener_id;
  *route_id = routing_id_;
}
