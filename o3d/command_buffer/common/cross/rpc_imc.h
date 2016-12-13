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


#ifndef O3D_COMMAND_BUFFER_COMMON_CROSS_RPC_IMC_H_
#define O3D_COMMAND_BUFFER_COMMON_CROSS_RPC_IMC_H_

#include "third_party/native_client/googleclient/native_client/src/shared/imc/nacl_imc.h"
#include "base/scoped_ptr.h"
#include "command_buffer/common/cross/rpc.h"

namespace o3d {
namespace command_buffer {

class IMCSender : public RPCSendInterface {
 public:
  explicit IMCSender(nacl::HtpHandle handle) : handle_(handle) {}
  virtual ~IMCSender() {}
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
  nacl::HtpHandle handle_;
};

class IMCMessageProcessor : public RPCProcessInterface {
 public:
  IMCMessageProcessor(nacl::HtpHandle handle, RPCImplInterface *impl)
      : handle_(handle),
        impl_(impl),
        has_message_(false),
        incoming_message_id_(0),
        incoming_message_size_(0),
        incoming_message_handles_(0),
        data_size_(0),
        handle_count_(0) {}
  virtual ~IMCMessageProcessor() {}
  // Processes one message, blocking if necessary until a message is available
  // or the server is killed. This will dispatch the received message to the
  // RPC implementation, and send back a response message with the return
  // value to the client.
  // Returns:
  //    false if the server was killed.
  virtual bool ProcessMessage();

  // Checks whether a message is available for processing - allowing to test if
  // ProcessMessage will block.
  // Returns:
  //    true if a message is available.
  virtual bool HasMessage();
 private:
  bool GetMessageIDSize(bool wait);
  nacl::HtpHandle handle_;
  RPCImplInterface *impl_;
  bool has_message_;
  int incoming_message_id_;
  size_t incoming_message_size_;
  size_t incoming_message_handles_;
  size_t data_size_;
  scoped_array<char> data_;
  size_t handle_count_;
  scoped_array<RPCHandle> handles_;
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_COMMON_CROSS_RPC_IMC_H_
