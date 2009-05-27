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


// This file defines the CommandBufferEngine class, providing the main loop for
// the service, exposing the RPC API, managing the command parser.

#ifndef O3D_COMMAND_BUFFER_SERVICE_CROSS_CMD_BUFFER_ENGINE_H__
#define O3D_COMMAND_BUFFER_SERVICE_CROSS_CMD_BUFFER_ENGINE_H__

#include <vector>
#include "base/scoped_ptr.h"
#include "command_buffer/common/cross/buffer_sync_api.h"
#include "command_buffer/service/cross/cmd_parser.h"

namespace o3d {
namespace command_buffer {

class BufferRPCImpl;

class CommandBufferEngine : public BufferSyncInterface {
 public:
  explicit CommandBufferEngine(AsyncAPIInterface *handler);
  virtual ~CommandBufferEngine();

  // Initializes the connection with the client.
  virtual void InitConnection();

  // Closes the connection with the client.
  virtual void CloseConnection();

  // Registers a shared memory buffer. While a buffer is registered, it can be
  // accessed by the service, including the underlying asynchronous API,
  // through a single identifier.
  // Parameters:
  //   buffer: the shared memory buffer handle.
  // Returns:
  //   an identifier for the shared memory.
  virtual unsigned int RegisterSharedMemory(RPCShmHandle buffer, size_t size);

  // Unregisters a shared memory buffer.
  // Parameters:
  //   shm_id: the identifier for the shared memory buffer.
  virtual void UnregisterSharedMemory(unsigned int shm_id);

  // Initializes the command buffer.
  // Parameters:
  //    buffer: the memory buffer descriptor in which the command buffer
  //            resides.
  //    offset: the offset of the command buffer, relative to the memory
  //            buffer.
  //    size: the size of the command buffer.
  //    start_get: the inital value for the Get pointer, relative to the
  //               command buffer, where the parser will start interpreting
  //               commands. Put is also initialized to that value.
  virtual void SetCommandBuffer(unsigned int shm_id,
                                ptrdiff_t offset,
                                size_t size,
                                CommandBufferOffset start_get);

  // Sets the value of the Put pointer.
  // Parameters:
  //   offset: the new value of the Put pointer, as an offset into the command
  //           buffer.
  virtual void Put(CommandBufferOffset offset);

  // Gets the value of the Get pointer.
  // Returns:
  //   the current value of the Get pointer, as an offset into the command
  //   buffer.
  virtual CommandBufferOffset Get();

  // Gets the current token value.
  // Returns:
  //   the current token value.
  virtual unsigned int GetToken();

  // Waits until Get changes from the currently known value.
  // Parameters:
  //    current_value: the currently known value. This call will block until
  //                   Get is different from that value (returning immediately
  //                   if it is already different).
  // Returns:
  //   the current (changed) value of Get.
  virtual CommandBufferOffset WaitGetChanges(
      CommandBufferOffset current_value);

  // Asks the service to signal the client when Get changes from the currently
  // known value. This is a non-blocking version of WaitGetChanges.
  // Parameters:
  //   current_value: the currently known value of Get.
  //   rpc_message_id: the RPC message ID to call on the client when Get is
  //                   different from current_value. That RPC is called
  //                   immediately if Get is already different from
  //                   current_value.
  virtual void SignalGetChanges(CommandBufferOffset current_value,
                                int rpc_message_id);

  // Gets the status of the service.
  // Returns:
  //   The status of the service.
  virtual ParserStatus GetStatus();

  // Gets the current parse error. The current parse error is set when the
  // service is in the PARSE_ERROR status. It may also be set while in the
  // PARSING state, if a recoverable error (like PARSE_UNKNOWN_METHOD) was
  // encountered. Getting the error resets it to PARSE_NO_ERROR.
  // Returns:
  //   The current parse error.
  virtual ParseError GetParseError();

  // Gets the base address of a registered shared memory buffer.
  // Parameters:
  //   shm_id: the identifier for the shared memory buffer.
  void *GetSharedMemoryAddress(unsigned int shm_id);

  // Gets the size of a registered shared memory buffer.
  // Parameters:
  //   shm_id: the identifier for the shared memory buffer.
  size_t GetSharedMemorySize(unsigned int shm_id);

  // Executes the main loop: parse commands and execute RPC calls until the
  // server is killed.
  void DoMainLoop();

  // Returns whether or not the engine has work to do (process synchronous or
  // asynchronous commands).
  bool HasWork();

  // Does some work (process synchronous or asynchronous commands). It will not
  // block if HasWork() returns true.
  // Returns:
  //   true if the engine should keep running, false if it has been sent a
  //   command to terminate.
  bool DoWork();

  // Gets the RPC server.
  BufferRPCImpl *rpc_impl() const { return buffer_rpc_impl_.get(); }

  // Sets the RPC processing interface.
  void set_process_interface(RPCProcessInterface *iface) {
    process_interface_ = iface;
  }

  // Sets the RPC processing interface.
  void set_client_rpc(RPCSendInterface *iface) {
    client_rpc_ = iface;
  }

  // Gets the command parser.
  CommandParser *parser() const { return parser_.get(); }

  // Sets the token value.
  void set_token(unsigned int token) { token_ = token; }
 private:

  // Processes one command from the command buffer.
  void ProcessOneCommand();

  // Sends the signal that get has changed to the client.
  void DoSignalChangedGet(int rpc_message_id);

  // Finish parsing and executing all the commands in the buffer.
  void FinishParsing();

  scoped_ptr<BufferRPCImpl> buffer_rpc_impl_;
  RPCProcessInterface *process_interface_;
  scoped_ptr<CommandParser> parser_;
  AsyncAPIInterface *handler_;
  RPCSendInterface *client_rpc_;
  unsigned int token_;
  ParserStatus status_;
  bool signal_change_;
  int signal_rpc_message_id_;
  ParseError parse_error_;
  struct MemoryMapping {
    void *address;
    size_t size;
  };
  std::vector<MemoryMapping> shared_memory_buffers_;
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SERVICE_CROSS_CMD_BUFFER_ENGINE_H__
