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


// Tests for the Command Buffer RPC glue.

#include "tests/common/win/testing_common.h"
#include "command_buffer/common/cross/mocks.h"
#include "command_buffer/service/cross/buffer_rpc.h"
#include "command_buffer/service/cross/mocks.h"

namespace o3d {
namespace command_buffer {

using testing::Return;

// Test fixture for BufferRPCImpl test. Creates a BufferSyncMock and a
// BufferRPCImpl that uses it.
class BufferRPCImplTest : public testing::Test {
 protected:
  virtual void SetUp() {
    buffer_sync_mock_.reset(new BufferSyncMock);
    buffer_rpc_impl_.reset(new BufferRPCImpl(buffer_sync_mock_.get()));
  }
  virtual void TearDown() {}

  BufferSyncMock &buffer_sync_mock() { return *buffer_sync_mock_.get(); }
  BufferRPCImpl *buffer_rpc_impl() { return buffer_rpc_impl_.get(); }
 private:
  scoped_ptr<BufferSyncMock> buffer_sync_mock_;
  scoped_ptr<BufferRPCImpl> buffer_rpc_impl_;
};

// Checks that the INIT_CONNECTION RPC is properly parsed.
TEST_F(BufferRPCImplTest, TestInitConnection) {
  EXPECT_CALL(buffer_sync_mock(), InitConnection());
  buffer_rpc_impl()->DoCall(BufferRPCImpl::INIT_CONNECTION, NULL, 0, NULL, 0);
}

// Checks that the CLOSE_CONNECTION RPC is properly parsed.
TEST_F(BufferRPCImplTest, TestCloseConnection) {
  EXPECT_CALL(buffer_sync_mock(), CloseConnection());
  buffer_rpc_impl()->DoCall(BufferRPCImpl::CLOSE_CONNECTION, NULL, 0, NULL, 0);
}

// Checks that the REGISTER_SHARED_MEMORY RPC is properly parsed and that the
// return value is properly forwarded.
TEST_F(BufferRPCImplTest, TestRegisterSharedMemory) {
  RPCShmHandle shm = reinterpret_cast<RPCShmHandle>(456);
  size_t size = 789;
  EXPECT_CALL(buffer_sync_mock(), RegisterSharedMemory(shm, size))
      .WillOnce(Return(1234));
  RPCHandle handles[1] = {shm};
  EXPECT_EQ(1234, buffer_rpc_impl()->DoCall(
      BufferRPCImpl::REGISTER_SHARED_MEMORY, &size, sizeof(size), handles, 1));
}

// Checks that the UNREGISTER_SHARED_MEMORY RPC is properly parsed.
TEST_F(BufferRPCImplTest, TestUnregisterSharedMemory) {
  unsigned int shm_id = 385;
  EXPECT_CALL(buffer_sync_mock(), UnregisterSharedMemory(shm_id));
  buffer_rpc_impl()->DoCall(BufferRPCImpl::UNREGISTER_SHARED_MEMORY, &shm_id,
                            sizeof(shm_id), NULL, 0);
}

// Checks that the SET_COMMAND_BUFFER RPC is properly parsed.
TEST_F(BufferRPCImplTest, TestSetCommandBuffer) {
  EXPECT_CALL(buffer_sync_mock(), SetCommandBuffer(93, 7878, 3434, 5151));
  BufferRPCImpl::SetCommandBufferStruct param;
  param.shm_id = 93;
  param.offset = 7878;
  param.size = 3434;
  param.start_get = 5151;
  buffer_rpc_impl()->DoCall(BufferRPCImpl::SET_COMMAND_BUFFER, &param,
                            sizeof(param), NULL, 0);
}

// Checks that the PUT RPC is properly parsed.
TEST_F(BufferRPCImplTest, TestPut) {
  CommandBufferOffset offset = 8765;
  EXPECT_CALL(buffer_sync_mock(), Put(offset));
  buffer_rpc_impl()->DoCall(BufferRPCImpl::PUT, &offset, sizeof(offset), NULL,
                            0);
}

// Checks that the GET RPC is properly parsed and that the return value is
// properly forwarded.
TEST_F(BufferRPCImplTest, TestGet) {
  EXPECT_CALL(buffer_sync_mock(), Get()).WillOnce(Return(9375));
  EXPECT_EQ(9375, buffer_rpc_impl()->DoCall(BufferRPCImpl::GET, NULL, 0, NULL,
                                            0));
}

// Checks that the GET_TOKEN RPC is properly parsed and that the return value is
// properly forwarded.
TEST_F(BufferRPCImplTest, TestGetToken) {
  EXPECT_CALL(buffer_sync_mock(), GetToken()).WillOnce(Return(1618));
  EXPECT_EQ(1618, buffer_rpc_impl()->DoCall(BufferRPCImpl::GET_TOKEN, NULL, 0,
                                            NULL, 0));
}

// Checks that the WAIT_GET_CHANGES RPC is properly parsed and that the return
// value is properly forwarded.
TEST_F(BufferRPCImplTest, TestWaitGetChanges) {
  CommandBufferOffset value = 339;
  EXPECT_CALL(buffer_sync_mock(), WaitGetChanges(value))
      .WillOnce(Return(16180));
  EXPECT_EQ(16180, buffer_rpc_impl()->DoCall(BufferRPCImpl::WAIT_GET_CHANGES,
                                             &value, sizeof(value), NULL, 0));
}

// Checks that the SIGNAL_GET_CHANGES RPC is properly parsed.
TEST_F(BufferRPCImplTest, TestSignalGetChanges) {
  EXPECT_CALL(buffer_sync_mock(), SignalGetChanges(34, 21));
  BufferRPCImpl::SignalGetChangesStruct param;
  param.current_value = 34;
  param.rpc_message_id = 21;
  buffer_rpc_impl()->DoCall(BufferRPCImpl::SIGNAL_GET_CHANGES, &param,
                            sizeof(param), NULL, 0);
}

// Checks that the GET_STATUS RPC is properly parsed and that the return value
// is properly forwarded.
TEST_F(BufferRPCImplTest, TestGetStatus) {
  EXPECT_CALL(buffer_sync_mock(), GetStatus())
      .WillOnce(Return(BufferSyncInterface::PARSE_ERROR));
  EXPECT_EQ(BufferSyncInterface::PARSE_ERROR,
            buffer_rpc_impl()->DoCall(BufferRPCImpl::GET_STATUS, NULL, 0, NULL,
                                      0));
}

// Checks that the GET_STATUS RPC is properly parsed and that the return value
// is properly forwarded.
TEST_F(BufferRPCImplTest, TestGetParseError) {
  EXPECT_CALL(buffer_sync_mock(), GetParseError())
      .WillOnce(Return(BufferSyncInterface::PARSE_OUT_OF_BOUNDS));
  EXPECT_EQ(BufferSyncInterface::PARSE_OUT_OF_BOUNDS,
            buffer_rpc_impl()->DoCall(BufferRPCImpl::GET_PARSE_ERROR, NULL, 0,
                                      NULL, 0));
}

}  // namespace command_buffer
}  // namespace o3d
