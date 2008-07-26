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
#include <stack>

#include "chrome/common/ipc_sync_message.h"
#include "base/logging.h"

namespace IPC {

uint32 SyncMessage::next_id_ = 0;
#define kSyncMessageHeaderSize 4


SyncMessage::SyncMessage(
    int32 routing_id,
    WORD type,
    PriorityValue priority,
    MessageReplyDeserializer* deserializer)
    : Message(routing_id, type, priority),
      deserializer_(deserializer),
      pump_messages_event_(NULL) {
  set_sync();
  set_unblock(true);

  // Add synchronous message data before the message payload.
  SyncHeader header;
  header.message_id = ++next_id_;
  WriteSyncHeader(this, header);
}

MessageReplyDeserializer* SyncMessage::GetReplyDeserializer() {
  MessageReplyDeserializer* rv = deserializer_;
  DCHECK(rv);
  deserializer_ = NULL;
  return rv;
}

bool SyncMessage::IsMessageReplyTo(const Message& msg, int request_id) {
  if (!msg.is_reply())
    return false;

  return GetMessageId(msg) == request_id;
}

void* SyncMessage::GetDataIterator(const Message* msg) {
  void* iter = const_cast<char*>(msg->payload());
  UpdateIter(&iter, kSyncMessageHeaderSize);
  return iter;
}

int SyncMessage::GetMessageId(const Message& msg) {
  if (!msg.is_sync() && !msg.is_reply())
    return 0;

  SyncHeader header;
  if (!ReadSyncHeader(msg, &header))
    return 0;

  return header.message_id;
}

Message* SyncMessage::GenerateReply(const Message* msg) {
  DCHECK(msg->is_sync());

  Message* reply = new Message(msg->routing_id(), IPC_REPLY_ID, msg->priority());
  reply->set_reply();

  SyncHeader header;

  // use the same message id, but this time reply bit is set
  header.message_id = GetMessageId(*msg);
  WriteSyncHeader(reply, header);

  return reply;
}

bool SyncMessage::ReadSyncHeader(const Message& msg, SyncHeader* header) {
  DCHECK(msg.is_sync() || msg.is_reply());

  void* iter = NULL;
  bool result = msg.ReadInt(&iter, &header->message_id);
  if (!result) {
    NOTREACHED();
    return false;
  }

  return true;
}

bool SyncMessage::WriteSyncHeader(Message* msg, const SyncHeader& header) {
  DCHECK(msg->is_sync() || msg->is_reply());
  DCHECK(msg->payload_size() == 0);

  void* iter = NULL;
  bool result = msg->WriteInt(header.message_id);
  if (!result) {
    NOTREACHED();
    return false;
  }

  // Note: if you add anything here, you need to update kSyncMessageHeaderSize.
  DCHECK(kSyncMessageHeaderSize == msg->payload_size());

  return true;
}


bool MessageReplyDeserializer::SerializeOutputParameters(const Message& msg) {
  return SerializeOutputParameters(msg, SyncMessage::GetDataIterator(&msg));
}

}  // namespace IPC
