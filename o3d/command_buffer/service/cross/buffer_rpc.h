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


// This file defines the RPC glue for the Command Buffer Synchronous API,
// service side: an implementation of a RPC service, forwarding calls to
// a Command Buffer API implemention (RPC -> API).

#ifndef O3D_COMMAND_BUFFER_SERVICE_CROSS_BUFFER_RPC_H__
#define O3D_COMMAND_BUFFER_SERVICE_CROSS_BUFFER_RPC_H__

#include "command_buffer/common/cross/buffer_sync_api.h"
#include "command_buffer/common/cross/rpc.h"

namespace o3d {
namespace command_buffer {

// RPC service implementation, implementing the Command Buffer Synchronous API
// RPC glue. This class is temporary, and will be replaced when the
// NativeClient RPC mechanism will be available.
//
// The API exposed through the RPC mechanism maps 1-to-1 to BufferSyncInterface,
// trivially serializing arguments.
class BufferRPCImpl: public RPCImplInterface {
 public:
  explicit BufferRPCImpl(BufferSyncInterface *handler) : handler_(handler) {}
  virtual ~BufferRPCImpl() {}

  enum MessageId {
    INIT_CONNECTION = RESPONSE_ID + 1,
    CLOSE_CONNECTION,
    REGISTER_SHARED_MEMORY,
    UNREGISTER_SHARED_MEMORY,
    SET_COMMAND_BUFFER,
    PUT,
    GET,
    GET_TOKEN,
    WAIT_GET_CHANGES,
    SIGNAL_GET_CHANGES,
    GET_STATUS,
    GET_PARSE_ERROR,
  };

  // Structure describing the arguments for the SET_COMMAND_BUFFER RPC.
  struct SetCommandBufferStruct {
    unsigned int shm_id;
    ptrdiff_t offset;
    size_t size;
    CommandBufferOffset start_get;
  };

  // Structure describing the arguments for the SIGNAL_GET_CHANGES RPC.
  struct SignalGetChangesStruct {
    CommandBufferOffset current_value;
    int rpc_message_id;
  };

  // Implements the DoCall interface, interpreting the message with the
  // parameters, and passing the calls with arguments to the handler.
  virtual ReturnValue DoCall(int message_id,
                             const void * data,
                             size_t size,
                             RPCHandle const *handles,
                             size_t handle_count);

 private:
  BufferSyncInterface *handler_;
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SERVICE_CROSS_BUFFER_RPC_H__
