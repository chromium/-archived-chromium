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


// Tests for the command parser.

#include "tests/common/win/testing_common.h"
#include "command_buffer/service/cross/cmd_parser.h"
#include "command_buffer/service/cross/mocks.h"

namespace o3d {
namespace command_buffer {

using testing::Return;
using testing::Mock;
using testing::Truly;
using testing::Sequence;
using testing::_;

// Test fixture for CommandParser test - Creates a mock AsyncAPIInterface, and
// a fixed size memory buffer. Also provides a simple API to create a
// CommandParser.
class CommandParserTest : public testing::Test {
 protected:
  virtual void SetUp() {
    api_mock_.reset(new AsyncAPIMock);
    buffer_entry_count_ = 20;
    buffer_.reset(new CommandBufferEntry[buffer_entry_count_]);
  }
  virtual void TearDown() {}

  // Adds a DoCommand expectation in the mock.
  void AddDoCommandExpect(BufferSyncInterface::ParseError _return,
                          unsigned int command,
                          unsigned int arg_count,
                          CommandBufferEntry *args) {
    EXPECT_CALL(*api_mock(), DoCommand(command, arg_count,
        Truly(AsyncAPIMock::IsArgs(arg_count, args))))
        .InSequence(sequence_)
        .WillOnce(Return(_return));
  }

  // Creates a parser, with a buffer of the specified size (in entries).
  CommandParser *MakeParser(unsigned int entry_count) {
    size_t shm_size = buffer_entry_count_ *
                      sizeof(CommandBufferEntry);  // NOLINT
    size_t command_buffer_size = entry_count *
                                 sizeof(CommandBufferEntry);  // NOLINT
    DCHECK_LE(command_buffer_size, shm_size);
    return new CommandParser(buffer(),
                             shm_size,
                             0,
                             command_buffer_size,
                             0,
                             api_mock());
  }

  unsigned int buffer_entry_count() { return 20; }
  AsyncAPIMock *api_mock() { return api_mock_.get(); }
  CommandBufferEntry *buffer() { return buffer_.get(); }
 private:
  unsigned int buffer_entry_count_;
  scoped_ptr<AsyncAPIMock> api_mock_;
  scoped_array<CommandBufferEntry> buffer_;
  Sequence sequence_;
};

// Tests initialization conditions.
TEST_F(CommandParserTest, TestInit) {
  scoped_ptr<CommandParser> parser(MakeParser(10));
  EXPECT_EQ(0, parser->get());
  EXPECT_EQ(0, parser->put());
  EXPECT_TRUE(parser->IsEmpty());
}

// Tests simple commands.
TEST_F(CommandParserTest, TestSimple) {
  scoped_ptr<CommandParser> parser(MakeParser(10));
  CommandBufferOffset put = parser->put();
  CommandHeader header;

  // add a single command, no args
  header.size = 1;
  header.command = 123;
  buffer()[put++].value_header = header;

  parser->set_put(put);
  EXPECT_EQ(put, parser->put());

  AddDoCommandExpect(BufferSyncInterface::PARSE_NO_ERROR, 123, 0, NULL);
  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, parser->ProcessCommand());
  EXPECT_EQ(put, parser->get());
  Mock::VerifyAndClearExpectations(api_mock());

  // add a single command, 2 args
  header.size = 3;
  header.command = 456;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 2134;
  buffer()[put++].value_float = 1.f;

  parser->set_put(put);
  EXPECT_EQ(put, parser->put());

  CommandBufferEntry param_array[2];
  param_array[0].value_int32 = 2134;
  param_array[1].value_float = 1.f;
  AddDoCommandExpect(BufferSyncInterface::PARSE_NO_ERROR, 456, 2, param_array);
  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, parser->ProcessCommand());
  EXPECT_EQ(put, parser->get());
  Mock::VerifyAndClearExpectations(api_mock());
}

// Tests having multiple commands in the buffer.
TEST_F(CommandParserTest, TestMultipleCommands) {
  scoped_ptr<CommandParser> parser(MakeParser(10));
  CommandBufferOffset put = parser->put();
  CommandHeader header;

  // add 2 commands, test with single ProcessCommand()
  header.size = 2;
  header.command = 789;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 5151;

  CommandBufferOffset put_cmd2 = put;
  header.size = 2;
  header.command = 2121;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 3434;

  parser->set_put(put);
  EXPECT_EQ(put, parser->put());

  CommandBufferEntry param_array[2];
  param_array[0].value_int32 = 5151;
  AddDoCommandExpect(BufferSyncInterface::PARSE_NO_ERROR, 789, 1, param_array);
  param_array[1].value_int32 = 3434;
  AddDoCommandExpect(BufferSyncInterface::PARSE_NO_ERROR, 2121, 1,
                     param_array+1);

  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, parser->ProcessCommand());
  EXPECT_EQ(put_cmd2, parser->get());
  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, parser->ProcessCommand());
  EXPECT_EQ(put, parser->get());
  Mock::VerifyAndClearExpectations(api_mock());

  // add 2 commands again, test with ProcessAllCommands()
  header.size = 2;
  header.command = 4545;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 5656;

  header.size = 2;
  header.command = 6767;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 7878;

  parser->set_put(put);
  EXPECT_EQ(put, parser->put());

  param_array[0].value_int32 = 5656;
  AddDoCommandExpect(BufferSyncInterface::PARSE_NO_ERROR, 4545, 1, param_array);
  param_array[1].value_int32 = 7878;
  AddDoCommandExpect(BufferSyncInterface::PARSE_NO_ERROR, 6767, 1,
                     param_array+1);

  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, parser->ProcessAllCommands());
  EXPECT_EQ(put, parser->get());
  Mock::VerifyAndClearExpectations(api_mock());
}

