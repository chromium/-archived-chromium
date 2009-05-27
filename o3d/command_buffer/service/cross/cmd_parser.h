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


// This file contains the command parser class.

#ifndef O3D_COMMAND_BUFFER_SERVICE_CROSS_CMD_PARSER_H__
#define O3D_COMMAND_BUFFER_SERVICE_CROSS_CMD_PARSER_H__

#include "core/cross/types.h"
#include "command_buffer/common/cross/rpc.h"
#include "command_buffer/common/cross/buffer_sync_api.h"
#include "command_buffer/common/cross/cmd_buffer_format.h"

namespace o3d {
namespace command_buffer {

class AsyncAPIInterface;

// Command parser class. This class parses commands from a shared memory
// buffer, to implement some asynchronous RPC mechanism.
class CommandParser {
 public:
  CommandParser(void *shm_address,
                size_t shm_size,
                ptrdiff_t offset,
                size_t size,
                CommandBufferOffset start_get,
                AsyncAPIInterface *handler);

  // Gets the "get" pointer. The get pointer is an index into the command
  // buffer considered as an array of CommandBufferEntry.
  CommandBufferOffset get() const { return get_; }

  // Sets the "put" pointer. The put pointer is an index into the command
  // buffer considered as an array of CommandBufferEntry.
  void set_put(CommandBufferOffset put) { put_ = put; }

  // Gets the "put" pointer. The put pointer is an index into the command
  // buffer considered as an array of CommandBufferEntry.
  CommandBufferOffset put() const { return put_; }

  // Checks whether there are commands to process.
  bool IsEmpty() const { return put_ == get_; }

  // Processes one command, updating the get pointer. This will return an error
  // if there are no commands in the buffer.
  BufferSyncInterface::ParseError ProcessCommand();

  // Processes all commands until get == put.
  BufferSyncInterface::ParseError ProcessAllCommands();

 private:
  CommandBufferOffset get_;
  CommandBufferOffset put_;
  CommandBufferEntry *buffer_;
  size_t entry_count_;
  AsyncAPIInterface *handler_;
};

// This class defines the interface for an asynchronous API handler, that
// is responsible for de-multiplexing commands and their arguments.
class AsyncAPIInterface {
 public:
  AsyncAPIInterface() {}
  virtual ~AsyncAPIInterface() {}

  // Executes a command.
  // Parameters:
  //    command: the command index.
  //    arg_count: the number of CommandBufferEntry arguments.
  //    args: the arguments.
  // Returns:
  //   BufferSyncInterface::NO_ERROR if no error was found, one of
  //   BufferSyncInterface::ParseError otherwise.
  virtual BufferSyncInterface::ParseError DoCommand(
      unsigned int command,
      unsigned int arg_count,
      CommandBufferEntry *args) = 0;
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SERVICE_CROSS_CMD_PARSER_H__
