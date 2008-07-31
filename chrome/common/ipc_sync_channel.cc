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

#include <windows.h>

#include "chrome/common/ipc_sync_channel.h"

#include "base/logging.h"
#include "base/thread_local_storage.h"
#include "chrome/common/child_process.h"
#include "chrome/common/ipc_logging.h"
#include "chrome/common/ipc_sync_message.h"


namespace IPC {
// When we're blocked in a Send(), we need to process incoming synchronous
// messages right away because it could be blocking our reply (either
// directly from the same object we're calling, or indirectly through one or
// more other channels).  That means that in SyncContext's OnMessageReceived,
// we need to process sync message right away if we're blocked.  However a
// simple check isn't sufficient, because the listener thread can be in the
// process of calling Send.
// To work around this, when SyncChannel filters a sync message, it sets
// an event that the listener thread waits on during its Send() call.  This
// allows us to dispatch incoming sync messages when blocked.  The race
// condition is handled because if Send is in the process of being called, it
// will check the event.  In case the listener thread isn't sending a message,
// we queue a task on the listener thread to dispatch the received messages.
// The messages are stored in this queue object that's shared among all
// SyncChannel objects on the same thread (since one object can receive a
// sync message while another one is blocked).

// Holds a pointer to the per-thread ReceivedSyncMsgQueue object.
static int g_tls_index = ThreadLocalStorage::Alloc();

class SyncChannel::ReceivedSyncMsgQueue :
    public base::RefCountedThreadSafe<ReceivedSyncMsgQueue> {
 public:
  ReceivedSyncMsgQueue() :
      blocking_event_(CreateEvent(NULL, FALSE, FALSE, NULL)),
      task_pending_(false),
      listener_message_loop_(MessageLoop::current()) {
  }

  ~ReceivedSyncMsgQueue() {
    CloseHandle(blocking_event_);
    ThreadLocalStorage::Set(g_tls_index, NULL);
  }

  // Called on IPC thread when a synchronous message or reply arrives.
  void QueueMessage(const Message& msg, Channel::Listener* listener,
                    const std::wstring& channel_id) {
    bool was_task_pending;
    {
      AutoLock auto_lock(message_lock_);

      was_task_pending = task_pending_;
      task_pending_ = true;

      // We set the event in case the listener thread is blocked (or is about
      // to). In case it's not, the PostTask dispatches the messages.
      message_queue_.push(ReceivedMessage(new Message(msg), listener,
                                          channel_id));
    }

    SetEvent(blocking_event_);
    if (!was_task_pending) {
      listener_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
          this, &ReceivedSyncMsgQueue::DispatchMessagesTask));
    }
  }

  void QueueReply(const Message &msg, SyncChannel::SyncContext* context) {
    AutoLock auto_lock(reply_lock_);

    received_replies_.push_back(Reply(new Message(msg), context));
  }

  // Called on the listerner's thread to process any queues synchronous
  // messages.
  void DispatchMessagesTask() {
    {
      AutoLock auto_lock(message_lock_);
      task_pending_ = false;
    }
    DispatchMessages();
  }

  void DispatchMessages() {
    while (true) {
      Message* message = NULL;
      std::wstring channel_id;
      Channel::Listener* listener = NULL;
      {
        AutoLock auto_lock(message_lock_);
        if (message_queue_.empty())
          break;

        ReceivedMessage& blocking_msg = message_queue_.front();
        message = blocking_msg.message;
        listener = blocking_msg.listener;
        channel_id = blocking_msg.channel_id;
        message_queue_.pop();
      }

#ifdef IPC_MESSAGE_LOG_ENABLED
      IPC::Logging* logger = IPC::Logging::current();
      if (logger->Enabled())
        logger->OnPreDispatchMessage(*message);
#endif

      if (listener)
        listener->OnMessageReceived(*message);

#ifdef IPC_MESSAGE_LOG_ENABLED
      if (logger->Enabled())
        logger->OnPostDispatchMessage(*message, channel_id);
#endif

      delete message;
    }
  }

  // Called on the IPC thread when the current sync Send() call is unblocked.
  void OnUnblock() {
    bool queued_replies = false;
    {
      AutoLock auto_lock(reply_lock_);
      queued_replies = !received_replies_.empty();
    }

    if (queued_replies) {
      MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
          this, &ReceivedSyncMsgQueue::DispatchReplies));
    }
  }

  // SyncChannel calls this in its destructor.
  void RemoveListener(Channel::Listener* listener) {
    AutoLock auto_lock(message_lock_);

    SyncMessageQueue temp_queue;
    while (!message_queue_.empty()) {
      if (message_queue_.front().listener != listener) {
        temp_queue.push(message_queue_.front());
      } else {
        delete message_queue_.front().message;
      }

      message_queue_.pop();
    }

    while (!temp_queue.empty()) {
      message_queue_.push(temp_queue.front());
      temp_queue.pop();
    }
  }

  HANDLE blocking_event() { return blocking_event_; }

 private:
  // Called on the ipc thread to check if we can unblock any current Send()
  // calls based on a queued reply.
  void DispatchReplies() {
    AutoLock auto_lock(reply_lock_);

    for (size_t i = 0; i < received_replies_.size(); ++i) {
      Message* message = received_replies_[i].message;
      if (received_replies_[i].context->UnblockListener(message)) {
        delete message;
        received_replies_.erase(received_replies_.begin() + i);
        return;
      }
    }
  }

  // Set when we got a synchronous message that we must respond to as the
  // sender needs its reply before it can reply to our original synchronous
  // message.
  HANDLE blocking_event_;

  MessageLoop* listener_message_loop_;

  // Holds information about a queued synchronous message.
  struct ReceivedMessage {
    ReceivedMessage(Message* m, Channel::Listener* l, const std::wstring& i)
        : message(m), listener(l), channel_id(i) { }
    Message* message;
    Channel::Listener* listener;
    std::wstring channel_id;
  };

  typedef std::queue<ReceivedMessage> SyncMessageQueue;
  SyncMessageQueue message_queue_;
  Lock message_lock_;
  bool task_pending_;

  // Holds information about a queued reply message.
  struct Reply {
    Reply(Message* m, SyncChannel::SyncContext* c)
        : message(m),
          context(c) { }

    Message* message;
    scoped_refptr<SyncChannel::SyncContext> context;
  };

  std::vector<Reply> received_replies_;
  Lock reply_lock_;
};


