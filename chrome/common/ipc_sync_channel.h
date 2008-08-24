// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_SYNC_SENDER_H__
#define CHROME_COMMON_IPC_SYNC_SENDER_H__

#include <windows.h>
#include <string>
#include <stack>
#include <queue>
#include "base/basictypes.h"
#include "base/lock.h"
#include "base/ref_counted.h"
#include "chrome/common/ipc_channel_proxy.h"

namespace IPC {

class SyncMessage;

// This is similar to IPC::ChannelProxy, with the added feature of supporting
// sending synchronous messages.
// Note that care must be taken that the lifetime of the ipc_thread argument
// is more than this object.  If the message loop goes away while this object
// is running and it's used to send a message, then it will use the invalid
// message loop pointer to proxy it to the ipc thread.
class SyncChannel : public ChannelProxy {
 public:
  SyncChannel(const std::wstring& channel_id, Channel::Mode mode,
              Channel::Listener* listener, MessageFilter* filter,
              MessageLoop* ipc_message_loop, bool create_pipe_now,
              HANDLE shutdown_handle);
  ~SyncChannel();

  virtual bool Send(Message* message);
  virtual bool SendWithTimeout(Message* message, int timeout_ms);
  bool UnblockListener(Message* message);

  // Whether we allow sending messages with no time-out.
  void set_sync_messages_with_no_timeout_allowed(bool value) {
    sync_messages_with_no_timeout_allowed_ = value;
  }

 protected:
  class ReceivedSyncMsgQueue;
  friend class ReceivedSyncMsgQueue;

  // SyncContext holds the per object data for SyncChannel, so that SyncChannel
  // can be deleted while it's being used in a different thread.  See
  // ChannelProxy::Context for more information.
  class SyncContext : public Context {
   public:
    SyncContext(Channel::Listener* listener,
                MessageFilter* filter,
                MessageLoop* ipc_thread);

    ~SyncContext();

    // Adds information about an outgoing sync message to the context so that
    // we know how to deserialize the reply.  Returns a handle that's set when
    // the reply has arrived.
    HANDLE Push(IPC::SyncMessage* sync_msg);

    // Returns true if the reply message was deserialized without any errors,
    // or false otherwise.
    bool reply_deserialize_result() { return reply_deserialize_result_; }

    // Returns an event that's set when an incoming message that's not the reply
    // needs to get dispatched (by calling SyncContext::DispatchMessages).
    HANDLE blocking_event();

    void DispatchMessages();
    void RemoveListener(Channel::Listener* listener);

    // Checks if the given message is blocking the listener thread because of a
    // synchronous send.  If it is, the thread is unblocked and true is returned.
    // Otherwise the function returns false.
    bool UnblockListener(const Message* msg);

    // Cleanly remove the top deserializer (and throw it away).
    void PopDeserializer(bool close_reply_event);

   private:
    void OnMessageReceived(const Message& msg);
    void OnChannelError();

    // When sending a synchronous message, this structure contains an object that
    // knows how to deserialize the response.
    struct PendingSyncMsg {
      PendingSyncMsg(int id, IPC::MessageReplyDeserializer* d, HANDLE e) :
          id(id), deserializer(d), reply_event(e) { }
      int id;
      IPC::MessageReplyDeserializer* deserializer;
      HANDLE reply_event;
    };

    typedef std::stack<PendingSyncMsg> PendingSyncMessageQueue;
    PendingSyncMessageQueue deserializers_;
    Lock deserializers_lock_;

    // This can't be a scoped_refptr because it needs to be released on the
    // listener thread.
    ReceivedSyncMsgQueue* received_sync_msgs_;

    bool channel_closed_;
    bool reply_deserialize_result_;
  };

 private:
  SyncContext* sync_context() { return reinterpret_cast<SyncContext*>(context()); }

  // Copy of shutdown event that we get in constructor.
  HANDLE shutdown_event_;

  std::stack<HANDLE> pump_messages_events_;

  bool sync_messages_with_no_timeout_allowed_;

  DISALLOW_EVIL_CONSTRUCTORS(SyncChannel);
};

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_SYNC_SENDER_H__

