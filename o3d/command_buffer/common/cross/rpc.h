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


#ifndef O3D_COMMAND_BUFFER_COMMON_CROSS_RPC_H_
#define O3D_COMMAND_BUFFER_COMMON_CROSS_RPC_H_

#include "base/basictypes.h"

#include "third_party/native_client/googleclient/native_client/src/shared/imc/nacl_htp.h"

namespace o3d {
namespace command_buffer {

// Pre-defined message IDs.
enum {
  POISONED_MESSAGE_ID,  // sent to kill the RPC server.
  RESPONSE_ID,          // sent as a response to a RPC call.
};

typedef nacl::HtpHandle RPCHandle;
typedef RPCHandle RPCShmHandle;
typedef RPCHandle RPCSocketHandle;
static const RPCHandle kRPCInvalidHandle = nacl::kInvalidHtpHandle;

// Interface for a RPC implementation. This class defines the calls a RPC
// service needs to implement.
class RPCImplInterface {
 public:
  // The type of return values for RPC calls. The RPC model is an arbitrary
  // set of parameters, but a single return value.
  typedef unsigned int ReturnValue;

  RPCImplInterface() {}
  virtual ~RPCImplInterface() {}

  // De-multiplexes a RPC call. This function takes the message ID and the data
  // to deduce a proper function to call, with its arguments, returning a
  // single value. Most protocols will select the function from the message ID,
  // and take the arguments from the data.
  // Parameters:
  //    message_id: the RPC message ID.
  //    data: the RPC message payload.
  //    size: the size of the data.
  // Returns:
  //    a single return value from the function called.
  virtual ReturnValue DoCall(int message_id,
                             const void * data,
                             size_t size,
                             RPCHandle const *handles,
                             size_t handle_count) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(RPCImplInterface);
};

class RPCSendInterface {
 public:
  virtual ~RPCSendInterface() {}
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
                                                 size_t handle_count) = 0;
};

class RPCProcessInterface {
 public:
  virtual ~RPCProcessInterface() {}
  // Processes one message, blocking if necessary until a message is available
  // or the server is killed. This will dispatch the received message to the
  // RPC implementation, and send back a response message with the return
  // value to the client.
  // Returns:
  //    false if the server was killed.
  virtual bool ProcessMessage() = 0;

  // Checks whether a message is available for processing - allowing to test if
  // ProcessMessage will block.
  // Returns:
  //    true if a message is available.
  virtual bool HasMessage() = 0;
};

// Creates a RPCSendInterface from a RPCSocketHandle.
RPCSendInterface *MakeSendInterface(RPCSocketHandle handle);

// Creates a shared memory buffer.
// Parameters:
//   size: the size of the buffer.
// Returns:
//   the handle to the shared memory buffer.
RPCShmHandle CreateShm(size_t size);

// Destroys a shared memory buffer.
// Parameters:
//   handle: the handle to the shared memory buffer.
void DestroyShm(RPCShmHandle handle);

// Maps a shared memory buffer into the address space.
// Parameters:
//   handle: the handle to the shared memory buffer.
//   size: the size of the region to map in the memory buffer. May be smaller
//         than the shared memory region, but the underlying implementation
//         will round it up to the page size or the whole shared memory.
// Returns:
//   the address of the mapped region, or NULL if failure.
void *MapShm(RPCShmHandle handle, size_t size);

// Unmaps a previously mapped memory buffer.
// Parameters:
//   address: the address of the mapped region.
//   size: the size of the region to un-map. It can be a subset of the
//         previously mapped region, but the underlying implementation will
//         round it up to the page size.
void UnmapShm(void *address, size_t size);

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_COMMON_CROSS_RPC_H_
