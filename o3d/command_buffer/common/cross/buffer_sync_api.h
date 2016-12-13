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


// This file defines the Command Buffer Synchronous API.

#ifndef O3D_COMMAND_BUFFER_COMMON_CROSS_BUFFER_SYNC_API_H__
#define O3D_COMMAND_BUFFER_COMMON_CROSS_BUFFER_SYNC_API_H__

#include "command_buffer/common/cross/rpc.h"

namespace o3d {
namespace command_buffer {

// Command buffer type.
typedef ptrdiff_t CommandBufferOffset;

// Interface class for the Command Buffer Synchronous API.
// This is the part of the command buffer API that is accessible through the
// RPC mechanism, synchronously.
class BufferSyncInterface {
 public:
  // Status of the command buffer service. It does not process commands
  // (meaning: get will not change) unless in PARSING state.
  enum ParserStatus {
    NOT_CONNECTED,  // The service is not connected - initial state.
    NO_BUFFER,      // The service is connected but no buffer was set.
    PARSING,        // The service is connected, and parsing commands from the
                    // buffer.
    PARSE_ERROR,    // Parsing stopped because a parse error was found.
  };

  enum ParseError {
    PARSE_NO_ERROR,
    PARSE_INVALID_SIZE,
    PARSE_OUT_OF_BOUNDS,
    PARSE_UNKNOWN_COMMAND,
    PARSE_INVALID_ARGUMENTS,
  };

  // Invalid shared memory Id, returned by RegisterSharedMemory in case of
  // failure.
  static const unsigned int kInvalidSharedMemoryId = 0xffffffffU;

  BufferSyncInterface() {}
  virtual ~BufferSyncInterface() {}

  // Initializes the connection with the client.
  virtual void InitConnection() = 0;

  // Closes the connection with the client.
  virtual void CloseConnection() = 0;

  // Registers a shared memory buffer. While a buffer is registered, it can be
  // accessed by the service, including the underlying asynchronous API,
  // through a single identifier.
  // Parameters:
  //   buffer: the shared memory buffer handle.
  // Returns:
  //   an identifier for the shared memory, or kInvalidSharedMemoryId in case
  //   of failure.
  virtual unsigned int RegisterSharedMemory(RPCShmHandle buffer,
                                            size_t size) = 0;

  // Unregisters a shared memory buffer.
  // Parameters:
  //   shm_id: the identifier for the shared memory buffer.
  virtual void UnregisterSharedMemory(unsigned int shm_id) = 0;

  // Initializes the command buffer.
  // Parameters:
  //    shm_id: the registered memory buffer in which the command buffer
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
                                CommandBufferOffset start_get) = 0;

  // Sets the value of the Put pointer.
  // Parameters:
  //   offset: the new value of the Put pointer, as an offset into the command
  //           buffer.
  virtual void Put(CommandBufferOffset offset) = 0;

  // Gets the value of the Get pointer.
  // Returns:
  //   the current value of the Get pointer, as an offset into the command
  //   buffer.
  virtual CommandBufferOffset Get() = 0;

  // Gets the current token value.
  // Returns:
  //   the current token value.
  virtual unsigned int GetToken() = 0;

  // Waits until Get changes from the currently known value.
  // Parameters:
  //    current_value: the currently known value. This call will block until
  //                   Get is different from that value (returning immediately
  //                   if it is different).
  // Returns:
  //   the current (changed) value of Get.
  virtual CommandBufferOffset WaitGetChanges(
      CommandBufferOffset current_value) = 0;

  // Asks the service to signal the client when Get changes from the currently
  // known value. This is a non-blocking version of WaitGetChanges.
  // Parameters:
  //   current_value: the currently known value of Get.
  //   rpc_message_id: the RPC message ID to call on the client when Get is
  //                   different from current_value. That RPC is called
  //                   immediately if Get is already different from
  //                   current_value.
  virtual void SignalGetChanges(CommandBufferOffset current_value,
                                int rpc_message_id) = 0;

  // Gets the status of the service.
  // Returns:
  //   The status of the service.
  virtual ParserStatus GetStatus() = 0;

  // Gets the current parse error. The current parse error is set when the
  // service is in the PARSE_ERROR status. It may also be set while in the
  // PARSING state, if a recoverable error (like PARSE_UNKNOWN_METHOD) was
  // encountered. Getting the error resets it to PARSE_NO_ERROR.
  // Returns:
  //   The current parse error.
  virtual ParseError GetParseError() = 0;
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_COMMON_CROSS_BUFFER_SYNC_API_H__
