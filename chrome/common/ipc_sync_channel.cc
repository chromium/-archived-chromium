// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "chrome/common/ipc_sync_channel.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/thread_local.h"
#include "chrome/common/child_process.h"
#include "chrome/common/ipc_logging.h"
#include "chrome/common/ipc_sync_message.h"

using base::TimeDelta;
using base::TimeTicks;

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

class SyncChannel::ReceivedSyncMsgQueue;

class SyncChannel::ReceivedSyncMsgQueue :
    public base::RefCountedThreadSafe<ReceivedSyncMsgQueue> {
 public:
  // Returns the ReceivedSyncMsgQueue instance for this thread, creating one
  // if necessary.  Call RemoveListener on the same thread when done.
  static ReceivedSyncMsgQueue* AddListener() {
    // We want one ReceivedSyncMsgQueue per listener thread (i.e. since multiple
    // SyncChannel objects can block the same thread).
    ReceivedSyncMsgQueue* rv = lazy_tls_ptr_.Pointer()->Get();
    if (!rv) {
      rv = new ReceivedSyncMsgQueue();
      ReceivedSyncMsgQueue::lazy_tls_ptr_.Pointer()->Set(rv);
    }
    rv->listener_count_++;
    return rv;
  }

  ~ReceivedSyncMsgQueue() {
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

    SetEvent(dispatch_event_);
    if (!was_task_pending) {
      listener_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
          this, &ReceivedSyncMsgQueue::DispatchMessagesTask));
    }
  }

  void QueueReply(const Message &msg, SyncChannel::SyncContext* context) {
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
      Logging* logger = Logging::current();
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

    if (--listener_count_ == 0) {
      DCHECK(lazy_tls_ptr_.Pointer()->Get());
      lazy_tls_ptr_.Pointer()->Set(NULL);
    }
  }

  HANDLE dispatch_event() { return dispatch_event_; }
  MessageLoop* listener_message_loop() { return listener_message_loop_; }

  // Holds a pointer to the per-thread ReceivedSyncMsgQueue object.
  static base::LazyInstance<base::ThreadLocalPointer<ReceivedSyncMsgQueue> >
      lazy_tls_ptr_;

  // Called on the ipc thread to check if we can unblock any current Send()
  // calls based on a queued reply.
  void DispatchReplies() {
    for (size_t i = 0; i < received_replies_.size(); ++i) {
      Message* message = received_replies_[i].message;
      if (received_replies_[i].context->TryToUnblockListener(message)) {
        delete message;
        received_replies_.erase(received_replies_.begin() + i);
        return;
      }
    }
  }

 private:
  // See the comment in SyncChannel::SyncChannel for why this event is created
  // as manual reset.
  ReceivedSyncMsgQueue() :
      dispatch_event_(CreateEvent(NULL, TRUE, FALSE, NULL)),
      task_pending_(false),
      listener_message_loop_(MessageLoop::current()),
      listener_count_(0) {
  }

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

  // Holds information about a queued reply message.
  struct Reply {
    Reply(Message* m, SyncChannel::SyncContext* c)
        : message(m),
          context(c) { }

    Message* message;
    scoped_refptr<SyncChannel::SyncContext> context;
  };

  std::vector<Reply> received_replies_;

  // Set when we got a synchronous message that we must respond to as the
  // sender needs its reply before it can reply to our original synchronous
  // message.
  ScopedHandle dispatch_event_;
  MessageLoop* listener_message_loop_;
  Lock message_lock_;
  bool task_pending_;
  int listener_count_;
};

base::LazyInstance<base::ThreadLocalPointer<SyncChannel::ReceivedSyncMsgQueue> >
    SyncChannel::ReceivedSyncMsgQueue::lazy_tls_ptr_(base::LINKER_INITIALIZED);

SyncChannel::SyncContext::SyncContext(
    Channel::Listener* listener,
    MessageFilter* filter,
    MessageLoop* ipc_thread,
    HANDLE shutdown_event)
    : ChannelProxy::Context(listener, filter, ipc_thread),
      shutdown_event_(shutdown_event),
      received_sync_msgs_(ReceivedSyncMsgQueue::AddListener()){
}

SyncChannel::SyncContext::~SyncContext() {
  while (!deserializers_.empty())
    Pop();
}

