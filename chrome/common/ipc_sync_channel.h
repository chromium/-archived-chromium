// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_SYNC_SENDER_H__
#define CHROME_COMMON_IPC_SYNC_SENDER_H__

#include <windows.h>
#include <string>
#include <deque>
#include "base/basictypes.h"
#include "base/lock.h"
#include "base/object_watcher.h"
#include "base/ref_counted.h"
#include "base/scoped_handle.h"
#include "chrome/common/ipc_channel_proxy.h"

namespace IPC {

class SyncMessage;

// This is similar to IPC::ChannelProxy, with the added feature of supporting
// sending synchronous messages.
// Note that care must be taken that the lifetime of the ipc_thread argument
// is more than this object.  If the message loop goes away while this object
// is running and it's used to send a message, then it will use the invalid
// message loop pointer to proxy it to the ipc thread.
class SyncChannel : public ChannelProxy,
                    public base::ObjectWatcher::Delegate {
 public:
  SyncChannel(const std::wstring& channel_id, Channel::Mode mode,
              Channel::Listener* listener, MessageFilter* filter,
              MessageLoop* ipc_message_loop, bool create_pipe_now,
              HANDLE shutdown_event);
  ~SyncChannel();

  virtual bool Send(Message* message);
  virtual bool SendWithTimeout(Message* message, int timeout_ms);

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
  class SyncContext : public Context,
                      public base::ObjectWatcher::Delegate {
   public:
    SyncContext(Channel::Listener* listener,
                MessageFilter* filter,
                MessageLoop* ipc_thread,
                HANDLE shutdown_event);

    ~SyncContext();

    // Adds information about an outgoing sync message to the context so that
    // we know how to deserialize the reply.
    void Push(IPC::SyncMessage* sync_msg);

    // Cleanly remove the top deserializer (and throw it away).  Returns the
    // result of the Send call for that message.
    bool Pop();

    // Returns an event that's set when the send is complete, timed out or the
    // process shut down.
    HANDLE GetSendDoneEvent();

    // Returns an event that's set when an incoming message that's not the reply
    // needs to get dispatched (by calling SyncContext::DispatchMessages).
    HANDLE GetDispatchEvent();

    void DispatchMessages();

    // Checks if the given message is blocking the listener thread because of a
    // synchronous send.  If it is, the thread is unblocked and true is returned.
    // Otherwise the function returns false.
    bool TryToUnblockListener(const Message* msg);

    // Called on the IPC thread when a sync send that runs a nested message loop
    // times out.
    void OnSendTimeout(int message_id);

    HANDLE shutdown_event() { return shutdown_event_; }

   private:
    // IPC::ChannelProxy methods that we override.

    // Called on the listener thread.
   virtual void Clear();

    // Called on the IPC thread.
    virtual void OnMessageReceived(const Message& msg);
    virtual void OnChannelError();
    virtual void OnChannelOpened();
    virtual void OnChannelClosed();

    // Cancels all pending Send calls.
    void CancelPendingSends();

    // ObjectWatcher::Delegate implementation.
    virtual void OnObjectSignaled(HANDLE object);

    // When sending a synchronous message, this structure contains an object that
    // knows how to deserialize the response.
    struct PendingSyncMsg {
      PendingSyncMsg(int id, IPC::MessageReplyDeserializer* d, HANDLE e) :
          id(id), deserializer(d), done_event(e), send_result(false) { }
      int id;
      IPC::MessageReplyDeserializer* deserializer;
      HANDLE done_event;
      bool send_result;
    };

    typedef std::deque<PendingSyncMsg> PendingSyncMessageQueue;
    PendingSyncMessageQueue deserializers_;
    Lock deserializers_lock_;

    scoped_refptr<ReceivedSyncMsgQueue> received_sync_msgs_;

    HANDLE shutdown_event_;
    base::ObjectWatcher shutdown_watcher_;
  };

 private:
  // ObjectWatcher::Delegate implementation.
  virtual void OnObjectSignaled(HANDLE object);

  SyncContext* sync_context() { return reinterpret_cast<SyncContext*>(context()); }

  // Both these functions wait for a reply, timeout or process shutdown.  The
  // latter one also runs a nested message loop in the meantime.
  void WaitForReply(HANDLE pump_messages_event);

  // Runs a nested message loop until a reply arrives, times out, or the process
  // shuts down.
  void WaitForReplyWithNestedMessageLoop();

  bool sync_messages_with_no_timeout_allowed_;

  // Used to signal events between the IPC and listener threads.
  base::ObjectWatcher send_done_watcher_;
  base::ObjectWatcher dispatch_watcher_;

  DISALLOW_EVIL_CONSTRUCTORS(SyncChannel);
};

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_SYNC_SENDER_H__