SyncChannel::SyncContext::SyncContext(
    Channel::Listener* listener,
    MessageFilter* filter,
    MessageLoop* ipc_thread)
    : ChannelProxy::Context(listener, filter, ipc_thread),
      channel_closed_(false),
      reply_deserialize_result_(false) {
  // We want one ReceivedSyncMsgQueue per listener thread (i.e. since multiple
  // SyncChannel objects that can block the same thread).
  received_sync_msgs_ = static_cast<ReceivedSyncMsgQueue*>(
      ThreadLocalStorage::Get(g_tls_index));

  if (!received_sync_msgs_.get()) {
    // Stash a pointer to the listener thread's ReceivedSyncMsgQueue, as we
    // need to be able to access it in the IPC thread.
    received_sync_msgs_ = new ReceivedSyncMsgQueue();
    ThreadLocalStorage::Set(g_tls_index, received_sync_msgs_.get());
  }
}

SyncChannel::SyncContext::~SyncContext() {
  while (!deserializers_.empty())
    PopDeserializer();
}

// Adds information about an outgoing sync message to the context so that
// we know how to deserialize the reply.  Returns a handle that's set when
// the reply has arrived.
HANDLE SyncChannel::SyncContext::Push(IPC::SyncMessage* sync_msg) {
  PendingSyncMsg pending(IPC::SyncMessage::GetMessageId(*sync_msg),
                         sync_msg->GetReplyDeserializer(),
                         CreateEvent(NULL, FALSE, FALSE, NULL));
  AutoLock auto_lock(deserializers_lock_);
  deserializers_.push(pending);

  return pending.reply_event;
}

HANDLE SyncChannel::SyncContext::blocking_event() {
  return received_sync_msgs_->blocking_event();
}

void SyncChannel::SyncContext::DispatchMessages() {
  received_sync_msgs_->DispatchMessages();
}

void SyncChannel::SyncContext::RemoveListener(Channel::Listener* listener) {
  received_sync_msgs_->RemoveListener(listener);
}

bool SyncChannel::SyncContext::UnblockListener(const Message* msg) {
  bool rv = false;
  HANDLE reply_event = NULL;
  {
    AutoLock auto_lock(deserializers_lock_);
    if (channel_closed_) {
      // The channel is closed, or we couldn't connect, so cancel all Send()
      // calls.
      reply_deserialize_result_ = false;
      if (!deserializers_.empty()) {
        reply_event = deserializers_.top().reply_event;
        PopDeserializer();
      }
    } else {
      if (deserializers_.empty())
        return false;

      if (!IPC::SyncMessage::IsMessageReplyTo(*msg, deserializers_.top().id))
        return false;

      rv = true;
      if (msg->is_reply_error()) {
        reply_deserialize_result_ = false;
      } else {
        reply_deserialize_result_ =
            deserializers_.top().deserializer->SerializeOutputParameters(*msg);
      }

      // Can't CloseHandle the event just yet, since doing so might cause the
      // Wait call above to never return.
      reply_event = deserializers_.top().reply_event;
      PopDeserializer();
    }
  }

  if (reply_event)
    SetEvent(reply_event);

  // We got a reply to a synchronous Send() call that's blocking the listener
  // thread.  However, further down the call stack there could be another
  // blocking Send() call, whose reply we received after we made this last
  // Send() call.  So check if we have any queued replies available that
  // can now unblock the listener thread.
  received_sync_msgs_->OnUnblock();

  return rv;
}

