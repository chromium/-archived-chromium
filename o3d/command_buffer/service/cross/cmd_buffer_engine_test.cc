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


// Tests for the Command Buffer Engine.

#include "tests/common/win/testing_common.h"
#include "command_buffer/service/cross/cmd_buffer_engine.h"
#include "command_buffer/service/cross/mocks.h"

namespace o3d {
namespace command_buffer {

using testing::AnyNumber;
using testing::Return;
using testing::Mock;
using testing::Truly;
using testing::Sequence;
using testing::_;

// Test fixture for CommandBufferEngine test - Creates a CommandBufferEngine,
// using a mock AsyncAPIInterface.
class CommandBufferEngineTest : public testing::Test {
 protected:
  CommandBufferEngineTest()
      : shm_(kRPCInvalidHandle),
        shm_id_(BufferSyncInterface::kInvalidSharedMemoryId) {}

  virtual void SetUp() {
    api_mock_.reset(new AsyncAPIMock);
    engine_.reset(new CommandBufferEngine(api_mock_.get()));
    process_mock_.reset(new RPCProcessMock());
    engine_->set_process_interface(process_mock_.get());
  }
  virtual void TearDown() {
  }

  // Creates a shared memory buffer for the command buffer, and pass it to the
  // engine.
  CommandBufferEntry *InitCommandBuffer(size_t entries, unsigned int start) {
    CHECK(shm_ == kRPCInvalidHandle);
    CHECK(shm_id_ == BufferSyncInterface::kInvalidSharedMemoryId);
    const size_t kShmSize = entries * sizeof(CommandBufferEntry);  // NOLINT
    shm_ = CreateShm(kShmSize);
    EXPECT_NE(kRPCInvalidHandle, shm_);
    if (kRPCInvalidHandle == shm_) return NULL;
    shm_id_ = engine()->RegisterSharedMemory(shm_, kShmSize);
    EXPECT_NE(BufferSyncInterface::kInvalidSharedMemoryId, shm_id_);
    if (shm_id_ == BufferSyncInterface::kInvalidSharedMemoryId) {
      DestroyShm(shm_);
      shm_ = kRPCInvalidHandle;
      return NULL;
    }
    engine()->SetCommandBuffer(shm_id_, 0, kShmSize, start);
    return static_cast<CommandBufferEntry *>(
        engine()->GetSharedMemoryAddress(shm_id_));
  }

  // Destroys the command buffer.
  void DestroyCommandBuffer() {
    engine()->UnregisterSharedMemory(shm_id_);
    shm_id_ = BufferSyncInterface::kInvalidSharedMemoryId;
    DestroyShm(shm_);
    shm_ = kRPCInvalidHandle;
  }

  // Adds a command to the buffer, while adding it as an expected call on the
  // API mock.
  unsigned int AddCommandWithExpect(CommandBufferEntry *buffer,
                                    BufferSyncInterface::ParseError _return,
                                    unsigned int command,
                                    unsigned int arg_count,
                                    CommandBufferEntry *args) {
    unsigned int put = 0;
    CommandHeader header;
    header.size = arg_count + 1;
    header.command = command;
    buffer[put++].value_header = header;
    for (unsigned int i = 0; i < arg_count; ++i) {
      buffer[put++].value_uint32 = args[i].value_uint32;
    }
    EXPECT_CALL(*api_mock(), DoCommand(command, arg_count,
        Truly(AsyncAPIMock::IsArgs(arg_count, args))))
        .InSequence(sequence_)
        .WillOnce(Return(_return));
    return put;
  }

