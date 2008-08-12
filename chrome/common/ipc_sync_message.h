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

#ifndef CHROME_COMMON_IPC_SYNC_MESSAGE_H__
#define CHROME_COMMON_IPC_SYNC_MESSAGE_H__

#include <windows.h>
#include <string>
#include "base/basictypes.h"
#include "chrome/common/ipc_message.h"

namespace IPC {

class MessageReplyDeserializer;

class SyncMessage : public Message {
 public:
  SyncMessage(int32 routing_id, WORD type, PriorityValue priority,
              MessageReplyDeserializer* deserializer);

  // Call this to get a deserializer for the output parameters.
  // Note that this can only be called once, and the caller is responsible
  // for deleting the deserializer when they're done.
  MessageReplyDeserializer* GetReplyDeserializer();

  // If this message can cause the receiver to block while waiting for user
  // input (i.e. by calling MessageBox), then the caller needs to pump window
  // messages and dispatch asynchronous messages while waiting for the reply.
  // If this handle is passed in, then window messages will be pumped while
  // it's set.  The handle must be valid until after the Send call returns.
  void set_pump_messages_event(HANDLE event) {
    pump_messages_event_ = event;
    if (event) {
      header()->flags |= PUMPING_MSGS_BIT;
    } else {
      header()->flags &= ~PUMPING_MSGS_BIT;
    }
  }

  // Call this if you always want to pump messages.  You can call this method
  // or set_pump_messages_event but not both.
  void EnableMessagePumping();

  HANDLE pump_messages_event() const { return pump_messages_event_; }

  // Returns true if the message is a reply to the given request id.
  static bool IsMessageReplyTo(const Message& msg, int request_id);

  // Given a reply message, returns an iterator to the beginning of the data
  // (i.e. skips over the synchronous specific data).
  static void* GetDataIterator(const Message* msg);

  // Given a synchronous message (or its reply), returns its id.
  static int GetMessageId(const Message& msg);

  // Generates a reply message to the given message.
  static Message* GenerateReply(const Message* msg);

 private:
  struct SyncHeader {
    // unique ID (unique per sender)
    int message_id;
  };

  static bool ReadSyncHeader(const Message& msg, SyncHeader* header);
  static bool WriteSyncHeader(Message* msg, const SyncHeader& header);

  MessageReplyDeserializer* deserializer_;
  HANDLE pump_messages_event_;

  static uint32 next_id_;  // for generation of unique ids
};

// Used to deserialize parameters from a reply to a synchronous message
class MessageReplyDeserializer {
 public:
  bool SerializeOutputParameters(const Message& msg);
 private:
  // Derived classes need to implement this, using the given iterator (which
  // is skipped past the header for synchronous messages).
  virtual bool SerializeOutputParameters(const Message& msg, void* iter) = 0;
};

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_SYNC_MESSAGE_H__
