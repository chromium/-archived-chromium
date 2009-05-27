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


// This file contains the implementation of the command buffer helper class.

#include "command_buffer/client/cross/cmd_buffer_helper.h"

namespace o3d {
namespace command_buffer {

CommandBufferHelper::CommandBufferHelper(BufferSyncInterface *interface)
    : interface_(interface),
      entries_(NULL),
      entry_count_(0),
      token_(0) {
  // The interface should be connected already.
  DCHECK_NE(BufferSyncInterface::NOT_CONNECTED, interface_->GetStatus());
}

bool CommandBufferHelper::Init(unsigned int entry_count) {
  if (entry_count == 0)
    return false;
  size_t size = entry_count * sizeof(CommandBufferEntry);  // NOLINT
  shm_handle_ = CreateShm(size);
  if (shm_handle_ == kRPCInvalidHandle)
    return false;
  void *address = MapShm(shm_handle_, size);
  if (!address) {
    DestroyShm(shm_handle_);
    shm_handle_ = kRPCInvalidHandle;
    return false;
  }
  entries_ = static_cast<CommandBufferEntry *>(address);
  entry_count_ = entry_count;
  shm_id_ = interface_->RegisterSharedMemory(shm_handle_, size);
  interface_->SetCommandBuffer(shm_id_, 0, size, 0);
  get_ = interface_->Get();
  put_ = get_;
  last_token_read_ = interface_->GetToken();
  return true;
}

CommandBufferHelper::~CommandBufferHelper() {
  if (entries_) {
    interface_->UnregisterSharedMemory(shm_id_);
    UnmapShm(entries_, entry_count_ * sizeof(CommandBufferEntry));  // NOLINT
    DestroyShm(shm_handle_);
  }
}

// Calls Flush() and then waits until the buffer is empty. Break early if the
// error is set.
void CommandBufferHelper::Finish() {
  Flush();
  while (put_ != get_) {
    WaitForGetChange();
  }
}

// Inserts a new token into the command stream. It uses an increasing value
// scheme so that we don't lose tokens (a token has passed if the current token
// value is higher than that token). Calls Finish() if the token value wraps,
// which will be rare.
unsigned int CommandBufferHelper::InsertToken() {
  ++token_;
  CommandBufferEntry args;
  args.value_uint32 = token_;
  AddCommand(SET_TOKEN, 1, &args);
  if (token_ == 0) {
    // we wrapped
    Finish();
    last_token_read_ = interface_->GetToken();
    DCHECK_EQ(token_, last_token_read_);
  }
  return token_;
}

// Waits until the current token value is greater or equal to the value passed
// in argument.
void CommandBufferHelper::WaitForToken(unsigned int token) {
  if (last_token_read_ >= token) return;  // fast path.
  if (token > token_) return;  // we wrapped
  Flush();
  last_token_read_ = interface_->GetToken();
  while (last_token_read_ < token) {
    if (get_ == put_) {
      LOG(FATAL) << "Empty command buffer while waiting on a token.";
      return;
    }
    WaitForGetChange();
    last_token_read_ = interface_->GetToken();
  }
}

// Waits for get to change. In case get doesn't change or becomes invalid,
// check for an error.
void CommandBufferHelper::WaitForGetChange() {
  CommandBufferOffset new_get = interface_->WaitGetChanges(get_);
  if (new_get == get_ || new_get == -1) {
    // If get_ didn't change or is invalid (-1), it means an error may have
    // occured. Check that.
    BufferSyncInterface::ParserStatus status = interface_->GetStatus();
    if (status != BufferSyncInterface::PARSING) {
      switch (status) {
        case BufferSyncInterface::NOT_CONNECTED:
          LOG(FATAL) << "Service disconnected.";
          return;
        case BufferSyncInterface::NO_BUFFER:
          LOG(FATAL) << "Service doesn't have a buffer set.";
          return;
        case BufferSyncInterface::PARSE_ERROR: {
          BufferSyncInterface::ParseError error = interface_->GetParseError();
          LOG(WARNING) << "Parse error: " << error;
          return;
        }
        case BufferSyncInterface::PARSING:
          break;
      }
    }
  }
  get_ = new_get;
}

// Waits for available entries, basically waiting until get >= put + count + 1.
// It actually waits for contiguous entries, so it may need to wrap the buffer
// around, adding noops. Thus this function may change the value of put_.
// The function will return early if an error occurs, in which case the
// available space may not be available.
void CommandBufferHelper::WaitForAvailableEntries(unsigned int count) {
  CHECK(count < entry_count_);
  if (put_ + count > entry_count_) {
    // There's not enough room between the current put and the end of the
    // buffer, so we need to wrap. We will add noops all the way to the end,
    // but we need to make sure get wraps first, actually that get is 1 or
    // more (since put will wrap to 0 after we add the noops).
    DCHECK_LE(1, put_);
    Flush();
    while (get_ > put_ || get_ == 0) WaitForGetChange();
    // Add the noops. By convention, a noop is a command 0 with no args.
    CommandHeader header;
    header.size = 1;
    header.command = 0;
    while (put_ < entry_count_) {
      entries_[put_++].value_header = header;
    }
    put_ = 0;
  }
  // If we have enough room, return immediatly.
  if (count <= AvailableEntries()) return;
  // Otherwise flush, and wait until we do have enough room.
  Flush();
  while (AvailableEntries() < count) WaitForGetChange();
}

}  // namespace command_buffer
}  // namespace o3d
