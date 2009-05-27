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


// This file contains definitions for mock objects, used for testing.

// TODO: This file "manually" defines some mock objects. Using gMock
// would be definitely preferable, unfortunately it doesn't work on Windows
// yet.

#ifndef O3D_COMMAND_BUFFER_SERVICE_CROSS_MOCKS_H__
#define O3D_COMMAND_BUFFER_SERVICE_CROSS_MOCKS_H__

#include <vector>
#include "gmock/gmock.h"
#include "command_buffer/service/cross/cmd_parser.h"
#include "command_buffer/service/cross/cmd_buffer_engine.h"

namespace o3d {
namespace command_buffer {

// Mocks an AsyncAPIInterface, using GMock.
class AsyncAPIMock : public AsyncAPIInterface {
 public:
  AsyncAPIMock() {
    testing::DefaultValue<BufferSyncInterface::ParseError>::Set(
        BufferSyncInterface::PARSE_NO_ERROR);
  }

  // Predicate that matches args passed to DoCommand, by looking at the values.
  class IsArgs {
   public:
    IsArgs(unsigned int arg_count, CommandBufferEntry *args)
        : arg_count_(arg_count),
          args_(args) { }

    bool operator() (CommandBufferEntry *args) const {
      for (unsigned int i = 0; i < arg_count_; ++i) {
        if (args[i].value_uint32 != args_[i].value_uint32) return false;
      }
      return true;
    }

   private:
    unsigned int arg_count_;
    CommandBufferEntry *args_;
  };

  MOCK_METHOD3(DoCommand, BufferSyncInterface::ParseError(
      unsigned int command,
      unsigned int arg_count,
      CommandBufferEntry *args));

  // Sets the engine, to forward SET_TOKEN commands to it.
  void set_engine(CommandBufferEngine *engine) { engine_ = engine; }

  // Forwards the SetToken commands to the engine.
  void SetToken(unsigned int command,
                unsigned int arg_count,
                CommandBufferEntry *args) {
    DCHECK(engine_);
    DCHECK_EQ(1, command);
    DCHECK_EQ(1, arg_count);
    engine_->set_token(args[0].value_uint32);
  }
 private:
  CommandBufferEngine *engine_;
};

class RPCProcessMock : public RPCProcessInterface {
 public:
  RPCProcessMock()
      : would_have_blocked_(false),
        message_count_(0) {
    ON_CALL(*this, ProcessMessage()).WillByDefault(
        testing::Invoke(this, &RPCProcessMock::DefaultProcessMessage));
    ON_CALL(*this, HasMessage()).WillByDefault(
        testing::Invoke(this, &RPCProcessMock::DefaultHasMessage));
  }
  MOCK_METHOD0(ProcessMessage, bool());
  MOCK_METHOD0(HasMessage, bool());

  void Reset() {
    would_have_blocked_ = false;
    message_count_ = 0;
  }

  bool DefaultProcessMessage() {
    if (message_count_ > 0) {
      --message_count_;
    } else {
      would_have_blocked_ = true;
    }
    return true;
  }

  bool DefaultHasMessage() {
    return message_count_ > 0;
  }

  bool AddMessage() {
    ++message_count_;
  }

  bool would_have_blocked() { return would_have_blocked_; }
  void set_would_have_blocked(bool would_have_blocked) {
    would_have_blocked_ = would_have_blocked;
  }

  unsigned int message_count() { return message_count_; }
  void set_message_count(unsigned int count) { message_count_ = count; }
 private:
  bool would_have_blocked_;
  unsigned int message_count_;
};


}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SERVICE_CROSS_MOCKS_H__
