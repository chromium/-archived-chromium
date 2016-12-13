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


// This file contains the implementation of the command parser.

#include "command_buffer/service/cross/cmd_parser.h"

namespace o3d {
namespace command_buffer {

CommandParser::CommandParser(void *shm_address,
                             size_t shm_size,
                             ptrdiff_t offset,
                             size_t size,
                             CommandBufferOffset start_get,
                             AsyncAPIInterface *handler)
    : get_(start_get),
      put_(start_get),
      handler_(handler) {
  // check proper alignments.
  DCHECK_EQ(0, (reinterpret_cast<intptr_t>(shm_address)) % 4);
  DCHECK_EQ(0, offset % 4);
  DCHECK_EQ(0, size % 4);
  // check that the command buffer fits into the memory buffer.
  DCHECK_GE(shm_size, offset + size);
  char * buffer_begin = static_cast<char *>(shm_address) + offset;
  buffer_ = reinterpret_cast<CommandBufferEntry *>(buffer_begin);
  entry_count_ = size / 4;
}

// Process one command, reading the header from the command buffer, and
// forwarding the command index and the arguments to the handler.
// Note that:
// - validation needs to happen on a copy of the data (to avoid race
// conditions). This function only validates the header, leaving the arguments
// validation to the handler, so it can pass a reference to them.
// - get_ is modified *after* the command has been executed.
BufferSyncInterface::ParseError CommandParser::ProcessCommand() {
  CommandBufferOffset get = get_;
  if (get == put_) return BufferSyncInterface::PARSE_NO_ERROR;

  CommandHeader header = buffer_[get].value_header;
  if (header.size == 0) return BufferSyncInterface::PARSE_INVALID_SIZE;
  if (header.size + get > entry_count_)
    return BufferSyncInterface::PARSE_OUT_OF_BOUNDS;
  BufferSyncInterface::ParseError result = handler_->DoCommand(
      header.command, header.size - 1, buffer_ + get + 1);
  get_ = (get + header.size) % entry_count_;
  return result;
}

// Processes all the commands, while the buffer is not empty. Stop if an error
// is encountered.
BufferSyncInterface::ParseError CommandParser::ProcessAllCommands() {
  while (!IsEmpty()) {
    BufferSyncInterface::ParseError error = ProcessCommand();
    if (error) return error;
  }
  return BufferSyncInterface::PARSE_NO_ERROR;
}

}  // namespace command_buffer
}  // namespace o3d