// Adds information about an outgoing sync message to the context so that
// we know how to deserialize the reply.  Returns a handle that's set when
// the reply has arrived.
void SyncChannel::SyncContext::Push(SyncMessage* sync_msg) {
  // The event is created as manual reset because in between SetEvent and
  // OnObjectSignalled, another Send can happen which would stop the watcher
  // from being called.  The event would get watched later, when the nested
  // Send completes, so the event will need to remain set.
  PendingSyncMsg pending(SyncMessage::GetMessageId(*sync_msg),
                         sync_msg->GetReplyDeserializer(),
                         CreateEvent(NULL, TRUE, FALSE, NULL));
  AutoLock auto_lock(deserializers_lock_);
  deserializers_.push_back(pending);
}

bool SyncChannel::SyncContext::Pop() {
  bool result;
  {
    AutoLock auto_lock(deserializers_lock_);
    PendingSyncMsg msg = deserializers_.back();
    delete msg.deserializer;
    CloseHandle(msg.done_event);
    deserializers_.pop_back();
    result = msg.send_result;
  }

  // We got a reply to a synchronous Send() call that's blocking the listener
  // thread.  However, further down the call stack there could be another
  // blocking Send() call, whose reply we received after we made this last
  // Send() call.  So check if we have any queued replies available that
  // can now unblock the listener thread.
  ipc_message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      received_sync_msgs_.get(), &ReceivedSyncMsgQueue::DispatchReplies));

  return result;
}

HANDLE SyncChannel::SyncContext::GetSendDoneEvent() {
  AutoLock auto_lock(deserializers_lock_);
  return deserializers_.back().done_event;
}

HANDLE SyncChannel::SyncContext::GetDispatchEvent() {
  return received_sync_msgs_->dispatch_event();
}

void SyncChannel::SyncContext::DispatchMessages() {
  received_sync_msgs_->DispatchMessages();
}

bool SyncChannel::SyncContext::TryToUnblockListener(const Message* msg) {
  AutoLock auto_lock(deserializers_lock_);
  if (deserializers_.empty() ||
      !SyncMessage::IsMessageReplyTo(*msg, deserializers_.back().id)) {
    return false;
  }

  if (!msg->is_reply_error()) {
    deserializers_.back().send_result = deserializers_.back().deserializer->
        SerializeOutputParameters(*msg);
  }
  SetEvent(deserializers_.back().done_event);

  return true;
}

void SyncChannel::SyncContext::Clear() {
  CancelPendingSends();
  received_sync_msgs_->RemoveListener(listener());

  Context::Clear();
}

void SyncChannel::SyncContext::OnMessageReceived(const Message& msg) {
  // Give the filters a chance at processing this message.
  if (TryFilters(msg))
    return;

  if (TryToUnblockListener(&msg))
    return;

  if (msg.should_unblock()) {
    received_sync_msgs_->QueueMessage(msg, listener(), channel_id());
    return;
  }

  if (msg.is_reply()) {
    received_sync_msgs_->QueueReply(msg, this);
    return;
  }

  return Context::OnMessageReceivedNoFilter(msg);
}

void SyncChannel::SyncContext::OnChannelError() {
  CancelPendingSends();
  Context::OnChannelError();
}

void SyncChannel::SyncContext::OnChannelOpened() {
  shutdown_watcher_.StartWatching(shutdown_event_, this);
  Context::OnChannelOpened();
}

void SyncChannel::SyncContext::OnChannelClosed() {
  shutdown_watcher_.StopWatching();
  Context::OnChannelClosed();
}

void SyncChannel::SyncContext::OnSendTimeout(int message_id) {
  AutoLock auto_lock(deserializers_lock_);
  PendingSyncMessageQueue::iterator iter;
  for (iter = deserializers_.begin(); iter != deserializers_.end(); iter++) {
    if ((*iter).id == message_id) {
      SetEvent((*iter).done_event);
      break;
    }
  }
}

void SyncChannel::SyncContext::CancelPendingSends() {
  AutoLock auto_lock(deserializers_lock_);
  PendingSyncMessageQueue::iterator iter;
  for (iter = deserializers_.begin(); iter != deserializers_.end(); iter++)
    SetEvent((*iter).done_event);
}

void SyncChannel::SyncContext::OnObjectSignaled(HANDLE object) {
  DCHECK(object == shutdown_event_);
  // Process shut down before we can get a reply to a synchronous message.
  // Cancel pending Send calls, which will end up setting the send done event.
  CancelPendingSends();
}


