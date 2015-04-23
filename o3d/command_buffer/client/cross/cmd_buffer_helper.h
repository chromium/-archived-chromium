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


// This file contains the command buffer helper class.

#ifndef O3D_COMMAND_BUFFER_CLIENT_CROSS_CMD_BUFFER_HELPER_H_
#define O3D_COMMAND_BUFFER_CLIENT_CROSS_CMD_BUFFER_HELPER_H_

#include "command_buffer/common/cross/logging.h"
#include "command_buffer/common/cross/buffer_sync_api.h"
#include "command_buffer/common/cross/cmd_buffer_format.h"

namespace o3d {
namespace command_buffer {

// Command buffer helper class. This class simplifies ring buffer management:
// it will allocate the buffer, give it to the buffer interface, and let the
// user add commands to it, while taking care of the synchronization (put and
// get). It also provides a way to ensure commands have been executed, through
// the token mechanism:
//
// helper.AddCommand(...);
// helper.AddCommand(...);
// unsigned int token = helper.InsertToken();
// helper.AddCommand(...);
// helper.AddCommand(...);
// [...]
//
// helper.WaitForToken(token);  // this doesn't return until the first two
//                              // commands have been executed.
class CommandBufferHelper {
 public:
  // Constructs a CommandBufferHelper object. The helper needs to be
  // initialized by calling Init() before use.
  // Parameters:
  //   interface: the buffer interface the helper sends commands to.
  explicit CommandBufferHelper(BufferSyncInterface *interface);
  ~CommandBufferHelper();

  // Initializes the command buffer by allocating shared memory.
  // Parameters:
  //   entry_count: the number of entries in the buffer. Note that commands
  //     sent through the buffer must use at most entry_count-2 arguments
  //     (entry_count-1 size).
  // Returns:
  //   true if successful.
  bool Init(unsigned int entry_count);

  // Flushes the commands, setting the put pointer to let the buffer interface
  // know that new commands have been added.
  void Flush() {
    interface_->Put(put_);
  }

  // Waits until all the commands have been executed.
  void Finish();

  // Waits until a given number of available entries are available.
  // Parameters:
  //   count: number of entries needed. This value must be at most
  //     the size of the buffer minus one.
  void WaitForAvailableEntries(unsigned int count);

  // Adds a command to the command buffer. This may wait until sufficient space
  // is available.
  // Parameters:
  //   command: the command index.
  //   arg_count: the number of arguments for the command.
  //   args: the arguments for the command (these are copied before the
  //     function returns).
  void AddCommand(unsigned int command,
                  unsigned int arg_count,
                  CommandBufferEntry *args) {
    CommandHeader header;
    header.size = arg_count + 1;
    header.command = command;
    WaitForAvailableEntries(header.size);
    entries_[put_++].value_header = header;
    for (unsigned int i = 0; i < arg_count; ++i) {
      entries_[put_++] = args[i];
    }
    DCHECK_LE(put_, entry_count_);
    if (put_ == entry_count_) put_ = 0;
  }

  // Inserts a new token into the command buffer. This token either has a value
  // different from previously inserted tokens, or ensures that previously
  // inserted tokens with that value have already passed through the command
  // stream.
  // Returns:
  //   the value of the new token.
  unsigned int InsertToken();

  // Waits until the token of a particular value has passed through the command
  // stream (i.e. commands inserted before that token have been executed).
  // NOTE: This will call Flush if it needs to block.
  // Parameters:
  //   the value of the token to wait for.
  void WaitForToken(unsigned int token);

  // Returns the buffer interface used to send synchronous commands.
  BufferSyncInterface *interface() { return interface_; }

 private:
  // Waits until get changes, updating the value of get_.
  void WaitForGetChange();

  // Returns the number of available entries (they may not be contiguous).
  unsigned int AvailableEntries() {
    return (get_ - put_ - 1 + entry_count_) % entry_count_;
  }

  BufferSyncInterface *interface_;
  CommandBufferEntry *entries_;
  unsigned int entry_count_;
  unsigned int token_;
  unsigned int last_token_read_;
  RPCShmHandle shm_handle_;
  unsigned int shm_id_;
  CommandBufferOffset get_;
  CommandBufferOffset put_;

  friend class CommandBufferHelperTest;
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_CLIENT_CROSS_CMD_BUFFER_HELPER_H_
