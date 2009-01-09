// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MOCK_RENDER_THREAD_H_
#define CHROME_RENDERER_MOCK_RENDER_THREAD_H_

#include <vector>

#include "chrome/renderer/render_thread.h"

// This class is very simple mock of RenderThread. It simulates an IPC channel
// which supports only two messages:
// ViewHostMsg_CreateWidget : sync message sent by the Widget.
// ViewMsg_Close : async, send to the Widget.
class MockRenderThread : public RenderThreadBase {
 public:
  // Encapusulates an IPC message and its associated data (which is not
  // otherwise bound to the lifetime of the message).
  typedef std::pair<IPC::Message, char*> MessagePair;

  MockRenderThread();
  virtual ~MockRenderThread();

  // Called by the Widget. Not used in the test.
  virtual bool InSend() const {
    return false;
  }

  // Called by the Widget. The routing_id must match the routing id assigned
  // to the Widget in reply to ViewHostMsg_CreateWidget message.
  virtual void AddRoute(int32 routing_id, IPC::Channel::Listener* listener);

  // Called by the Widget. The routing id must match the routing id of AddRoute.
  virtual void RemoveRoute(int32 routing_id);

  // Called by the Widget. Used to send messages to the browser.
  // We short-circuit the mechanim and handle the messages right here on this
  // class.
  virtual bool Send(IPC::Message* msg);

  // Our mock thread doesn't do filtering.
  virtual void AddFilter(IPC::ChannelProxy::MessageFilter* filter) {
  }
  virtual void RemoveFilter(IPC::ChannelProxy::MessageFilter* filter) {
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

  // Returns the number of messages in the queue.
  size_t message_count() const { return messages_.size(); }

  // Clears the message queue of saved messages.
  void ClearMessages();

  // Returns the message at the given index in the queue. The index may be out
  // of range, in which case the return value is NULL. The returned pointer will
  // only be valid until another message is received or the list is cleared.
  const IPC::Message* GetMessageAt(size_t index) const;

  // Returns the first message with the given ID in the queue. If there is no
  // message with the given ID, returns NULL. The returned pointer will only be
  // valid until another message is received or the list is cleared.
  const IPC::Message* GetFirstMessageMatching(uint16 id) const;

  // Returns the message with the given ID in the queue. If there is no such
  // message or there is more than one of that message, this will return NULL
  // (with the expectation that you'll do an ASSERT_TRUE() on the result).
  // The returned pointer will only be valid until another message is received
  // or the list is cleared.
  const IPC::Message* GetUniqueMessageMatching(uint16 id) const;

  // Simulates the Widget receiving a close message. This should result
  // on releasing the internal reference counts and destroying the internal
  // state.
  void SendCloseMessage();

 private:
  // This function operates as a regular IPC listener.
  void OnMessageReceived(const IPC::Message& msg);

  // The Widget expects to be returned valid route_id.
  void OnMsgCreateWidget(int opener_id,
                         bool activatable,
                         int* route_id);

  // Routing id what will be assigned to the Widget.
  int32 routing_id_;

  // Opener id reported by the Widget.
  int32 opener_id_;

  // We only keep track of one Widget, we learn its pointer when it
  // adds a new route.
  IPC::Channel::Listener* widget_;

  // The last known good deserializer for sync messages.
  scoped_ptr<IPC::MessageReplyDeserializer> reply_deserializer_;

  std::vector<MessagePair> messages_;
};

#endif  // CHROME_RENDERER_MOCK_RENDER_THREAD_H_