SyncChannel::SyncChannel(
    const std::wstring& channel_id, Channel::Mode mode,
    Channel::Listener* listener, MessageFilter* filter,
    MessageLoop* ipc_message_loop, bool create_pipe_now, HANDLE shutdown_event)
    : ChannelProxy(
          channel_id, mode, ipc_message_loop,
          new SyncContext(listener, filter, ipc_message_loop, shutdown_event),
          create_pipe_now),
      sync_messages_with_no_timeout_allowed_(true) {
  // Ideally we only want to watch this object when running a nested message
  // loop.  However, we don't know when it exits if there's another nested
  // message loop running under it or not, so we wouldn't know whether to
  // stop or keep watching.  So we always watch it, and create the event as
  // manual reset since the object watcher might otherwise reset the event
  // when we're doing a WaitForMultipleObjects.
  dispatch_watcher_.StartWatching(sync_context()->GetDispatchEvent(), this);
}

SyncChannel::~SyncChannel() {
}

bool SyncChannel::Send(Message* message) {
  return SendWithTimeout(message, INFINITE);
}

bool SyncChannel::SendWithTimeout(Message* message, int timeout_ms) {
  if (!message->is_sync()) {
    ChannelProxy::Send(message);
    return true;
  }

  // *this* might get deleted in WaitForReply.
  scoped_refptr<SyncContext> context(sync_context());
  if (WaitForSingleObject(context->shutdown_event(), 0) == WAIT_OBJECT_0) {
    delete message;
    return false;
  }

  DCHECK(sync_messages_with_no_timeout_allowed_ || timeout_ms != INFINITE);
  SyncMessage* sync_msg = static_cast<SyncMessage*>(message);
  context->Push(sync_msg);
  int message_id = SyncMessage::GetMessageId(*sync_msg);
  HANDLE pump_messages_event = sync_msg->pump_messages_event();

  ChannelProxy::Send(message);

  if (timeout_ms != INFINITE) {
    // We use the sync message id so that when a message times out, we don't
    // confuse it with another send that is either above/below this Send in
    // the call stack.
    context->ipc_message_loop()->PostDelayedTask(FROM_HERE, 
        NewRunnableMethod(context.get(),
            &SyncContext::OnSendTimeout, message_id), timeout_ms);
  }

  // Wait for reply, or for any other incoming synchronous messages.
  WaitForReply(pump_messages_event);

  return context->Pop();
}

void SyncChannel::WaitForReply(HANDLE pump_messages_event) {
  while (true) {
    HANDLE objects[] = { sync_context()->GetDispatchEvent(),
                         sync_context()->GetSendDoneEvent(),
                         pump_messages_event };
    uint32 count = pump_messages_event ? 3: 2;
    DWORD result = WaitForMultipleObjects(count, objects, FALSE, INFINITE);
    if (result == WAIT_OBJECT_0) {
      // We're waiting for a reply, but we received a blocking synchronous
      // call.  We must process it or otherwise a deadlock might occur.
      ResetEvent(sync_context()->GetDispatchEvent());
      sync_context()->DispatchMessages();
      continue;
    }

    if (result == WAIT_OBJECT_0 + 2)
      WaitForReplyWithNestedMessageLoop();  // Start a nested message loop.

    break;
  }
}

void SyncChannel::WaitForReplyWithNestedMessageLoop() {
  HANDLE old_done_event = send_done_watcher_.GetWatchedObject();
  send_done_watcher_.StopWatching();
  send_done_watcher_.StartWatching(sync_context()->GetSendDoneEvent(), this);
  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  MessageLoop::current()->Run();
  MessageLoop::current()->SetNestableTasksAllowed(old_state);
  if (old_done_event)
    send_done_watcher_.StartWatching(old_done_event, this);
}

void SyncChannel::OnObjectSignaled(HANDLE object) {
  HANDLE dispatch_event = sync_context()->GetDispatchEvent();
  if (object == dispatch_event) {
    // The call to DispatchMessages might delete this object, so reregister
    // the object watcher first.
    ResetEvent(dispatch_event);
    dispatch_watcher_.StartWatching(dispatch_event, this);
    sync_context()->DispatchMessages();
  } else {
    // We got the reply, timed out or the process shutdown.
    DCHECK(object == sync_context()->GetSendDoneEvent());
    MessageLoop::current()->Quit();
  }
}

}  // namespace IPC