// Tests that the parser will wrap correctly at the end of the buffer.
TEST_F(CommandParserTest, TestWrap) {
  scoped_ptr<CommandParser> parser(MakeParser(5));
  CommandBufferOffset put = parser->put();
  CommandHeader header;

  // add 3 commands with no args (1 word each)
  for (unsigned int i = 0; i < 3; ++i) {
    header.size = 1;
    header.command = i;
    buffer()[put++].value_header = header;
    AddDoCommandExpect(BufferSyncInterface::PARSE_NO_ERROR, i, 0, NULL);
  }
  parser->set_put(put);
  EXPECT_EQ(put, parser->put());
  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, parser->ProcessAllCommands());
  EXPECT_EQ(put, parser->get());
  Mock::VerifyAndClearExpectations(api_mock());

  // add 1 command with 1 arg (2 words). That should put us at the end of the
  // buffer.
  header.size = 2;
  header.command = 3;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 5;
  CommandBufferEntry param;
  param.value_int32 = 5;
  AddDoCommandExpect(BufferSyncInterface::PARSE_NO_ERROR, 3, 1, &param);

  DCHECK_EQ(5, put);
  put = 0;
  parser->set_put(put);
  EXPECT_EQ(put, parser->put());
  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, parser->ProcessAllCommands());
  EXPECT_EQ(put, parser->get());
  Mock::VerifyAndClearExpectations(api_mock());

  // add 1 command with 1 arg (2 words).
  header.size = 2;
  header.command = 4;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 6;
  param.value_int32 = 6;
  AddDoCommandExpect(BufferSyncInterface::PARSE_NO_ERROR, 4, 1, &param);
  parser->set_put(put);
  EXPECT_EQ(put, parser->put());
  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, parser->ProcessAllCommands());
  EXPECT_EQ(put, parser->get());
  Mock::VerifyAndClearExpectations(api_mock());
}

// Tests error conditions.
TEST_F(CommandParserTest, TestError) {
  scoped_ptr<CommandParser> parser(MakeParser(5));
  CommandBufferOffset put = parser->put();
  CommandHeader header;

  // Generate a command with size 0.
  header.size = 0;
  header.command = 3;
  buffer()[put++].value_header = header;

  parser->set_put(put);
  EXPECT_EQ(put, parser->put());
  EXPECT_EQ(BufferSyncInterface::PARSE_INVALID_SIZE,
            parser->ProcessAllCommands());
  // check that no DoCommand call was made.
  Mock::VerifyAndClearExpectations(api_mock());

  parser.reset(MakeParser(5));
  put = parser->put();

  // Generate a command with size 6, extends beyond the end of the buffer.
  header.size = 6;
  header.command = 3;
  buffer()[put++].value_header = header;

  parser->set_put(put);
  EXPECT_EQ(put, parser->put());
  EXPECT_EQ(BufferSyncInterface::PARSE_OUT_OF_BOUNDS,
            parser->ProcessAllCommands());
  // check that no DoCommand call was made.
  Mock::VerifyAndClearExpectations(api_mock());

  parser.reset(MakeParser(5));
  put = parser->put();

  // Generates 2 commands.
  header.size = 1;
  header.command = 3;
  buffer()[put++].value_header = header;
  CommandBufferOffset put_post_fail = put;
  header.size = 1;
  header.command = 4;
  buffer()[put++].value_header = header;

  parser->set_put(put);
  EXPECT_EQ(put, parser->put());
  // have the first command fail to parse.
  AddDoCommandExpect(BufferSyncInterface::PARSE_UNKNOWN_COMMAND, 3, 0, NULL);
  EXPECT_EQ(BufferSyncInterface::PARSE_UNKNOWN_COMMAND,
            parser->ProcessAllCommands());
  // check that only one command was executed, and that get reflects that
  // correctly.
  EXPECT_EQ(put_post_fail, parser->get());
  Mock::VerifyAndClearExpectations(api_mock());
  // make the second one succeed, and check that the parser recovered fine.
  AddDoCommandExpect(BufferSyncInterface::PARSE_NO_ERROR, 4, 0, NULL);
  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, parser->ProcessAllCommands());
  EXPECT_EQ(put, parser->get());
  Mock::VerifyAndClearExpectations(api_mock());
}

}  // namespace command_buffer
}  // namespace o3d
