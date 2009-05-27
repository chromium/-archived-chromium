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


// This file defines classes faking the NativeClient RPC mechanism. This is
// intended as a temporary replacement of NativeClient until it is ready. It
// assumes the various clients and services run in separate threads of the same
// process.

#ifndef O3D_COMMAND_BUFFER_COMMON_CROSS_RPC_FAKE_H_
#define O3D_COMMAND_BUFFER_COMMON_CROSS_RPC_FAKE_H_

#include <windows.h>
#include <queue>
#include "core/cross/types.h"
#include "command_buffer/common/cross/rpc.h"

namespace o3d {
namespace command_buffer {

typedef CRITICAL_SECTION Mutex;
typedef HANDLE Event;

// Struct describing a RPC message, as a message ID and a data payload.
// It "owns" the data pointer.
struct RPCMessage {
  int message_id;
  size_t size;
  void *data;
  RPCHandle *handles;
  size_t handle_count;
};

// Implementation of a (thread-safe) RPC message queue (FIFO). It allows simply
// to enqueue and dequeue messages, in a blocking or non-blocking fashion.
class RPCQueue {
 public:
  RPCQueue();
  ~RPCQueue();

  // Adds a message into the back of the queue.
  // Parameters:
  //    message: the message to enqueue. Ownership of the message data is taken
  //             by the queue.
  void AddMessage(const RPCMessage &message);

  // Tests whether or not the queue is empty.
  // Returns:
  //    true if the queue is empty.
  bool IsEmpty();

  // Gets a message from the front of the queue. This call blocks until a
  // message is available in the queue.
  // Parameters:
  //    message: a pointer to a RPCMessage structure receiving the message.
  //             That structure takes ownership of the message data.
  void GetMessage(RPCMessage *message);

  // Try to get a message from the front of the queue, if any. This call will
  // not block if the queue is empty.
  // Parameters:
  //    message: a pointer to a RPCMessage structure receiving the message, if
  //             any. That structure takes ownership of the message data. If no
  //             message is available, that structure is unchanged.
  // Returns:
  //    true if a message was available.
  bool TryGetMessage(RPCMessage *message);

 private:
  std::queue<RPCMessage> queue_;
  Mutex mutex_;
  Event event_;

  DISALLOW_COPY_AND_ASSIGN(RPCQueue);
};

// Implements a fake RPC server interface. This class is intended to be used
// across different threads (it is thread safe):
// - one server thread, that processes messages (using MessageLoop() or
// ProcessMessage()), to execute the RPC calls.
// - one or several client threads, that can send RPC calls to it.
//
// One of the client threads can "kill" the server so that it exits its
// processing loop.
class RPCServer {
 public:
  explicit RPCServer(RPCImplInterface *impl);
  ~RPCServer();

  // Server thread functions

  // Processes all messages, until the server is killed.
  void MessageLoop();
  RPCProcessInterface *GetProcessInterface() { return processor_.get(); }

  // client thread functions
  RPCSendInterface *GetSendInterface() { return sender_.get(); }

  // Kills the server thread, making it exit its processing loop. This call
  // will block until the server has finished processing all the previous
  // messages.
  void KillServer();

 private:
  static void AllocMessage(int message_id,
                           const void * data,
                           size_t size,
                           RPCHandle const *handles,
                           size_t handle_count,
                           RPCMessage *message);
  static void DestroyMessage(RPCMessage *message);

  class Sender : public RPCSendInterface {
   public:
    Sender(RPCQueue *in_queue, RPCQueue *out_queue)
        : in_queue_(in_queue),
          out_queue_(out_queue) {}
    // Sends a call to the server thread. This call will be dispatched by the
    // server thread to the RPC implementation. This call will block until the
    // call is processed and the return value is sent back.
    // Parameters:
    //    message_id: the RPC message ID.
    //    data: the RPC message payload.
    //    size: the size of the data.
    //    handles: an array of RPC handles to transmit
    //    handle_count: the number of RPC handles in the array
    // Returns:
    //    the return value of the RPC call.
    virtual RPCImplInterface::ReturnValue SendCall(int message_id,
                                                   const void * data,
                                                   size_t size,
                                                   RPCHandle const *handles,
                                                   size_t handle_count);
   private:
    RPCQueue *in_queue_;
    RPCQueue *out_queue_;
    DISALLOW_COPY_AND_ASSIGN(Sender);
  };

  class Processor : public RPCProcessInterface {
   public:
    Processor(RPCQueue *in_queue, RPCQueue *out_queue, RPCImplInterface *impl)
        : in_queue_(in_queue),
          out_queue_(out_queue),
          impl_(impl) {}
    virtual ~Processor() {}
    // Processes one message, blocking if necessary until a message is available
    // or the server is killed. This will dispatch the received message to the
    // RPC implementation, and send back a response message with the return
    // value to the client.
    // Returns:
    //    false if the server was killed.
    virtual bool ProcessMessage();

    // Checks whether a message is available for processing - allowing to test
    // if ProcessMessage will block.
    // Returns:
    //    true if a message is available.
    virtual bool HasMessage();
   private:
    RPCQueue *in_queue_;
    RPCQueue *out_queue_;
    RPCImplInterface *impl_;
    DISALLOW_COPY_AND_ASSIGN(Processor);
  };

  RPCQueue in_queue_;
  RPCQueue out_queue_;
  scoped_ptr<Sender> sender_;
  scoped_ptr<Processor> processor_;
  DISALLOW_COPY_AND_ASSIGN(RPCServer);
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_COMMON_CROSS_RPC_FAKE_H_
