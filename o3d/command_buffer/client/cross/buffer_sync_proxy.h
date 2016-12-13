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
// client side: a proxy implementation of BufferSyncInterface, forwarding calls
// to a RPC send interface.

#ifndef O3D_COMMAND_BUFFER_CLIENT_CROSS_BUFFER_SYNC_PROXY_H_
#define O3D_COMMAND_BUFFER_CLIENT_CROSS_BUFFER_SYNC_PROXY_H_

#include "command_buffer/common/cross/rpc.h"
#include "command_buffer/common/cross/buffer_sync_api.h"

namespace o3d {
namespace command_buffer {

// Class implementing the Command Buffer Synchronous API, forwarding all the
// calls to a RPC server, according to the (trivial) protocol defined in
// BufferRPCImpl.
class BufferSyncProxy : public BufferSyncInterface {
 public:
  explicit BufferSyncProxy(RPCSendInterface *server) : server_(server) {}
  virtual ~BufferSyncProxy() {}

  // Implements the InitConnection call, forwarding it to the RPC server.
  virtual void InitConnection();

  // Implements the CloseConnection call, forwarding it to the RPC server.
  virtual void CloseConnection();

  // Implements the RegisterSharedMemory call, forwarding it to the RPC server.
  virtual unsigned int RegisterSharedMemory(RPCShmHandle buffer, size_t size);

  // Implements the UnregisterSharedMemory call, forwarding it to the RPC
  // server.
  virtual void UnregisterSharedMemory(unsigned int shm_id);

  // Implements the SetCommandBuffer call, forwarding it to the RPC server.
  virtual void SetCommandBuffer(unsigned int shm_id,
                                ptrdiff_t offset,
                                size_t size,
                                CommandBufferOffset start_get);

  // Implements the Put call, forwarding it to the RPC server.
  virtual void Put(CommandBufferOffset offset);

  // Implements the Get call, forwarding it to the RPC server.
  virtual CommandBufferOffset Get();

  // Implements the GetToken call, forwarding it to the RPC server.
  virtual unsigned int GetToken();

  // Implements the WaitGetChanges call, forwarding it to the RPC server.
  virtual CommandBufferOffset WaitGetChanges(
      CommandBufferOffset current_value);

  // Implements the SignalGetChanges call, forwarding it to the RPC server.
  virtual void SignalGetChanges(CommandBufferOffset current_value,
                                int rpc_message_id);

  // Implements the GetStatus call, forwarding it to the RPC server.
  virtual ParserStatus GetStatus();

  // Implements the GetParseError call, forwarding it to the RPC server.
  virtual ParseError GetParseError();
 private:
  RPCSendInterface *server_;
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_CLIENT_CROSS_BUFFER_SYNC_PROXY_H_
