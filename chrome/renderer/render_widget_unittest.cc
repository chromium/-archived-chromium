// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/ref_counted.h"
#include "chrome/renderer/render_widget.h"
#include "chrome/renderer/render_thread.h"

// This class is very simple mock of RenderThread. It simulates an IPC channel
// which supports only two messages:
// ViewHostMsg_CreateWidget : sync message sent by the Widget.
// ViewMsg_Close : async, send to the Widget.
class MockRenderThread : public RenderThreadBase {
 public:
  MockRenderThread() 
      : routing_id_(0), opener_id_(0), widget_(NULL),
        reply_deserializer_(NULL) {
  }
  virtual ~MockRenderThread() {
  }

  // Called by the Widget. Not used in the test.
  virtual bool InSend() const {
    return false;
  }

  // Called by the Widget. The routing_id must match the routing id assigned
  // to the Widget in reply to ViewHostMsg_CreateWidget message.
  virtual void AddRoute(int32 routing_id, IPC::Channel::Listener* listener) {
    EXPECT_EQ(routing_id_, routing_id);
    widget_ = listener;
  }

  // Called by the Widget. The routing id must match the routing id of AddRoute.
  virtual void RemoveRoute(int32 routing_id) {
    EXPECT_EQ(routing_id_, routing_id);
    widget_ = NULL;
  }

  // Called by the Widget. Used to send messages to the browser.
  // We short-circuit the mechanim and handle the messages right here on this
  // class.
  virtual bool Send(IPC::Message* msg) {
    // We need to simulate a synchronous channel, thus we are going to receive
    // through this function messages, messages with reply and reply messages.
    // We can only handle one synchronous message at a time.
    if (msg->is_reply()) {
      if (reply_deserializer_)
        reply_deserializer_->SerializeOutputParameters(*msg);
        reply_deserializer_ = NULL;
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

  //////////////////////////////////////////////////////////////////////////
  // The following functions are called by the test itself.

  void set_routing_id(int32 id) {
    routing_id_ = id;
  }

  int32 opener_id() const {
    return opener_id_;
  }

  bool has_widget() const {
    return widget_ ? true : false;
  }

  // Simulates the Widget receiving a close message. This should result
  // on releasing the internal reference counts and destroying the internal
  // state.
  void SendCloseMessage() {
    ViewMsg_Close msg(routing_id_);
    widget_->OnMessageReceived(msg);
  }

 private:

  // This function operates as a regular IPC listener.
  void OnMessageReceived(const IPC::Message& msg) {
    bool handled = true;
    bool msg_is_ok = true;
    IPC_BEGIN_MESSAGE_MAP_EX(MockRenderThread, msg, msg_is_ok)
      IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWidget, OnMsgCreateWidget);
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP_EX()

    // This test must handle all messages that RenderWidget generates.
    EXPECT_TRUE(handled);
  }

  // The Widget expects to be returned valid route_id.
  void OnMsgCreateWidget(int opener_id, int* route_id) {
    opener_id_ = opener_id;
    *route_id = routing_id_;
  }

  // Routing id what will be assigned to the Widget.
  int32 routing_id_;
  // Opener id reported by the Widget.
  int32 opener_id_;
  // We only keep track of one Widget, we learn its pointer when it
  // adds a new route.
  IPC::Channel::Listener* widget_;
  // The last known good deserializer for sync messages.
  IPC::MessageReplyDeserializer* reply_deserializer_;
};

TEST(RenderWidgetTest, DISABLED_CreateAndCloseWidget) {
  MessageLoop msg_loop;
  MockRenderThread render_thread;

  const int32 kRouteId = 5;
  const int32 kOpenerId = 7;

  render_thread.set_routing_id(kRouteId);
  scoped_refptr<RenderWidget> rw =
      RenderWidget::Create(kOpenerId, &render_thread);
  ASSERT_TRUE(rw != NULL);

  // After the RenderWidget it must have sent a message to the render thread
  // that sets the opener id.
  EXPECT_EQ(kOpenerId, render_thread.opener_id());
  ASSERT_TRUE(render_thread.has_widget());

  // Now simulate a close of the Widget.
  render_thread.SendCloseMessage();
  EXPECT_FALSE(render_thread.has_widget());  
}