  CommandBufferEngine *engine() { return engine_.get(); }
  RPCProcessMock *process_mock() { return process_mock_.get(); }
  AsyncAPIMock *api_mock() { return api_mock_.get(); }
 private:
  scoped_ptr<AsyncAPIMock> api_mock_;
  scoped_ptr<CommandBufferEngine> engine_;
  scoped_ptr<RPCProcessMock> process_mock_;
  // handles for the command buffer.
  RPCShmHandle shm_;
  unsigned int shm_id_;
  Sequence sequence_;
};

// Tests the initialization/termination sequence, checking that it sets the
// engine in the correct states.
TEST_F(CommandBufferEngineTest, TestInitialization) {
  // Check initial state
  EXPECT_TRUE(engine()->rpc_impl() != NULL);
  EXPECT_TRUE(engine()->parser() == NULL);
  EXPECT_EQ(BufferSyncInterface::NOT_CONNECTED, engine()->GetStatus());
  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, engine()->GetParseError());
  EXPECT_EQ(-1, engine()->Get());
  EXPECT_EQ(0, engine()->GetToken());

  engine()->InitConnection();
  EXPECT_EQ(BufferSyncInterface::NO_BUFFER, engine()->GetStatus());
  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, engine()->GetParseError());
  EXPECT_EQ(-1, engine()->Get());

  CommandBufferEntry *entries = InitCommandBuffer(25, 5);
  ASSERT_TRUE(entries != NULL);

  EXPECT_EQ(BufferSyncInterface::PARSING, engine()->GetStatus());
  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, engine()->GetParseError());
  EXPECT_EQ(5, engine()->Get());
  EXPECT_TRUE(engine()->parser() != NULL);

  engine()->set_token(5678);
  EXPECT_EQ(5678, engine()->GetToken());

  engine()->CloseConnection();
  DestroyCommandBuffer();

  EXPECT_EQ(BufferSyncInterface::NOT_CONNECTED, engine()->GetStatus());
  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, engine()->GetParseError());
  EXPECT_EQ(-1, engine()->Get());
  EXPECT_TRUE(engine()->parser() == NULL);
}

// Checks that shared memory registration works.
TEST_F(CommandBufferEngineTest, TestSharedMemoryRegistration) {
  // Create and register a first shared memory buffer.
  const size_t kShmSize1 = 10;
  RPCShmHandle shm1 = CreateShm(kShmSize1);
  ASSERT_NE(kRPCInvalidHandle, shm1);
  unsigned int shm_id1 = engine()->RegisterSharedMemory(shm1, kShmSize1);
  EXPECT_NE(BufferSyncInterface::kInvalidSharedMemoryId, shm_id1);
  EXPECT_TRUE(engine()->GetSharedMemoryAddress(shm_id1) != NULL);
  EXPECT_EQ(kShmSize1, engine()->GetSharedMemorySize(shm_id1));

  // Create and register a second shared memory buffer, check that it has a
  // different memory location than the first one.
  const size_t kShmSize2 = 25;
  RPCShmHandle shm2 = CreateShm(kShmSize2);
  ASSERT_NE(kRPCInvalidHandle, shm2);
  unsigned int shm_id2 = engine()->RegisterSharedMemory(shm2, kShmSize2);
  EXPECT_NE(BufferSyncInterface::kInvalidSharedMemoryId, shm_id2);
  EXPECT_TRUE(engine()->GetSharedMemoryAddress(shm_id2) != NULL);
  EXPECT_EQ(kShmSize2, engine()->GetSharedMemorySize(shm_id2));
  EXPECT_NE(shm_id1, shm_id2);
  EXPECT_NE(engine()->GetSharedMemoryAddress(shm_id1),
            engine()->GetSharedMemoryAddress(shm_id2));

  // Create and register a third shared memory buffer, check that it has a
  // different memory location than the first and second ones.
  const size_t kShmSize3 = 33;
  RPCShmHandle shm3 = CreateShm(kShmSize3);
  ASSERT_NE(kRPCInvalidHandle, shm3);
  unsigned int shm_id3 = engine()->RegisterSharedMemory(shm3, kShmSize3);
  EXPECT_NE(BufferSyncInterface::kInvalidSharedMemoryId, shm_id3);
  EXPECT_TRUE(engine()->GetSharedMemoryAddress(shm_id3) != NULL);
  EXPECT_EQ(kShmSize3, engine()->GetSharedMemorySize(shm_id3));
  EXPECT_NE(shm_id1, shm_id3);
  EXPECT_NE(shm_id2, shm_id3);
  EXPECT_NE(engine()->GetSharedMemoryAddress(shm_id1),
            engine()->GetSharedMemoryAddress(shm_id3));
  EXPECT_NE(engine()->GetSharedMemoryAddress(shm_id2),
            engine()->GetSharedMemoryAddress(shm_id3));

  engine()->UnregisterSharedMemory(shm_id1);
  EXPECT_EQ(NULL, engine()->GetSharedMemoryAddress(shm_id1));
  EXPECT_EQ(0UL, engine()->GetSharedMemorySize(shm_id1));
  DestroyShm(shm1);

  engine()->UnregisterSharedMemory(shm_id2);
  EXPECT_EQ(NULL, engine()->GetSharedMemoryAddress(shm_id2));
  EXPECT_EQ(0UL, engine()->GetSharedMemorySize(shm_id2));
  DestroyShm(shm2);

  engine()->UnregisterSharedMemory(shm_id3);
  EXPECT_EQ(NULL, engine()->GetSharedMemoryAddress(shm_id2));
  EXPECT_EQ(0UL, engine()->GetSharedMemorySize(shm_id2));
  DestroyShm(shm3);
}

