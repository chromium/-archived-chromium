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


// This file has the implementation of the Command Buffer Synchronous API RPC
// glue.

#include "command_buffer/common/cross/logging.h"
#include "command_buffer/service/cross/buffer_rpc.h"

namespace o3d {
namespace command_buffer {

// Implements the dispatch function, deciding on which function to call based
// on the message ID, taking the arguments trivially serialized in the data
// payload.
BufferRPCImpl::ReturnValue BufferRPCImpl::DoCall(int message_id,
                                                 const void *data,
                                                 size_t size,
                                                 RPCHandle const *handles,
                                                 size_t handle_count) {
  switch (message_id) {
    case INIT_CONNECTION: {
      DCHECK_EQ(0, size);
      DCHECK_EQ(0, handle_count);
      handler_->InitConnection();
      return 0;
    }
    case CLOSE_CONNECTION:
      DCHECK_EQ(0, size);
      DCHECK_EQ(0, handle_count);
      handler_->CloseConnection();
      return 0;
    case REGISTER_SHARED_MEMORY: {
      DCHECK_EQ(sizeof(size_t), size);  // NOLINT
      DCHECK_EQ(1, handle_count);
      RPCShmHandle buffer = static_cast<RPCShmHandle>(handles[0]);
      return handler_->RegisterSharedMemory(buffer,
                                            *static_cast<const size_t *>(data));
    }
    case UNREGISTER_SHARED_MEMORY: {
      DCHECK_EQ(sizeof(unsigned int), size); // NOLINT
      DCHECK_EQ(0, handle_count);
      unsigned int shm_id = *(static_cast<const unsigned int *>(data));
      handler_->UnregisterSharedMemory(shm_id);
      return 0;
    }
    case SET_COMMAND_BUFFER: {
      DCHECK_EQ(sizeof(SetCommandBufferStruct), size);
      DCHECK_EQ(0, handle_count);
      const SetCommandBufferStruct *params =
          static_cast<const SetCommandBufferStruct *>(data);
      handler_->SetCommandBuffer(params->shm_id,
                                 params->offset,
                                 params->size,
                                 params->start_get);
      return 0;
    }
    case PUT: {
      DCHECK_EQ(sizeof(CommandBufferOffset), size);
      DCHECK_EQ(0, handle_count);
      CommandBufferOffset offset =
          *(static_cast<const CommandBufferOffset *>(data));
      handler_->Put(offset);
      return 0;
    }
    case GET:
      DCHECK_EQ(0, size);
      DCHECK_EQ(0, handle_count);
      return handler_->Get();
    case GET_TOKEN:
      DCHECK_EQ(0, size);
      DCHECK_EQ(0, handle_count);
      return handler_->GetToken();
    case WAIT_GET_CHANGES: {
      DCHECK_EQ(sizeof(CommandBufferOffset), size);
      DCHECK_EQ(0, handle_count);
      CommandBufferOffset current_value =
          *(static_cast<const CommandBufferOffset *>(data));
      return handler_->WaitGetChanges(current_value);
    }
    case SIGNAL_GET_CHANGES: {
      DCHECK_EQ(sizeof(SignalGetChangesStruct), size);
      DCHECK_EQ(0, handle_count);
      const SignalGetChangesStruct *params =
          static_cast<const SignalGetChangesStruct *>(data);
      handler_->SignalGetChanges(params->current_value,
                                 params->rpc_message_id);
      return 0;
    }
    case GET_STATUS: {
      DCHECK_EQ(0, size);
      DCHECK_EQ(0, handle_count);
      return handler_->GetStatus();
    }
    case GET_PARSE_ERROR: {
      DCHECK_EQ(0, size);
      DCHECK_EQ(0, handle_count);
      return handler_->GetParseError();
    }
    default:
      LOG(FATAL) << "unsupported RPC";
      return 0;
  }
}

}  // namespace command_buffer
}  // namespace o3d
