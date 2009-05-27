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

#ifndef O3D_COMMAND_BUFFER_COMMON_CROSS_MOCKS_H__
#define O3D_COMMAND_BUFFER_COMMON_CROSS_MOCKS_H__

#include <vector>
#include "gmock/gmock.h"
#include "command_buffer/common/cross/rpc.h"
#include "command_buffer/common/cross/buffer_sync_api.h"

namespace o3d {
namespace command_buffer {

// Mocks a RPC send interface. This class only mocks SendCall currently.
// Set call expectations with AddSendCallExpect: one for each call with exact
// parameters, and desired return value, then run the test.
class RPCSendInterfaceMock : public RPCSendInterface {
 public:
  RPCSendInterfaceMock() : called_(0) {}
  virtual ~RPCSendInterfaceMock() { Check(); }

  // Checks that the expected number of calls actually happened.
  void Check() {
    EXPECT_EQ(expects_.size(), called_);
  }

  // Struct describing a SendCall call expectation, with the desired return
  // value.
  struct SendCallExpect {
    RPCImplInterface::ReturnValue _return;
    int message_id;
    const void * data;
    size_t size;
    RPCHandle * handles;
    size_t handle_count;
  };

  // Adds an expectation for a SendCall call.
  void AddSendCallExpect(const SendCallExpect &expect) {
    expects_.push_back(expect);
  }

  // Mock SendCall implementation. This will check the arguments against the
  // expected values (the values pointed by 'data' are compared, not the
  // pointer), and return the desired return value.
  virtual RPCImplInterface::ReturnValue SendCall(int message_id,
                                                 const void * data,
                                                 size_t size,
                                                 RPCHandle const *handles,
                                                 size_t handle_count) {
    if (called_ < expects_.size()) {
      const SendCallExpect &expect = expects_[called_];
      ++called_;
      EXPECT_EQ(expect.message_id, message_id);
      EXPECT_EQ(expect.size, size);
      if (expect.size != size) return 0;
      EXPECT_EQ(expect.handle_count, handle_count);
      if (expect.handle_count != handle_count) return 0;
      if (size > 0) {
        EXPECT_EQ(0, memcmp(expect.data, data, size));
      } else {
        EXPECT_FALSE(data);
      }
      if (handle_count > 0) {
        for (unsigned int i = 0; i < handle_count; ++i) {
          EXPECT_TRUE(expect.handles[i] == handles[i]);
        }
      } else {
        EXPECT_FALSE(handles);
      }
      return expect._return;
    } else {
      ++called_;
      // This is really an EXPECT_FALSE, but we get to display useful values.
      EXPECT_GE(expects_.size(), called_);
      // We need to return something but we don't know what, we don't have any
      // expectation for this call. 0 will do.
      return 0;
    }
  }
 private:
  size_t called_;
  std::vector<SendCallExpect> expects_;
};

// Mocks a BufferSyncInterface, using GMock.
class BufferSyncMock : public BufferSyncInterface {
 public:
  MOCK_METHOD0(InitConnection, void());
  MOCK_METHOD0(CloseConnection, void());
  MOCK_METHOD2(RegisterSharedMemory, unsigned int(RPCShmHandle buffer,
                                                  size_t size));
  MOCK_METHOD1(UnregisterSharedMemory, void(unsigned int shm_id));
  MOCK_METHOD4(SetCommandBuffer, void(unsigned int shm_id,
                                      ptrdiff_t offset,
                                      size_t size,
                                      CommandBufferOffset start_get));
  MOCK_METHOD1(Put, void(CommandBufferOffset offset));
  MOCK_METHOD0(Get, CommandBufferOffset());
  MOCK_METHOD0(GetToken, unsigned int());
  MOCK_METHOD1(WaitGetChanges,
               CommandBufferOffset(CommandBufferOffset current_value));
  MOCK_METHOD2(SignalGetChanges, void(CommandBufferOffset current_value,
                                      int rpc_message_id));
  MOCK_METHOD0(GetStatus, ParserStatus());
  MOCK_METHOD0(GetParseError, ParseError());
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_COMMON_CROSS_MOCKS_H__