// Checks that commands in the buffer are properly executed, and that the
// status/error stay valid.
TEST_F(CommandBufferEngineTest, TestCommandProcessing) {
  engine()->InitConnection();
  CommandBufferEntry *entries = InitCommandBuffer(10, 0);
  ASSERT_TRUE(entries != NULL);

  CommandBufferOffset get = engine()->Get();
  CommandBufferOffset put = get;

  // Create a command buffer with 3 commands
  put += AddCommandWithExpect(entries + put,
                              BufferSyncInterface::PARSE_NO_ERROR,
                              0,
                              0,
                              NULL);

  CommandBufferEntry args1[2];
  args1[0].value_uint32 = 3;
  args1[1].value_float = 4.f;
  put += AddCommandWithExpect(entries + put,
                              BufferSyncInterface::PARSE_NO_ERROR,
                              1,
                              2,
                              args1);

  CommandBufferEntry args2[2];
  args2[0].value_uint32 = 5;
  args2[1].value_float = 6.f;
  put += AddCommandWithExpect(entries + put,
                              BufferSyncInterface::PARSE_NO_ERROR,
                              2,
                              2,
                              args2);

  engine()->Put(put);
  while (get != put) {
    // Check that the parsing progresses, and that no error occurs.
    CommandBufferOffset new_get = engine()->WaitGetChanges(get);
    EXPECT_NE(get, new_get);
    ASSERT_EQ(BufferSyncInterface::PARSING, engine()->GetStatus());
    EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, engine()->GetParseError());
    EXPECT_EQ(new_get, engine()->Get());
    get = new_get;
  }
  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock());

  engine()->CloseConnection();
  DestroyCommandBuffer();
}

// Checks that commands in the buffer are properly executed when wrapping the
// buffer, and that the status/error stay valid.
TEST_F(CommandBufferEngineTest, TestCommandWrapping) {
  engine()->InitConnection();
  CommandBufferEntry *entries = InitCommandBuffer(10, 6);
  ASSERT_TRUE(entries != NULL);

  CommandBufferOffset get = engine()->Get();
  CommandBufferOffset put = get;

  // Create a command buffer with 3 commands
  put += AddCommandWithExpect(entries + put,
                              BufferSyncInterface::PARSE_NO_ERROR,
                              0,
                              0,
                              NULL);

  CommandBufferEntry args1[2];
  args1[0].value_uint32 = 3;
  args1[1].value_float = 4.f;
  put += AddCommandWithExpect(entries + put,
                              BufferSyncInterface::PARSE_NO_ERROR,
                              1,
                              2,
                              args1);
  DCHECK_EQ(10, put);
  put = 0;

  CommandBufferEntry args2[2];
  args2[0].value_uint32 = 5;
  args2[1].value_float = 6.f;
  put += AddCommandWithExpect(entries + put,
                              BufferSyncInterface::PARSE_NO_ERROR,
                              2,
                              2,
                              args2);

  engine()->Put(put);
  while (get != put) {
    // Check that the parsing progresses, and that no error occurs.
    CommandBufferOffset new_get = engine()->WaitGetChanges(get);
    EXPECT_NE(get, new_get);
    ASSERT_EQ(BufferSyncInterface::PARSING, engine()->GetStatus());
    EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, engine()->GetParseError());
    EXPECT_EQ(new_get, engine()->Get());
    get = new_get;
  }
  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock());

  engine()->CloseConnection();
  DestroyCommandBuffer();
}

