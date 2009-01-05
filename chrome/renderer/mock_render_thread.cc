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
  // Don't leak the allocated message bodies.
  ClearMessages();
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
    if (reply_deserializer_) {
      reply_deserializer_->SerializeOutputParameters(*msg);
      delete reply_deserializer_;
      reply_deserializer_ = NULL;
    }
  } else {
    if (msg->is_sync()) {
      reply_deserializer_ =
          static_cast<IPC::SyncMessage*>(msg)->GetReplyDeserializer();
    }
    OnMessageReceived(*msg);
  }
  delete msg;
  return true;
}

void MockRenderThread::ClearMessages() {
  for (size_t i = 0; i < messages_.size(); i++)
    delete[] messages_[i].second;
  messages_.clear();
}

const IPC::Message* MockRenderThread::GetMessageAt(size_t index) const {
  if (index >= messages_.size())
    return NULL;
  return &messages_[index].first;
}

const IPC::Message* MockRenderThread::GetFirstMessageMatching(uint16 id) const {
  for (size_t i = 0; i < messages_.size(); i++) {
    if (messages_[i].first.type() == id)
      return &messages_[i].first;
  }
  return NULL;
}

const IPC::Message* MockRenderThread::GetUniqueMessageMatching(
    uint16 id) const {
  size_t found_index = 0;
  size_t found_count = 0;
  for (size_t i = 0; i < messages_.size(); i++) {
    if (messages_[i].first.type() == id) {
      found_count++;
      found_index = i;
    }
  }
  if (found_count != 1)
    return NULL;  // Didn't find a unique one.
  return &messages_[found_index].first;
}

void MockRenderThread::SendCloseMessage() {
  ViewMsg_Close msg(routing_id_);
  widget_->OnMessageReceived(msg);
}

void MockRenderThread::OnMessageReceived(const IPC::Message& msg) {
  // Copy the message into a pair. This is tricky since the message doesn't
  // manage its data itself.
  char* data_copy;
  if (msg.size()) {
    data_copy = new char[msg.size()];
    memcpy(data_copy, msg.data(), msg.size());
  } else {
    // Dummy data so we can treat everything the same.
    data_copy = new char[1];
    data_copy[0] = 0;
  }

  // Save the message.
  messages_.push_back(
      std::make_pair(IPC::Message(data_copy, msg.size()), data_copy));

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
                                         bool focus_on_show,
                                         int* route_id) {
  opener_id_ = opener_id;
  *route_id = routing_id_;
}

