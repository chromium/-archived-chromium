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

#ifndef CHROME_COMMON_IPC_SYNC_SENDER_H__
#define CHROME_COMMON_IPC_SYNC_SENDER_H__

#include <windows.h>
#include <string>
#include <stack>
#include <queue>
#include "base/basictypes.h"
#include "chrome/common/ipc_message.h"
#include "chrome/common/lock.h"

namespace IPC {

// Classes which send synchronous messages need to derive from this.
// This class is called on two threads.  The first is the main thread that does
// the message processing (and which may be blocked waiting for a reply to a
// synchronous message).  The second is the IPC thread that does the filtering
// of messages before passing it to the (maybe) blocked main thread.
//
// To use this class:
//  1) Your Send() must pass all messages to SendSync()
//  2) You must implement SendPrivate(), which SendSync() will call internally
//  3) You must be able to filter incoming messages on the IPC thread, and
//     pass them on to OnFilterMessage().
//  4) You must implement OnDispatchMessage, which is what dispatches
//     messages on the main thread.

class SyncSender {
 public:
  // shutdown_event is an event that can be waited on so that we don't block if
  //   if the process is shutting down.  If it's NULL, then it's not used.
  SyncSender(HANDLE shutdown_event);
  ~SyncSender();

  // These are called on the main thread.

  // The derived class's Send should just passthrough to SendSync.
  bool SendSync(IPC::Message* message);

  // SendSync will call your implementation's SendPrivate when it comes
  // time to send the message on the channel.
  virtual bool SendPrivate(IPC::Message* message) = 0;

  // If a message needs to be dispatched immediately because it's blocking our
  // reply, this function will be called.
  virtual void OnDispatchMessage(const Message& message) = 0;

  // This is called on the IPC thread.  Returns true if the message has been
  // consumed (i.e. don't do any more processing).
  bool OnFilterMessage(const Message& message);

 private:
  // When sending a synchronous message, this structure contains an object that
  // knows how to deserialize the response.
  struct PendingSyncMsg {
    PendingSyncMsg(int id, IPC::MessageReplyDeserializer* d) :
        id(id), deserializer(d) { }
    int id;
    IPC::MessageReplyDeserializer* deserializer;
  };

  // Set when we got a reply for a synchronous message that we sent.
  HANDLE reply_event_;

  // Set when we got a synchronous message that we must respond to as the
  // sender needs its reply before it can reply to our original synchronous
  // message.
  HANDLE blocking_event_;

  // Copy of shutdown event that we get in constructor.
  HANDLE shutdown_event_;

  typedef std::stack<PendingSyncMsg> PendingSyncMessageQueue;
  PendingSyncMessageQueue deserializers_;
  bool reply_deserialize_result_;

  // If we're waiting on a reply and the caller sends a synchronous message
  // that's blocking the reply, this variable is used to pass the intermediate
  // "blocking" message between our two threads.  We can store multiple
  // messages as a process will want to respond to any synchronous message
  // while they're blocked (i.e. because they talk to multiple processes).
  typedef std::queue<Message*> BlockingMessageQueue;
  BlockingMessageQueue blocking_messages_;

  // Above data structure is used on multiple threads, so it need
  // synchronization.
  Lock blocking_messages_lock_;

  DISALLOW_EVIL_CONSTRUCTORS(SyncSender);
};

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_SYNC_SENDER_H__