// Checks that commands in the buffer are properly executed when we change the
// buffer, and when we close the connection.
TEST_F(CommandBufferEngineTest, TestSetBufferAndClose) {
  engine()->InitConnection();
  CommandBufferEntry *entries = InitCommandBuffer(10, 0);
  ASSERT_TRUE(entries != NULL);

  CommandBufferOffset get = engine()->Get();
  CommandBufferOffset put = get;

  // Create a command buffer with 3 commands
  put += AddCommandWithExpect(entries + put,
                              BufferSyncInterface::PARSE_NO_ERROR,
                              0,
                              0,
                              NULL);

  CommandBufferEntry args1[2];
  args1[0].value_uint32 = 3;
  args1[1].value_float = 4.f;
  put += AddCommandWithExpect(entries + put,
                              BufferSyncInterface::PARSE_NO_ERROR,
                              1,
                              2,
                              args1);

  CommandBufferEntry args2[2];
  args2[0].value_uint32 = 5;
  args2[1].value_float = 6.f;
  put += AddCommandWithExpect(entries + put,
                              BufferSyncInterface::PARSE_NO_ERROR,
                              2,
                              2,
                              args2);

  engine()->Put(put);

  // Setup a new buffer.
  const size_t kShmSize = 10 * sizeof(CommandBufferEntry);  // NOLINT
  RPCShmHandle shm = CreateShm(kShmSize);
  ASSERT_NE(kRPCInvalidHandle, shm);
  unsigned int shm_id = engine()->RegisterSharedMemory(shm, kShmSize);
  ASSERT_NE(BufferSyncInterface::kInvalidSharedMemoryId, shm_id);
  CommandBufferEntry *entries2 = static_cast<CommandBufferEntry *>(
      engine()->GetSharedMemoryAddress(shm_id));
  engine()->SetCommandBuffer(shm_id, 0, kShmSize, 0);
  EXPECT_EQ(BufferSyncInterface::PARSING, engine()->GetStatus());
  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, engine()->GetParseError());
  EXPECT_EQ(0, engine()->Get());

  // Destroy the old command buffer.
  DestroyCommandBuffer();

  get = engine()->Get();
  put = get;
  put += AddCommandWithExpect(entries2 + put,
                              BufferSyncInterface::PARSE_NO_ERROR,
                              1,
                              2,
                              args1);

  engine()->Put(put);

  engine()->CloseConnection();
  // Check that all the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock());

  engine()->UnregisterSharedMemory(shm_id);
  DestroyShm(shm);
}