// Called on the IPC thread.
void SyncChannel::SyncContext::OnMessageReceived(const Message& msg) {
  if (UnblockListener(&msg))
    return;

  if (msg.should_unblock()) {
    received_sync_msgs_->QueueMessage(msg, listener(), channel_id());
    return;
  }

  if (msg.is_reply()) {
    received_sync_msgs_->QueueReply(msg, this);
    return;
  }

  return Context::OnMessageReceived(msg);
}

// Called on the IPC thread.
void SyncChannel::SyncContext::OnChannelError() {
  channel_closed_ = true;
  UnblockListener(NULL);

  Context::OnChannelError();
}

void SyncChannel::SyncContext::PopDeserializer() {
  delete deserializers_.top().deserializer;
  deserializers_.pop();
}

SyncChannel::SyncChannel(const std::wstring& channel_id, Channel::Mode mode,
                         Channel::Listener* listener,
                         MessageLoop* ipc_message_loop,
                         bool create_pipe_now)
    : ChannelProxy(channel_id, mode, listener, NULL, ipc_message_loop,
                   new SyncContext(listener, NULL, ipc_message_loop),
                   create_pipe_now),
      shutdown_event_(ChildProcess::GetShutDownEvent()) {
  DCHECK(shutdown_event_ != NULL);
}

SyncChannel::~SyncChannel() {
  // The listener ensures that its lifetime is greater than SyncChannel.  But
  // after SyncChannel is destructed there's no guarantee that the listener is
  // still around, so we wouldn't want ReceivedSyncMsgQueue to call the
  // listener.
  sync_context()->RemoveListener(listener());
}

bool SyncChannel::Send(IPC::Message* message) {
  bool message_is_sync = message->is_sync();
  HANDLE pump_messages_event = NULL;

  HANDLE reply_event = NULL;
  if (message_is_sync) {
    IPC::SyncMessage* sync_msg = static_cast<IPC::SyncMessage*>(message);
    reply_event = sync_context()->Push(sync_msg);
    pump_messages_event = sync_msg->pump_messages_event();
  }

  // Send the message using the ChannelProxy
  ChannelProxy::Send(message);
  if (!message_is_sync)
    return true;

  do {
    // Wait for reply, or for any other incoming synchronous message.
    DCHECK(reply_event != NULL);
    HANDLE objects[] = { shutdown_event_,
                         reply_event,
                         sync_context()->blocking_event(),
                         pump_messages_event};

    DWORD result;
    if (pump_messages_event == NULL) {
      // No need to pump messages since we didn't get an event to check.
      result = WaitForMultipleObjects(3, objects, FALSE, INFINITE);
    } else {
      // If the event is set, then we pump messages.  Otherwise we also wait on
      // it so that if it gets set we start pumping messages.
      if (WaitForSingleObject(pump_messages_event, 0) == WAIT_OBJECT_0) {
        result = MsgWaitForMultipleObjects(3, objects, FALSE, INFINITE,
                                           QS_ALLINPUT);
      } else {
        result = WaitForMultipleObjects(4, objects, FALSE, INFINITE);
      }
    }

    if (result == WAIT_OBJECT_0) {
      // Process shut down before we can get a reply to a synchronous message.
      // Unblock the thread.
      // Leak reply_event. Since we're shutting down, it's not a big deal.
      return false;
    }

    if (result == WAIT_OBJECT_0 + 1) {
      // We got the reply to our synchronous message.
      CloseHandle(reply_event);
      return sync_context()->reply_deserialize_result();
    }

    if (result == WAIT_OBJECT_0 + 2) {
      // We're waiting for a reply, but we received a blocking synchronous
      // call.  We must process it or otherwise a deadlock might occur.
      sync_context()->DispatchMessages();
    } else if (result == WAIT_OBJECT_0 + 3) {
      // Run a nested messsage loop to pump all the thread's messages.  We
      // shutdown the nested loop when there are no more messages.
      pump_messages_events_.push(pump_messages_event);
      bool old_state = MessageLoop::current()->NestableTasksAllowed();
      MessageLoop::current()->SetNestableTasksAllowed(true);
      // Process a message, but come right back out of the MessageLoop (don't
      // loop, sleep, or wait for a kMsgQuit).
      MessageLoop::current()->RunAllPending();
      MessageLoop::current()->SetNestableTasksAllowed(old_state);
      pump_messages_events_.pop();
    } else {
      DCHECK(result == WAIT_OBJECT_0 + 4);
      // We were doing a WaitForMultipleObjects, but now the pump messages
      // event is set, so the next time we loop we'll use
      // MsgWaitForMultipleObjects instead.
    }

    // Continue looping until we get the reply to our synchronous message.
  } while (true);
}

bool SyncChannel::UnblockListener(Message* message) {
  return sync_context()->UnblockListener(message);
}

}  // namespace IPC
