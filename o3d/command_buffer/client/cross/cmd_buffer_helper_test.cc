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


// Tests for the Command Buffer Helper.

#include "tests/common/win/testing_common.h"
#include "command_buffer/client/cross/cmd_buffer_helper.h"
#include "command_buffer/service/cross/cmd_buffer_engine.h"
#include "command_buffer/service/cross/mocks.h"

namespace o3d {
namespace command_buffer {

using testing::Return;
using testing::Mock;
using testing::Truly;
using testing::Sequence;
using testing::DoAll;
using testing::Invoke;
using testing::_;

// Test fixture for CommandBufferHelper test - Creates a CommandBufferHelper,
// using a CommandBufferEngine with a mock AsyncAPIInterface for its interface
// (calling it directly, not through the RPC mechanism).
class CommandBufferHelperTest : public testing::Test {
 protected:
  virtual void SetUp() {
    api_mock_.reset(new AsyncAPIMock);
    // ignore noops in the mock - we don't want to inspect the internals of the
    // helper.
    EXPECT_CALL(*api_mock_, DoCommand(0, 0, _))
        .WillRepeatedly(Return(BufferSyncInterface::PARSE_NO_ERROR));
    engine_.reset(new CommandBufferEngine(api_mock_.get()));
    api_mock_->set_engine(engine_.get());

    engine_->InitConnection();
    helper_.reset(new CommandBufferHelper(engine_.get()));
    helper_->Init(10);
  }

  virtual void TearDown() {
    helper_.release();
    engine_->CloseConnection();
  }

  // Adds a command to the buffer through the helper, while adding it as an
  // expected call on the API mock.
  void AddCommandWithExpect(BufferSyncInterface::ParseError _return,
                            unsigned int command,
                            unsigned int arg_count,
                            CommandBufferEntry *args) {
    helper_->AddCommand(command, arg_count, args);
    EXPECT_CALL(*api_mock(), DoCommand(command, arg_count,
        Truly(AsyncAPIMock::IsArgs(arg_count, args))))
        .InSequence(sequence_)
        .WillOnce(Return(_return));
  }

  // Checks that the buffer from put to put+size is free in the parser.
  void CheckFreeSpace(CommandBufferOffset put, unsigned int size) {
    CommandBufferOffset parser_put = engine()->parser()->put();
    CommandBufferOffset parser_get = engine()->parser()->get();
    CommandBufferOffset limit = put + size;
    if (parser_get > parser_put) {
      // "busy" buffer wraps, so "free" buffer is between put (inclusive) and
      // get (exclusive).
      EXPECT_LE(parser_put, put);
      EXPECT_GT(parser_get, limit);
    } else {
      // "busy" buffer does not wrap, so the "free" buffer is the top side (from
      // put to the limit) and the bottom side (from 0 to get).
      if (put >= parser_put) {
        // we're on the top side, check we are below the limit.
        EXPECT_GE(10, limit);
      } else {
        // we're on the bottom side, check we are below get.
        EXPECT_GT(parser_get, limit);
      }
    }
  }

  CommandBufferHelper *helper() { return helper_.get(); }
  CommandBufferEngine *engine() { return engine_.get(); }
  AsyncAPIMock *api_mock() { return api_mock_.get(); }
  CommandBufferOffset get_helper_put() { return helper_->put_; }
 private:
  scoped_ptr<AsyncAPIMock> api_mock_;
  scoped_ptr<CommandBufferEngine> engine_;
  scoped_ptr<CommandBufferHelper> helper_;
  Sequence sequence_;
};

// Checks that commands in the buffer are properly executed, and that the
// status/error stay valid.
TEST_F(CommandBufferHelperTest, TestCommandProcessing) {
  // Check initial state of the engine - it should have been configured by the
  // helper.
  EXPECT_TRUE(engine()->rpc_impl() != NULL);
  EXPECT_TRUE(engine()->parser() != NULL);
  EXPECT_EQ(BufferSyncInterface::PARSING, engine()->GetStatus());
  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, engine()->GetParseError());
  EXPECT_EQ(0, engine()->Get());

  // Add 3 commands through the helper
  AddCommandWithExpect(BufferSyncInterface::PARSE_NO_ERROR, 1, 0, NULL);

  CommandBufferEntry args1[2];
  args1[0].value_uint32 = 3;
  args1[1].value_float = 4.f;
  AddCommandWithExpect(BufferSyncInterface::PARSE_NO_ERROR, 2, 2, args1);

  CommandBufferEntry args2[2];
  args2[0].value_uint32 = 5;
  args2[1].value_float = 6.f;
  AddCommandWithExpect(BufferSyncInterface::PARSE_NO_ERROR, 3, 2, args2);

  helper()->Flush();
  // Check that the engine has work to do now.
  EXPECT_FALSE(engine()->parser()->IsEmpty());