// Checks that commands in the buffer are properly executed, even if they
// generate a recoverable error. Check that the error status is properly set,
// and reset when queried.
TEST_F(CommandBufferEngineTest, TestRecoverableError) {
  engine()->InitConnection();
  CommandBufferEntry *entries = InitCommandBuffer(10, 0);
  ASSERT_TRUE(entries != NULL);

  CommandBufferOffset get = engine()->Get();
  CommandBufferOffset put = get;

  // Create a command buffer with 3 commands, 2 of them generating errors
  put += AddCommandWithExpect(entries + put,
                              BufferSyncInterface::PARSE_NO_ERROR,
                              0,
                              0,
                              NULL);

  CommandBufferEntry args1[2];
  args1[0].value_uint32 = 3;
  args1[1].value_float = 4.f;
  put += AddCommandWithExpect(entries + put,
                              BufferSyncInterface::PARSE_INVALID_ARGUMENTS,
                              1,
                              2,
                              args1);

  CommandBufferEntry args2[2];
  args2[0].value_uint32 = 5;
  args2[1].value_float = 6.f;
  put += AddCommandWithExpect(entries + put,
                              BufferSyncInterface::PARSE_UNKNOWN_COMMAND,
                              2,
                              2,
                              args2);

  engine()->Put(put);
  while (get != put) {
    // Check that the parsing progresses, and that no error occurs.
    CommandBufferOffset new_get = engine()->WaitGetChanges(get);
    EXPECT_NE(get, new_get);
    ASSERT_EQ(BufferSyncInterface::PARSING, engine()->GetStatus());
    EXPECT_EQ(new_get, engine()->Get());
    get = new_get;
  }
  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock());

  // Check that the error status was set to the first error.
  EXPECT_EQ(BufferSyncInterface::PARSE_INVALID_ARGUMENTS,
            engine()->GetParseError());
  // Check that the error status was reset after the query.
  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, engine()->GetParseError());

  engine()->CloseConnection();
  DestroyCommandBuffer();
}

// Checks that commands in the buffer are properly executed up to the point
// where a parsing error happened. Check that at that point the status and
// error are properly set.
TEST_F(CommandBufferEngineTest, TestNonRecoverableError) {
  engine()->InitConnection();
  // Create a buffer with 6 entries, starting at entry 1, but allocate enough
  // memory so that we can add commands that cross over the limit.
  const size_t kShmSize = 10 * sizeof(CommandBufferEntry);  // NOLINT
  RPCShmHandle shm = CreateShm(kShmSize);
  ASSERT_NE(kRPCInvalidHandle, shm);
  unsigned int shm_id = engine()->RegisterSharedMemory(shm, kShmSize);
  ASSERT_NE(BufferSyncInterface::kInvalidSharedMemoryId, shm_id);
  CommandBufferEntry *entries = static_cast<CommandBufferEntry *>(
      engine()->GetSharedMemoryAddress(shm_id));
  ASSERT_TRUE(entries != NULL);
  engine()->SetCommandBuffer(shm_id, 0, 6 * sizeof(CommandBufferEntry), 1);

  CommandBufferOffset get = engine()->Get();
  CommandBufferOffset put = get;

  // Create a command buffer with 3 commands, the last one overlapping the end
  // of the buffer.
  put += AddCommandWithExpect(entries + put,
                              BufferSyncInterface::PARSE_NO_ERROR,
                              0,
                              0,
                              NULL);

  CommandBufferEntry args1[2];
  args1[0].value_uint32 = 3;
  args1[1].value_float = 4.f;
  put += AddCommandWithExpect(entries + put,
                              BufferSyncInterface::PARSE_NO_ERROR,
                              1,
                              2,
                              args1);

  CommandBufferOffset fail_get = put;
  CommandHeader header;
  header.size = 3;
  header.command = 4;
  entries[put++].value_header = header;
  entries[put++].value_uint32 = 5;
  entries[put++].value_uint32 = 6;

  // we should be beyond the end of the buffer now.
  DCHECK_LT(6, put);
  put = 0;

  engine()->Put(put);
  while (get != put) {
    // When the parsing stop progressing, break.
    CommandBufferOffset new_get = engine()->WaitGetChanges(get);
    if (new_get == get) {
      EXPECT_EQ(new_get, engine()->Get());
      break;
    }
    get = new_get;
  }
  // We should be in an error case now.
  EXPECT_EQ(BufferSyncInterface::PARSE_ERROR, engine()->GetStatus());
  // Check that the error status was set to the first error.
  EXPECT_EQ(BufferSyncInterface::PARSE_OUT_OF_BOUNDS,
            engine()->GetParseError());
  // Check that the error status was reset after the query.
  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, engine()->GetParseError());

  // Check that the valid commands did happen.
  Mock::VerifyAndClearExpectations(api_mock());

  engine()->CloseConnection();
  engine()->UnregisterSharedMemory(shm_id);
  DestroyShm(shm);
}

