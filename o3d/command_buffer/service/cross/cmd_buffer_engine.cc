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


// This file contains the implementation of the command buffer engine.

#include "command_buffer/service/cross/buffer_rpc.h"
#include "command_buffer/service/cross/cmd_buffer_engine.h"

namespace o3d {
namespace command_buffer {

// Creates a RPC implementation using 'this' as the handler, and a RPC server
// for it.
CommandBufferEngine::CommandBufferEngine(AsyncAPIInterface *handler)
    : buffer_rpc_impl_(),
      process_interface_(NULL),
      parser_(),
      handler_(handler),
      client_rpc_(NULL),
      token_(0),
      status_(NOT_CONNECTED),
      signal_change_(false),
      signal_rpc_message_id_(0),
      parse_error_(PARSE_NO_ERROR) {
  buffer_rpc_impl_.reset(new BufferRPCImpl(this));
}

CommandBufferEngine::~CommandBufferEngine() {}

// Inits the connection. Registers the client RPC service.
void CommandBufferEngine::InitConnection() {
  status_ = NO_BUFFER;
}

// Closes the connection. Executes all remaining commands.
void CommandBufferEngine::CloseConnection() {
  FinishParsing();
  status_ = NOT_CONNECTED;
  parser_.reset(NULL);
}

// Adds the shared memory buffer somewhere into the list, return the index in
// the list as the handle.  Either find a hole in the list, or add it at the
// end. We don't want to invalidate exiting indices.
unsigned int CommandBufferEngine::RegisterSharedMemory(
    RPCShmHandle handle,
    size_t size) {
  void *address = MapShm(handle, size);
  if (!address) return kInvalidSharedMemoryId;
  MemoryMapping mapping = {address, size};
  for (unsigned int i = 0; i < shared_memory_buffers_.size(); ++i) {
    if (shared_memory_buffers_[i].address == NULL) {
      shared_memory_buffers_[i] = mapping;
      return i;
    }
  }
  shared_memory_buffers_.push_back(mapping);
  return static_cast<unsigned int>(shared_memory_buffers_.size() - 1);
}

// Sets the list entry for the shared memory buffer to NULL.  Don't erase() the
// entry, We don't want to invalidate exiting indices.
void CommandBufferEngine::UnregisterSharedMemory(unsigned int shm_id) {
  if ((shm_id >= shared_memory_buffers_.size()) ||
      !shared_memory_buffers_[shm_id].size) {
    LOG(ERROR) << "Trying to unregister a non-registered shared memory";
    return;
  }
  MemoryMapping &mapping = shared_memory_buffers_[shm_id];
  UnmapShm(mapping.address, mapping.size);
  mapping.address = NULL;
  mapping.size = 0;
}

// Sets the command buffer. Executes all remaining commands in the old buffer
// (if any) and creates a new command parser.
void CommandBufferEngine::SetCommandBuffer(unsigned int shm_id,
                                           ptrdiff_t offset,
                                           size_t size,
                                           CommandBufferOffset start_get) {
  if ((shm_id >= shared_memory_buffers_.size()) ||
      !shared_memory_buffers_[shm_id].size) {
    LOG(ERROR) << "Trying to set the command buffer from a non-registered "
               << "shared memory";
    return;
  }
  if (status_ == NOT_CONNECTED) return;
  FinishParsing();
  parser_.reset(new CommandParser(shared_memory_buffers_[shm_id].address,
                                  shared_memory_buffers_[shm_id].size, offset,
                                  size, start_get, handler_));
  status_ = PARSING;
  parse_error_ = PARSE_NO_ERROR;
}

// Changes the put value.
void CommandBufferEngine::Put(CommandBufferOffset offset) {
  if (parser_.get()) {
    parser_->set_put(offset);
  }
}

// Retrieves the get value. This returns -1 if there is no current parser.
CommandBufferOffset CommandBufferEngine::Get() {
  if (parser_.get()) {
    return parser_->get();
  } else {
    return -1;
  }
}

// Retrieves the current token value.
unsigned int CommandBufferEngine::GetToken() {
  return token_;
}

// Executes commands until get is different from the value passed in. It will
// return immediately if the get value is already different, or if the engine
// is not in the PARSING status, or if the buffer is empty. It will return -1
// if there is no current buffer.
CommandBufferOffset CommandBufferEngine::WaitGetChanges(
    CommandBufferOffset current_value) {
  if (parser_.get()) {
    while (status_ == PARSING &&
           parser_->get() == current_value &&
           !parser_->IsEmpty()) {
      ProcessOneCommand();
    }
    return parser_->get();
  } else {
    return -1;
  }
}

// Signals the client when get gets different from the value passed in. If get
// is already different, or if the engine is not in the PARSING status, that
// will happen immediately, otherwise it will happen when commands get
// executed, moving the get pointer.
void CommandBufferEngine::SignalGetChanges(CommandBufferOffset current_value,
                                           int rpc_message_id) {
  if (status_ != PARSING || parser_->get() != current_value) {
    DoSignalChangedGet(rpc_message_id);
  } else {
    signal_change_ = true;
    signal_rpc_message_id_ = rpc_message_id;
  }
}

// Gets the memory address from the list entry.
void *CommandBufferEngine::GetSharedMemoryAddress(unsigned int shm_id) {
  if ((shm_id >= shared_memory_buffers_.size()) ||
      !shared_memory_buffers_[shm_id].size) {
    LOG(ERROR) << "Trying to get the address of a non-registered shared memory";
    return NULL;
  }
  return shared_memory_buffers_[shm_id].address;
}

// Gets the memory size from the list entry.
size_t CommandBufferEngine::GetSharedMemorySize(unsigned int shm_id) {
  if ((shm_id >= shared_memory_buffers_.size()) ||
      !shared_memory_buffers_[shm_id].size) {
    LOG(ERROR) << "Trying to get the size of a non-registered shared memory";
    return 0;
  }
  return shared_memory_buffers_[shm_id].size;
}

// Gets the status.
BufferSyncInterface::ParserStatus CommandBufferEngine::GetStatus() {
  return status_;
}

// Gets the current parse error, reset it to PARSE_NO_ERROR.
BufferSyncInterface::ParseError CommandBufferEngine::GetParseError() {
  ParseError error = parse_error_;
  parse_error_ = PARSE_NO_ERROR;
  return error;
}

// Finishes parsing, executing all the commands until the buffer is empty, or a
// parsing error occurs.
void CommandBufferEngine::FinishParsing() {
  // terminates current parsing, that is, execute all the commands
  // NOTE: status_ == PARSING implies parser_ != NULL
  while (status_ == PARSING && !parser_->IsEmpty()) {
    ProcessOneCommand();
  }
}

// Processes one command from the command buffer. This must only be called when
// in the PARSING status.
// This will update the status_ and the parse_error_ fields if an error occurs.
void CommandBufferEngine::ProcessOneCommand() {
  DCHECK_EQ(PARSING, status_);
  DCHECK(parser_.get());
  ParseError result = parser_->ProcessCommand();
  switch (result) {
    case PARSE_NO_ERROR:
      break;
    case PARSE_OUT_OF_BOUNDS:
    case PARSE_INVALID_SIZE:
      status_ = PARSE_ERROR;
      // Always override the error, to properly signal the stopping condition.
      parse_error_ = result;
      break;
    case PARSE_INVALID_ARGUMENTS:
    case PARSE_UNKNOWN_COMMAND:
      // Only set the error if it is not set already.
      if (parse_error_ == PARSE_NO_ERROR) {
        parse_error_ = result;
      }
      break;
  }
  // get has changed, signal the client if needed.
  if (signal_change_) {
    DoSignalChangedGet(signal_rpc_message_id_);
    signal_change_ = false;
  }
}

// Executes the main loop. While there are commands in the buffer, processes
// them one by one, checking for RPC messages between each of them (executing
// all of them). If the buffer is empty, block until a RPC message comes.
void CommandBufferEngine::DoMainLoop() {
  while (DoWork()) { }
  // Clean up if needed: execute all pending commands, then close the
  // connection.
  if (status_ != NOT_CONNECTED) CloseConnection();
}

bool CommandBufferEngine::HasWork() {
  return (status_ == PARSING && !parser_->IsEmpty()) ||
      process_interface_->HasMessage();
}

bool CommandBufferEngine::DoWork() {
  if (status_ == PARSING && !parser_->IsEmpty()) {
    bool running = true;
    // process as many messages as available but do not block.
    while (process_interface_->HasMessage()) {
      running = process_interface_->ProcessMessage();
    }
    if (running) ProcessOneCommand();
    return running;
  } else {
    // call ProcessMessage, always blocking. We have nothing else to do.
    return process_interface_->ProcessMessage();
  }
}

// Signals that get has changed, sending a RPC message back to the client. It
// will send -1 if there is no current buffer.
void CommandBufferEngine::DoSignalChangedGet(int rpc_message_id) {
  DCHECK(client_rpc_);
  CommandBufferOffset get = parser_.get() ? parser_->get() : -1;
  client_rpc_->SendCall(rpc_message_id, &get, sizeof(get), NULL, 0);
}

}  // namespace command_buffer
}  // namespace o3d