  // Wait until it's done.
  helper()->Finish();
  // Check that the engine has no more work to do.
  EXPECT_TRUE(engine()->parser()->IsEmpty());

  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock());

  // Check the error status.
  ASSERT_EQ(BufferSyncInterface::PARSING, engine()->GetStatus());
  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, engine()->GetParseError());
}

// Checks that commands in the buffer are properly executed when wrapping the
// buffer, and that the status/error stay valid.
TEST_F(CommandBufferHelperTest, TestCommandWrapping) {
  // Add 5 commands of size 3 through the helper to make sure we do wrap.
  CommandBufferEntry args1[2];
  args1[0].value_uint32 = 3;
  args1[1].value_float = 4.f;

  for (unsigned int i = 0; i < 5; ++i) {
    AddCommandWithExpect(BufferSyncInterface::PARSE_NO_ERROR, i+1, 2, args1);
  }

  helper()->Finish();
  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock());

  // Check the error status.
  ASSERT_EQ(BufferSyncInterface::PARSING, engine()->GetStatus());
  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, engine()->GetParseError());
}


// Checks that commands in the buffer are properly executed, even if they
// generate a recoverable error. Check that the error status is properly set,
// and reset when queried.
TEST_F(CommandBufferHelperTest, TestRecoverableError) {
  CommandBufferEntry args[2];
  args[0].value_uint32 = 3;
  args[1].value_float = 4.f;

  // Create a command buffer with 3 commands, 2 of them generating errors
  AddCommandWithExpect(BufferSyncInterface::PARSE_NO_ERROR, 1, 2, args);
  AddCommandWithExpect(BufferSyncInterface::PARSE_UNKNOWN_COMMAND, 2, 2, args);
  AddCommandWithExpect(BufferSyncInterface::PARSE_INVALID_ARGUMENTS, 3, 2,
                       args);

  helper()->Finish();
  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock());

  // Check that the error status was set to the first error.
  EXPECT_EQ(BufferSyncInterface::PARSE_UNKNOWN_COMMAND,
            engine()->GetParseError());
  // Check that the error status was reset after the query.
  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, engine()->GetParseError());

  engine()->CloseConnection();
}

// Checks that asking for available entries work, and that the parser
// effectively won't use that space.
TEST_F(CommandBufferHelperTest, TestAvailableEntries) {
  CommandBufferEntry args[2];
  args[0].value_uint32 = 3;
  args[1].value_float = 4.f;

  // Add 2 commands through the helper - 8 entries
  AddCommandWithExpect(BufferSyncInterface::PARSE_NO_ERROR, 1, 0, NULL);
  AddCommandWithExpect(BufferSyncInterface::PARSE_NO_ERROR, 2, 0, NULL);
  AddCommandWithExpect(BufferSyncInterface::PARSE_NO_ERROR, 3, 2, args);
  AddCommandWithExpect(BufferSyncInterface::PARSE_NO_ERROR, 4, 2, args);

  // Ask for 5 entries.
  helper()->WaitForAvailableEntries(5);

  CommandBufferOffset put = get_helper_put();
  CheckFreeSpace(put, 5);

  // Add more commands.
  AddCommandWithExpect(BufferSyncInterface::PARSE_NO_ERROR, 5, 2, args);

  // Wait until everything is done done.
  helper()->Finish();

  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock());

  // Check the error status.
  ASSERT_EQ(BufferSyncInterface::PARSING, engine()->GetStatus());
  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, engine()->GetParseError());
}

// Checks that the InsertToken/WaitForToken work.
TEST_F(CommandBufferHelperTest, TestToken) {
  CommandBufferEntry args[2];
  args[0].value_uint32 = 3;
  args[1].value_float = 4.f;

  // Add a first command.
  AddCommandWithExpect(BufferSyncInterface::PARSE_NO_ERROR, 3, 2, args);
  // keep track of the buffer position.
  CommandBufferOffset command1_put = get_helper_put();
  unsigned int token = helper()->InsertToken();

  EXPECT_CALL(*api_mock(), DoCommand(SET_TOKEN, 1, _))
      .WillOnce(DoAll(Invoke(api_mock(), &AsyncAPIMock::SetToken),
                      Return(BufferSyncInterface::PARSE_NO_ERROR)));
  // Add another command.
  AddCommandWithExpect(BufferSyncInterface::PARSE_NO_ERROR, 4, 2, args);
  helper()->WaitForToken(token);
  // check that the get pointer is beyond the first command.
  EXPECT_LE(command1_put, engine()->Get());
  helper()->Finish();

  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock());

  // Check the error status.
  ASSERT_EQ(BufferSyncInterface::PARSING, engine()->GetStatus());
  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, engine()->GetParseError());
}

}  // namespace command_buffer
}  // namespace o3d
