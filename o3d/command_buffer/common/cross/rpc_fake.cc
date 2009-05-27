/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <windows.h>
#include "command_buffer/common/cross/rpc_fake.h"

namespace o3d {
namespace command_buffer {

// Create the queue, initializing the synchronization structures: a mutex for
// the queue itself, and an event to signal the consumers.
RPCQueue::RPCQueue() {
  event_ = ::CreateEvent(NULL, FALSE, FALSE, NULL);
  ::InitializeCriticalSection(&mutex_);
}

RPCQueue::~RPCQueue() {
  ::DeleteCriticalSection(&mutex_);
  ::CloseHandle(event_);
}

// Adds a message into the queue. Signal waiting threads that a new message is
// available.
void RPCQueue::AddMessage(const RPCMessage &call) {
  ::EnterCriticalSection(&mutex_);
  queue_.push(call);
  ::SetEvent(event_);
  ::LeaveCriticalSection(&mutex_);
}

// Checks whether the queue is empty.
bool RPCQueue::IsEmpty() {
  ::EnterCriticalSection(&mutex_);
  bool result = queue_.empty();
  ::LeaveCriticalSection(&mutex_);
  return result;
}

// Gets a message, waiting for one if the queue is empty.
void RPCQueue::GetMessage(RPCMessage *message) {
  ::EnterCriticalSection(&mutex_);
  while (queue_.empty()) {
    ::LeaveCriticalSection(&mutex_);
    ::WaitForSingleObject(event_, INFINITE);
    ::EnterCriticalSection(&mutex_);
  }
  *message = queue_.front();
  queue_.pop();
  ::LeaveCriticalSection(&mutex_);
}

// Tries to gets a message, returning immediately if the queue is empty.
bool RPCQueue::TryGetMessage(RPCMessage *message) {
  ::EnterCriticalSection(&mutex_);
  bool result = !queue_.empty();
  if (result) {
    *message = queue_.front();
    queue_.pop();
  }
  ::LeaveCriticalSection(&mutex_);
  return result;
}

// Creates the RPC server. The RPC server uses 2 RPC Queues, one for "incoming"
// calls (in_queue_), and one for "outgoing" return values (out_queue_).
RPCServer::RPCServer(RPCImplInterface * impl) {
  sender_.reset(new Sender(&in_queue_, &out_queue_));
  processor_.reset(new Processor(&in_queue_, &out_queue_, impl));
}

RPCServer::~RPCServer() {}

// Allocates the data for a message, if needed. Initializes the RPCMessage
// structure.
void RPCServer::AllocMessage(int message_id,
                             const void *data,
                             size_t size,
                             RPCHandle const *handles,
                             size_t handle_count,
                             RPCMessage *message) {
  message->message_id = message_id;
  message->size = size;
  message->handle_count = handle_count;
  if (data) {
    message->data = malloc(size);
    memcpy(message->data, data, size);
  } else {
    DCHECK(size == 0);
    message->data = NULL;
  }
  if (handles) {
    DCHECK(handle_count > 0);
    message->handles = new RPCHandle[handle_count];
    for (unsigned int i = 0; i < handle_count; ++i)
      message->handles[i] = handles[i];
  } else {
    DCHECK(handle_count == 0);
    message->handles = NULL;
  }
}

// Destroys the message data if needed.
void RPCServer::DestroyMessage(RPCMessage *message) {
  if (message->data) free(message->data);
  if (message->handles) delete [] message->handles;
}

// Processes one message, getting one from the incoming queue (blocking),
// dispatching it to the implementation (if not the "poisoned" message), and
// adding the return value to the outgoing queue.
bool RPCServer::Processor::ProcessMessage() {
  RPCMessage input;
  in_queue_->GetMessage(&input);
  RPCImplInterface::ReturnValue result = 0;
  int continue_processing = true;
  if (input.message_id == POISONED_MESSAGE_ID) {
    continue_processing = false;
  } else {
    result = impl_->DoCall(input.message_id, input.data, input.size,
                           input.handles, input.handle_count);
  }
  DestroyMessage(&input);

  RPCMessage output;
  AllocMessage(RESPONSE_ID, &result, sizeof(result), NULL, 0, &output);
  out_queue_->AddMessage(output);
  return continue_processing;
}

// Checks if the incoming queue is empty.
bool RPCServer::Processor::HasMessage() {
  return !in_queue_->IsEmpty();
}

// Processes all messages until the server is killed.
void RPCServer::MessageLoop() {
  do {} while (processor_->ProcessMessage());
}

// Sends a "poisoned" call to the server thread, making it exit the processing
// loop.
void RPCServer::KillServer() {
  sender_->SendCall(POISONED_MESSAGE_ID, NULL, 0, NULL, 0);
}

// Sends a call to the server thread. This puts a message into the "incoming"
// queue, and waits for the return message on the "outgoing" queue.
RPCImplInterface::ReturnValue RPCServer::Sender::SendCall(
    int message_id,
    const void * data,
    size_t size,
    RPCHandle const *handles,
    size_t handle_count) {
  RPCMessage input;
  AllocMessage(message_id, data, size, handles, handle_count, &input);
  in_queue_->AddMessage(input);

  RPCMessage output;
  out_queue_->GetMessage(&output);
  DCHECK(output.message_id == RESPONSE_ID);
  DCHECK(output.size == sizeof(RPCImplInterface::ReturnValue));
  RPCImplInterface::ReturnValue result =
      *(reinterpret_cast<RPCImplInterface::ReturnValue *>(output.data));
  DestroyMessage(&output);
  return result;
}

class RPCSendProxy : public RPCSendInterface {
 public:
  explicit RPCSendProxy(RPCSendInterface *interface) : interface_(interface) {}
  virtual ~RPCSendProxy() {}
  virtual RPCImplInterface::ReturnValue SendCall(int message_id,
                                                 const void * data,
                                                 size_t size,
                                                 RPCHandle const *handles,
                                                 size_t handle_count) {
    return interface_->SendCall(message_id, data, size, handles, handle_count);
  }
 private:
  RPCSendInterface *interface_;
};

// Create a proxy so that it can be managed as a separate object, to have the
// same semantics as the IMC implementation.
RPCSendInterface *MakeSendInterface(RPCSocketHandle handle) {
  return new RPCSendProxy(handle->GetSendInterface());
}

void *MapShm(RPCShmHandle handle, size_t size) {
  return (size <= handle->size) ? return handle->address : NULL;
}

void UnmapShm(void * address, size_t size) {
}

}  // namespace command_buffer
}  // namespace o3d
