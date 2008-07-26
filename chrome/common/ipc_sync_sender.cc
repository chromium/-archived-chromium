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

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>

#include "chrome/common/ipc_sync_sender.h"

#include "chrome/common/ipc_sync_message.h"
#include "base/logging.h"

namespace IPC {

SyncSender::SyncSender(HANDLE shutdown_event) :
    shutdown_event_(shutdown_event),
    reply_deserialize_result_(false),
    reply_event_(CreateEvent(NULL, FALSE, FALSE, NULL)),
    blocking_event_(CreateEvent(NULL, FALSE, FALSE, NULL)) {
  DCHECK(shutdown_event != NULL);
}

SyncSender::~SyncSender() {
  CloseHandle(reply_event_);
  CloseHandle(blocking_event_);
  DCHECK(deserializers_.empty());
}

bool SyncSender::SendSync(IPC::Message* message) {
  bool message_is_sync = false;

  message_is_sync = message->is_sync();
  if (message_is_sync) {
    IPC::SyncMessage* sync_msg = static_cast<IPC::SyncMessage*>(message);
    PendingSyncMsg pending(IPC::SyncMessage::GetMessageId(*sync_msg),
                           sync_msg->GetReplyDeserializer());
    deserializers_.push(pending);
  }

  // Get the derived class to send the message.
  bool send_result = SendPrivate(message);

  if (!message_is_sync)
    return send_result;

  if (!send_result) {
    delete deserializers_.top().deserializer;
    deserializers_.pop();
    return false;
  }

  do {
    // wait for reply
    HANDLE objects[] = { reply_event_, blocking_event_, shutdown_event_ };
    DWORD result = WaitForMultipleObjects(3, objects, FALSE, INFINITE);
    if (result == WAIT_OBJECT_0 + 2) {
      // Process shut down before we can get a reply to a synchronous message.
      // Unblock the thread.
      return false;
    }

    if (result == WAIT_OBJECT_0 + 1) {
      // We're waiting for a reply, but the replier is making a synchronous
      // request that we must service or else we deadlock.  Or in case this
      // process supports processing of any synchronous messages while it's
      // blocked waiting for a reply (i.e. because it communicates with
      // multiple processes).
      while (true) {
        blocking_messages_lock_.Acquire();
        size_t size = blocking_messages_.size();
        Message* blocking_message;
        if (size) {
          blocking_message = blocking_messages_.front();
          blocking_messages_.pop();
        }

        blocking_messages_lock_.Release();

        if (!size)
          break;

        OnDispatchMessage(*blocking_message);
        delete blocking_message;
      }

      // Continue looping until we get the reply to our synchronous message.
    } else {
      // We got the reply to our synchronous message.
      return reply_deserialize_result_;
    }
  } while (true);
}

bool SyncSender::OnFilterMessage(const Message& msg) {
  if (deserializers_.empty())
    return false;

  if (IPC::SyncMessage::IsMessageReplyTo(msg, deserializers_.top().id)) {
    reply_deserialize_result_ =
        deserializers_.top().deserializer->SerializeOutputParameters(msg);
    delete deserializers_.top().deserializer;
    deserializers_.pop();
    SetEvent(reply_event_);
    return true;
  }

  if (msg.is_sync()) {
    // When we're blocked waiting for a reply we have to respond to other
    // synchronous messages as they might be blocking our reply.  We also don't
    // want to block other processes because one is blocked.

    // Create a copy of this message, as it can be deleted from under us
    // if there are more than two synchronous messages in parallel.
    // (i.e. A->B, B->A, A->B all synchronous)
    blocking_messages_lock_.Acquire();
    blocking_messages_.push(new Message(msg));
    blocking_messages_lock_.Release();
    SetEvent(blocking_event_);
    return true;
  }

  return false;
}

}  // namespace IPC
