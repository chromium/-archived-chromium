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


// Tests for the Command Buffer RPC glue, client side (proxy).

#include "base/scoped_ptr.h"
#include "tests/common/win/testing_common.h"
#include "command_buffer/client/cross/buffer_sync_proxy.h"
#include "command_buffer/common/cross/mocks.h"
#include "command_buffer/service/cross/buffer_rpc.h"

namespace o3d {
namespace command_buffer {

using testing::Return;

// Test fixture for BufferSyncProxy test - Creates a BufferSyncProxy, using a
// mock RPCSendInterface.
class BufferSyncProxyTest : public testing::Test {
 protected:
  virtual void SetUp() {
    server_mock_.reset(new RPCSendInterfaceMock);
    proxy_.reset(new BufferSyncProxy(server_mock_.get()));
  }
  virtual void TearDown() {}

  RPCSendInterfaceMock *server_mock() { return server_mock_.get(); }
  BufferSyncProxy *proxy() { return proxy_.get(); }
 private:
  scoped_ptr<RPCSendInterfaceMock> server_mock_;
  scoped_ptr<BufferSyncProxy> proxy_;
};

// Tests the implementation of InitConnection, checking that it sends the
// correct message.
TEST_F(BufferSyncProxyTest, TestInitConnection) {
  RPCSendInterfaceMock::SendCallExpect expect;
  expect._return = 0;
  expect.message_id = BufferRPCImpl::INIT_CONNECTION;
  expect.data = NULL;
  expect.size = 0;
  expect.handles = NULL;
  expect.handle_count = 0;
  server_mock()->AddSendCallExpect(expect);

  proxy()->InitConnection();
}

// Tests the implementation of CloseConnection, checking that it sends the
// correct message.
TEST_F(BufferSyncProxyTest, TestCloseConnection) {
  RPCSendInterfaceMock::SendCallExpect expect;
  expect._return = 0;
  expect.message_id = BufferRPCImpl::CLOSE_CONNECTION;
  expect.data = NULL;
  expect.size = 0;
  expect.handles = NULL;
  expect.handle_count = 0;
  server_mock()->AddSendCallExpect(expect);

  proxy()->CloseConnection();
}

// Tests the implementation of RegisterSharedMemory, checking that it sends the
// correct message and returns the correct value.
TEST_F(BufferSyncProxyTest, TestRegisterSharedMemory) {
  RPCShmHandle shm = reinterpret_cast<RPCShmHandle>(456);
  RPCHandle handles[1] = {shm};
  size_t size = 789;
  RPCSendInterfaceMock::SendCallExpect expect;
  expect._return = 123;
  expect.message_id = BufferRPCImpl::REGISTER_SHARED_MEMORY;
  expect.data = &size;
  expect.size = sizeof(size);
  expect.handles = handles;
  expect.handle_count = 1;
  server_mock()->AddSendCallExpect(expect);

  EXPECT_EQ(123, proxy()->RegisterSharedMemory(shm, size));
}

// Tests the implementation of UnregisterSharedMemory, checking that it sends
// the correct message.
TEST_F(BufferSyncProxyTest, TestUnregisterSharedMemory) {
  unsigned int shm_id = 456;
  RPCSendInterfaceMock::SendCallExpect expect;
  expect._return = 0;
  expect.message_id = BufferRPCImpl::UNREGISTER_SHARED_MEMORY;
  expect.data = &shm_id;
  expect.size = sizeof(shm_id);
  expect.handles = NULL;
  expect.handle_count = 0;
  server_mock()->AddSendCallExpect(expect);

  proxy()->UnregisterSharedMemory(456);
}

// Tests the implementation of SetCommandBuffer, checking that it sends the
// correct message.
TEST_F(BufferSyncProxyTest, TestSetCommandBuffer) {
  BufferRPCImpl::SetCommandBufferStruct params;
  params.shm_id = 53;
  params.offset = 1234;
  params.size = 5678;
  params.start_get = 42;

  RPCSendInterfaceMock::SendCallExpect expect;
  expect._return = 0;
  expect.message_id = BufferRPCImpl::SET_COMMAND_BUFFER;
  expect.data = &params;
  expect.size = sizeof(params);
  expect.handles = NULL;
  expect.handle_count = 0;
  server_mock()->AddSendCallExpect(expect);

  proxy()->SetCommandBuffer(53, 1234, 5678, 42);
}

// Tests the implementation of Put, checking that it sends the correct message.
TEST_F(BufferSyncProxyTest, TestPut) {
  RPCSendInterfaceMock::SendCallExpect expect;
  expect._return = 0;
  expect.message_id = BufferRPCImpl::PUT;
  CommandBufferOffset value = 67;
  expect.data = &value;
  expect.size = sizeof(value);
  expect.handles = NULL;
  expect.handle_count = 0;
  server_mock()->AddSendCallExpect(expect);

  proxy()->Put(67);
}

// Tests the implementation of Get, checking that it sends the correct message
// and returns the correct value.
TEST_F(BufferSyncProxyTest, TestGet) {
  RPCSendInterfaceMock::SendCallExpect expect;
  expect._return = 72;
  expect.message_id = BufferRPCImpl::GET;
  expect.data = NULL;
  expect.size = 0;
  expect.handles = NULL;
  expect.handle_count = 0;
  server_mock()->AddSendCallExpect(expect);

  EXPECT_EQ(72, proxy()->Get());
}

// Tests the implementation of GetToken, checking that it sends the correct
// message and returns the correct value.
TEST_F(BufferSyncProxyTest, TestGetToken) {
  RPCSendInterfaceMock::SendCallExpect expect;
  expect._return = 38;
  expect.message_id = BufferRPCImpl::GET_TOKEN;
  expect.data = NULL;
  expect.size = 0;
  expect.handles = NULL;
  expect.handle_count = 0;
  server_mock()->AddSendCallExpect(expect);

  EXPECT_EQ(38, proxy()->GetToken());
}

// Tests the implementation of WaitGetChanges, checking that it sends the
// correct message and returns the correct value.
TEST_F(BufferSyncProxyTest, TestWaitGetChanges) {
  RPCSendInterfaceMock::SendCallExpect expect;
  expect._return = 53;
  expect.message_id = BufferRPCImpl::WAIT_GET_CHANGES;
  CommandBufferOffset value = 101;
  expect.data = &value;
  expect.size = sizeof(value);
  expect.handles = NULL;
  expect.handle_count = 0;
  server_mock()->AddSendCallExpect(expect);

  EXPECT_EQ(53, proxy()->WaitGetChanges(101));
}

// Tests the implementation of SignalGetChanges, checking that it sends the
// correct message.
TEST_F(BufferSyncProxyTest, TestSignalGetChanges) {
  BufferRPCImpl::SignalGetChangesStruct params;
  params.current_value = 3141;
  params.rpc_message_id = 5926;

  RPCSendInterfaceMock::SendCallExpect expect;
  expect._return = 0;
  expect.message_id = BufferRPCImpl::SIGNAL_GET_CHANGES;
  expect.data = &params;
  expect.size = sizeof(params);
  expect.handles = NULL;
  expect.handle_count = 0;
  server_mock()->AddSendCallExpect(expect);

  proxy()->SignalGetChanges(3141, 5926);
}

// Tests the implementation of GetStatus, checking that it sends the correct
// message and returns the correct value.
TEST_F(BufferSyncProxyTest, TestGetStatus) {
  RPCSendInterfaceMock::SendCallExpect expect;
  expect._return = BufferSyncInterface::PARSING;
  expect.message_id = BufferRPCImpl::GET_STATUS;
  expect.data = NULL;
  expect.size = 0;
  expect.handles = NULL;
  expect.handle_count = 0;
  server_mock()->AddSendCallExpect(expect);

  EXPECT_EQ(BufferSyncInterface::PARSING, proxy()->GetStatus());
}

// Tests the implementation of GetParseError, checking that it sends the correct
// message and returns the correct value.
TEST_F(BufferSyncProxyTest, TestGetParseError) {
  RPCSendInterfaceMock::SendCallExpect expect;
  expect._return = BufferSyncInterface::PARSE_UNKNOWN_COMMAND;
  expect.message_id = BufferRPCImpl::GET_PARSE_ERROR;
  expect.data = NULL;
  expect.size = 0;
  expect.handles = NULL;
  expect.handle_count = 0;
  server_mock()->AddSendCallExpect(expect);

  EXPECT_EQ(BufferSyncInterface::PARSE_UNKNOWN_COMMAND,
            proxy()->GetParseError());
}

}  // namespace command_buffer
}  // namespace o3d
