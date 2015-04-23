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
// glue, client-side (proxy).

#include "command_buffer/client/cross/buffer_sync_proxy.h"
#include "command_buffer/service/cross/buffer_rpc.h"

namespace o3d {
namespace command_buffer {

// Implements the proxy InitConnection, forwarding the call with its argument to
// the RPC server.
void BufferSyncProxy::InitConnection() {
  server_->SendCall(BufferRPCImpl::INIT_CONNECTION, NULL, 0, NULL, 0);
}

// Implements the proxy CloseConnection, forwarding the call to the RPC server.
void BufferSyncProxy::CloseConnection() {
  server_->SendCall(BufferRPCImpl::CLOSE_CONNECTION, NULL, 0, NULL, 0);
}

unsigned int BufferSyncProxy::RegisterSharedMemory(RPCShmHandle buffer,
                                                   size_t size) {
  RPCHandle handles[1] = {buffer};
  return server_->SendCall(BufferRPCImpl::REGISTER_SHARED_MEMORY, &size,
                           sizeof(size), handles, 1);
}

void BufferSyncProxy::UnregisterSharedMemory(unsigned int shm_id) {
  server_->SendCall(BufferRPCImpl::UNREGISTER_SHARED_MEMORY, &shm_id,
                    sizeof(shm_id), NULL, 0);
}

// Implements the proxy SetCommandBuffer, forwarding the call with its
// arguments to the RPC server.
void BufferSyncProxy::SetCommandBuffer(unsigned int shm_id,
                                       ptrdiff_t offset,
                                       size_t size,
                                       CommandBufferOffset start_get) {
  BufferRPCImpl::SetCommandBufferStruct params;
  params.shm_id = shm_id;
  params.offset = offset;
  params.size = size;
  params.start_get = start_get;
  server_->SendCall(BufferRPCImpl::SET_COMMAND_BUFFER, &params, sizeof(params),
                    NULL, 0);
}

// Implements the proxy Put, forwarding the call with its argument to the RPC
// server.
void BufferSyncProxy::Put(CommandBufferOffset offset) {
  server_->SendCall(BufferRPCImpl::PUT, &offset, sizeof(offset), NULL, 0);
}

// Implements the proxy Get, forwarding the call to the RPC server.
CommandBufferOffset BufferSyncProxy::Get() {
  return server_->SendCall(BufferRPCImpl::GET, NULL, 0, NULL, 0);
}

// Implements the proxy GetToken, forwarding the call to the RPC server.
unsigned int BufferSyncProxy::GetToken() {
  return server_->SendCall(BufferRPCImpl::GET_TOKEN, NULL, 0, NULL, 0);
}

// Implements the proxy WaitGetChanges, forwarding the call with its argument
// to the RPC server.
CommandBufferOffset BufferSyncProxy::WaitGetChanges(
    CommandBufferOffset current_value) {
  return server_->SendCall(BufferRPCImpl::WAIT_GET_CHANGES, &current_value,
                           sizeof(current_value), NULL, 0);
}

// Implements the proxy SignalGetChanges, forwarding the call with its
// arguments to the RPC server.
void BufferSyncProxy::SignalGetChanges(CommandBufferOffset current_value,
                                       int rpc_message_id) {
  BufferRPCImpl::SignalGetChangesStruct params;
  params.current_value = current_value;
  params.rpc_message_id = rpc_message_id;
  server_->SendCall(BufferRPCImpl::SIGNAL_GET_CHANGES, &params, sizeof(params),
                    NULL, 0);
}

// Implements the proxy GetStatus, forwarding the call to the RPC server.
BufferSyncInterface::ParserStatus BufferSyncProxy::GetStatus() {
  return static_cast<BufferSyncInterface::ParserStatus>(
      server_->SendCall(BufferRPCImpl::GET_STATUS, NULL, 0, NULL, 0));
}

// Implements the proxy GetParseError, forwarding the call to the RPC server.
BufferSyncInterface::ParseError BufferSyncProxy::GetParseError() {
  return static_cast<ParseError>(
      server_->SendCall(BufferRPCImpl::GET_PARSE_ERROR, NULL, 0, NULL, 0));
}

}  // namespace command_buffer
}  // namespace o3d