// Checks that HasWork() and DoWork() have the correct semantics. If there is
// work to do, DoWork should never block.
TEST_F(CommandBufferEngineTest, TestDoWork) {
  engine()->InitConnection();
  CommandBufferEntry *entries = InitCommandBuffer(10, 0);
  ASSERT_TRUE(entries != NULL);

  CommandBufferOffset get = engine()->Get();
  CommandBufferOffset put = get;

  // Test that if we have no message and no command we will block.
  process_mock()->Reset();
  EXPECT_CALL(*process_mock(), HasMessage()).Times(AnyNumber());
  EXPECT_FALSE(engine()->HasWork());
  EXPECT_CALL(*process_mock(), ProcessMessage());
  EXPECT_TRUE(engine()->DoWork());

  EXPECT_TRUE(process_mock()->would_have_blocked());
  Mock::VerifyAndClearExpectations(process_mock());

  // Tests that messages get processed without blocking.
  process_mock()->Reset();
  EXPECT_CALL(*process_mock(), HasMessage()).Times(AnyNumber());
  process_mock()->set_message_count(3);
  EXPECT_TRUE(engine()->HasWork());

  EXPECT_CALL(*process_mock(), ProcessMessage()).Times(3);
  while (engine()->HasWork()) {
    EXPECT_TRUE(engine()->DoWork());
  }
  EXPECT_EQ(0, process_mock()->message_count());
  EXPECT_FALSE(process_mock()->would_have_blocked());
  Mock::VerifyAndClearExpectations(process_mock());

  // Test that if we have commands, we will process them without blocking.
  // Create a command buffer with 3 commands
  process_mock()->Reset();
  EXPECT_CALL(*process_mock(), HasMessage()).Times(AnyNumber());
  put += AddCommandWithExpect(entries + put,
                              BufferSyncInterface::PARSE_NO_ERROR,
                              0,
                              0,
                              NULL);

  CommandBufferEntry args1[2];
  args1[0].value_uint32 = 3;
  args1[1].value_float = 4.f;
  put += AddCommandWithExpect(entries + put,
                              BufferSyncInterface::PARSE_NO_ERROR,
                              1,
                              2,
                              args1);

  CommandBufferEntry args2[2];
  args2[0].value_uint32 = 5;
  args2[1].value_float = 6.f;
  put += AddCommandWithExpect(entries + put,
                              BufferSyncInterface::PARSE_NO_ERROR,
                              2,
                              2,
                              args2);

  EXPECT_FALSE(engine()->HasWork());  // No work yet, until we change put

  engine()->Put(put);
  EXPECT_TRUE(engine()->HasWork());

  EXPECT_CALL(*process_mock(), ProcessMessage()).Times(0);
  while (engine()->HasWork()) {
    EXPECT_TRUE(engine()->DoWork());
  }
  EXPECT_FALSE(process_mock()->would_have_blocked());
  get = engine()->Get();
  EXPECT_EQ(put, get);  // once we're done, we should have executed everything.
  ASSERT_EQ(BufferSyncInterface::PARSING, engine()->GetStatus());
  EXPECT_EQ(BufferSyncInterface::PARSE_NO_ERROR, engine()->GetParseError());
  Mock::VerifyAndClearExpectations(process_mock());
  Mock::VerifyAndClearExpectations(api_mock());

  // Test that the engine stops if we send it a "kill" message.
  process_mock()->Reset();
  EXPECT_CALL(*process_mock(), HasMessage()).Times(AnyNumber());
  process_mock()->set_message_count(1);
  EXPECT_TRUE(engine()->HasWork());

  EXPECT_CALL(*process_mock(), ProcessMessage()).WillOnce(Return(false));
  EXPECT_FALSE(engine()->DoWork());
  Mock::VerifyAndClearExpectations(process_mock());

  engine()->CloseConnection();
  DestroyCommandBuffer();
}

}  // namespace command_buffer
}  // namespace o3d
