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


// This file contains the tests for the FencedAllocator class.

#include "tests/common/win/testing_common.h"
#include "command_buffer/client/cross/cmd_buffer_helper.h"
#include "command_buffer/client/cross/fenced_allocator.h"
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

// Test fixture for FencedAllocator test - Creates a FencedAllocator, using a
// CommandBufferHelper with a mock AsyncAPIInterface for its interface (calling
// it directly, not through the RPC mechanism), making sure NOOPs are ignored
// and SET_TOKEN are properly forwarded to the engine.
class FencedAllocatorTest : public testing::Test {
 public:
  static const unsigned int kBufferSize = 1024;

 protected:
  virtual void SetUp() {
    api_mock_.reset(new AsyncAPIMock);
    // ignore noops in the mock - we don't want to inspect the internals of the
    // helper.
    EXPECT_CALL(*api_mock_, DoCommand(NOOP, 0, _))
        .WillRepeatedly(Return(BufferSyncInterface::PARSE_NO_ERROR));
    // Forward the SetToken calls to the engine
    EXPECT_CALL(*api_mock(), DoCommand(SET_TOKEN, 1, _))
        .WillRepeatedly(DoAll(Invoke(api_mock(), &AsyncAPIMock::SetToken),
                              Return(BufferSyncInterface::PARSE_NO_ERROR)));
    engine_.reset(new CommandBufferEngine(api_mock_.get()));
    api_mock_->set_engine(engine_.get());

    nacl::SocketAddress client_address = { "test-socket" };
    client_socket_ = nacl::BoundSocket(&client_address);
    engine_->InitConnection();
    helper_.reset(new CommandBufferHelper(engine_.get()));
    helper_->Init(100);

    allocator_.reset(new FencedAllocator(kBufferSize, helper_.get()));
  }

  virtual void TearDown() {
    EXPECT_TRUE(allocator_->CheckConsistency());
    allocator_.release();
    helper_.release();
    engine_->CloseConnection();
    nacl::Close(client_socket_);
  }

  CommandBufferHelper *helper() { return helper_.get(); }
  CommandBufferEngine *engine() { return engine_.get(); }
  AsyncAPIMock *api_mock() { return api_mock_.get(); }
  FencedAllocator *allocator() { return allocator_.get(); }
 private:
  scoped_ptr<AsyncAPIMock> api_mock_;
  scoped_ptr<CommandBufferEngine> engine_;
  scoped_ptr<CommandBufferHelper> helper_;
  scoped_ptr<FencedAllocator> allocator_;
  nacl::Handle client_socket_;
};

#ifndef COMPILER_MSVC
const unsigned int FencedAllocatorTest::kBufferSize;
#endif

// Checks basic alloc and free.
TEST_F(FencedAllocatorTest, TestBasic) {
  allocator()->CheckConsistency();

  const unsigned int kSize = 16;
  FencedAllocator::Offset offset = allocator()->Alloc(kSize);
  EXPECT_NE(FencedAllocator::kInvalidOffset, offset);
  EXPECT_GE(kBufferSize, offset+kSize);
  EXPECT_TRUE(allocator()->CheckConsistency());

  allocator()->Free(offset);
  EXPECT_TRUE(allocator()->CheckConsistency());
}

// Checks out-of-memory condition.
TEST_F(FencedAllocatorTest, TestOutOfMemory) {
  EXPECT_TRUE(allocator()->CheckConsistency());

  const unsigned int kSize = 16;
  const unsigned int kAllocCount = kBufferSize / kSize;
  CHECK(kAllocCount * kSize == kBufferSize);

  // Allocate several buffers to fill in the memory.
  FencedAllocator::Offset offsets[kAllocCount];
  for (unsigned int i = 0; i < kAllocCount; ++i) {
    offsets[i] = allocator()->Alloc(kSize);
    EXPECT_NE(FencedAllocator::kInvalidOffset, offsets[i]);
    EXPECT_GE(kBufferSize, offsets[i]+kSize);
    EXPECT_TRUE(allocator()->CheckConsistency());
  }

  // This allocation should fail.
  FencedAllocator::Offset offset_failed = allocator()->Alloc(kSize);
  EXPECT_EQ(FencedAllocator::kInvalidOffset, offset_failed);
  EXPECT_TRUE(allocator()->CheckConsistency());

  // Free one successful allocation, reallocate with half the size
  allocator()->Free(offsets[0]);
  EXPECT_TRUE(allocator()->CheckConsistency());
  offsets[0] = allocator()->Alloc(kSize/2);
  EXPECT_NE(FencedAllocator::kInvalidOffset, offsets[0]);
  EXPECT_GE(kBufferSize, offsets[0]+kSize);
  EXPECT_TRUE(allocator()->CheckConsistency());

  // This allocation should fail as well.
  offset_failed = allocator()->Alloc(kSize);
  EXPECT_EQ(FencedAllocator::kInvalidOffset, offset_failed);
  EXPECT_TRUE(allocator()->CheckConsistency());

  // Free up everything.
  for (unsigned int i = 0; i < kAllocCount; ++i) {
    allocator()->Free(offsets[i]);
    EXPECT_TRUE(allocator()->CheckConsistency());
  }
}

// Checks the free-pending-token mechanism.
TEST_F(FencedAllocatorTest, TestFreePendingToken) {
  EXPECT_TRUE(allocator()->CheckConsistency());

  const unsigned int kSize = 16;
  const unsigned int kAllocCount = kBufferSize / kSize;
  CHECK(kAllocCount * kSize == kBufferSize);

  // Allocate several buffers to fill in the memory.
  FencedAllocator::Offset offsets[kAllocCount];
  for (unsigned int i = 0; i < kAllocCount; ++i) {
    offsets[i] = allocator()->Alloc(kSize);
    EXPECT_NE(FencedAllocator::kInvalidOffset, offsets[i]);
    EXPECT_GE(kBufferSize, offsets[i]+kSize);
    EXPECT_TRUE(allocator()->CheckConsistency());
  }

  // This allocation should fail.
  FencedAllocator::Offset offset_failed = allocator()->Alloc(kSize);
  EXPECT_EQ(FencedAllocator::kInvalidOffset, offset_failed);
  EXPECT_TRUE(allocator()->CheckConsistency());

  // Free one successful allocation, pending fence.
  unsigned int token = helper()->InsertToken();
  allocator()->FreePendingToken(offsets[0], token);
  EXPECT_TRUE(allocator()->CheckConsistency());

  // The way we hooked up the helper and engine, it won't process commands
  // until it has to wait for something. Which means the token shouldn't have
  // passed yet at this point.
  EXPECT_GT(token, engine()->GetToken());

  // This allocation will need to reclaim the space freed above, so that should
  // process the commands until the token is passed.
  offsets[0] = allocator()->Alloc(kSize);
  EXPECT_NE(FencedAllocator::kInvalidOffset, offsets[0]);
  EXPECT_GE(kBufferSize, offsets[0]+kSize);
  EXPECT_TRUE(allocator()->CheckConsistency());
  // Check that the token has indeed passed.
  EXPECT_LE(token, engine()->GetToken());

  // Free up everything.
  for (unsigned int i = 0; i < kAllocCount; ++i) {
    allocator()->Free(offsets[i]);
    EXPECT_TRUE(allocator()->CheckConsistency());
  }
}

// Tests GetLargestFreeSize
TEST_F(FencedAllocatorTest, TestGetLargestFreeSize) {
  EXPECT_TRUE(allocator()->CheckConsistency());
  EXPECT_EQ(kBufferSize, allocator()->GetLargestFreeSize());

  FencedAllocator::Offset offset = allocator()->Alloc(kBufferSize);
  ASSERT_NE(FencedAllocator::kInvalidOffset, offset);
  EXPECT_EQ(0, allocator()->GetLargestFreeSize());
  allocator()->Free(offset);
  EXPECT_EQ(kBufferSize, allocator()->GetLargestFreeSize());

  const unsigned int kSize = 16;
  offset = allocator()->Alloc(kSize);
  ASSERT_NE(FencedAllocator::kInvalidOffset, offset);
  // The following checks that the buffer is allocated "smartly" - which is
  // dependent on the implementation. But both first-fit or best-fit would
  // ensure that.
  EXPECT_EQ(kBufferSize - kSize, allocator()->GetLargestFreeSize());

  // Allocate 2 more buffers (now 3), and then free the first two. This is to
  // ensure a hole. Note that this is dependent on the first-fit current
  // implementation.
  FencedAllocator::Offset offset1 = allocator()->Alloc(kSize);
  ASSERT_NE(FencedAllocator::kInvalidOffset, offset1);
  FencedAllocator::Offset offset2 = allocator()->Alloc(kSize);
  ASSERT_NE(FencedAllocator::kInvalidOffset, offset2);
  allocator()->Free(offset);
  allocator()->Free(offset1);
  EXPECT_EQ(kBufferSize - 3 * kSize, allocator()->GetLargestFreeSize());

  offset = allocator()->Alloc(kBufferSize - 3 * kSize);
  ASSERT_NE(FencedAllocator::kInvalidOffset, offset);
  EXPECT_EQ(2 * kSize, allocator()->GetLargestFreeSize());

  offset1 = allocator()->Alloc(2 * kSize);
  ASSERT_NE(FencedAllocator::kInvalidOffset, offset1);
  EXPECT_EQ(0, allocator()->GetLargestFreeSize());

  allocator()->Free(offset);
  allocator()->Free(offset1);
  allocator()->Free(offset2);
}

// Tests GetLargestFreeOrPendingSize
TEST_F(FencedAllocatorTest, TestGetLargestFreeOrPendingSize) {
  EXPECT_TRUE(allocator()->CheckConsistency());
  EXPECT_EQ(kBufferSize, allocator()->GetLargestFreeOrPendingSize());

  FencedAllocator::Offset offset = allocator()->Alloc(kBufferSize);
  ASSERT_NE(FencedAllocator::kInvalidOffset, offset);
  EXPECT_EQ(0, allocator()->GetLargestFreeOrPendingSize());
  allocator()->Free(offset);
  EXPECT_EQ(kBufferSize, allocator()->GetLargestFreeOrPendingSize());

  const unsigned int kSize = 16;
  offset = allocator()->Alloc(kSize);
  ASSERT_NE(FencedAllocator::kInvalidOffset, offset);
  // The following checks that the buffer is allocates "smartly" - which is
  // dependent on the implementation. But both first-fit or best-fit would
  // ensure that.
  EXPECT_EQ(kBufferSize - kSize, allocator()->GetLargestFreeOrPendingSize());

  // Allocate 2 more buffers (now 3), and then free the first two. This is to
  // ensure a hole. Note that this is dependent on the first-fit current
  // implementation.
  FencedAllocator::Offset offset1 = allocator()->Alloc(kSize);
  ASSERT_NE(FencedAllocator::kInvalidOffset, offset1);
  FencedAllocator::Offset offset2 = allocator()->Alloc(kSize);
  ASSERT_NE(FencedAllocator::kInvalidOffset, offset2);
  allocator()->Free(offset);
  allocator()->Free(offset1);
  EXPECT_EQ(kBufferSize - 3 * kSize,
            allocator()->GetLargestFreeOrPendingSize());

  // Free the last one, pending a token.
  unsigned int token = helper()->InsertToken();
  allocator()->FreePendingToken(offset2, token);

  // Now all the buffers have been freed...
  EXPECT_EQ(kBufferSize, allocator()->GetLargestFreeOrPendingSize());
  // .. but one is still waiting for the token.
  EXPECT_EQ(kBufferSize - 3 * kSize,
            allocator()->GetLargestFreeSize());

  // The way we hooked up the helper and engine, it won't process commands
  // until it has to wait for something. Which means the token shouldn't have
  // passed yet at this point.
  EXPECT_GT(token, engine()->GetToken());
  // This allocation will need to reclaim the space freed above, so that should
  // process the commands until the token is passed, but it will succeed.
  offset = allocator()->Alloc(kBufferSize);
  ASSERT_NE(FencedAllocator::kInvalidOffset, offset);
  // Check that the token has indeed passed.
  EXPECT_LE(token, engine()->GetToken());
  allocator()->Free(offset);

  // Everything now has been freed...
  EXPECT_EQ(kBufferSize, allocator()->GetLargestFreeOrPendingSize());
  // ... for real.
  EXPECT_EQ(kBufferSize, allocator()->GetLargestFreeSize());
}

// Test fixture for FencedAllocatorWrapper test - Creates a
// FencedAllocatorWrapper, using a CommandBufferHelper with a mock
// AsyncAPIInterface for its interface (calling it directly, not through the
// RPC mechanism), making sure NOOPs are ignored and SET_TOKEN are properly
// forwarded to the engine.
class FencedAllocatorWrapperTest : public testing::Test {
 public:
  static const unsigned int kBufferSize = 1024;

 protected:
  virtual void SetUp() {
    api_mock_.reset(new AsyncAPIMock);
    // ignore noops in the mock - we don't want to inspect the internals of the
    // helper.
    EXPECT_CALL(*api_mock_, DoCommand(NOOP, 0, _))
        .WillRepeatedly(Return(BufferSyncInterface::PARSE_NO_ERROR));
    // Forward the SetToken calls to the engine
    EXPECT_CALL(*api_mock(), DoCommand(SET_TOKEN, 1, _))
        .WillRepeatedly(DoAll(Invoke(api_mock(), &AsyncAPIMock::SetToken),
                              Return(BufferSyncInterface::PARSE_NO_ERROR)));
    engine_.reset(new CommandBufferEngine(api_mock_.get()));
    api_mock_->set_engine(engine_.get());

    nacl::SocketAddress client_address = { "test-socket" };
    client_socket_ = nacl::BoundSocket(&client_address);
    engine_->InitConnection();
    helper_.reset(new CommandBufferHelper(engine_.get()));
    helper_->Init(100);

    // Though allocating this buffer isn't strictly necessary, it makes
    // allocations point to valid addresses, so they could be used for
    // something.
    buffer_.reset(new char[kBufferSize]);
    allocator_.reset(new FencedAllocatorWrapper(kBufferSize, helper_.get(),
                                                base()));
  }

  virtual void TearDown() {
    EXPECT_TRUE(allocator_->CheckConsistency());
    allocator_.release();
    buffer_.release();
    helper_.release();
    engine_->CloseConnection();
    nacl::Close(client_socket_);
  }

  CommandBufferHelper *helper() { return helper_.get(); }
  CommandBufferEngine *engine() { return engine_.get(); }
  AsyncAPIMock *api_mock() { return api_mock_.get(); }
  FencedAllocatorWrapper *allocator() { return allocator_.get(); }
  char *base() { return buffer_.get(); }
 private:
  scoped_ptr<AsyncAPIMock> api_mock_;
  scoped_ptr<CommandBufferEngine> engine_;
  scoped_ptr<CommandBufferHelper> helper_;
  scoped_ptr<FencedAllocatorWrapper> allocator_;
  scoped_array<char> buffer_;
  nacl::Handle client_socket_;
};

#ifndef COMPILER_MSVC
const unsigned int FencedAllocatorWrapperTest::kBufferSize;
#endif

// Checks basic alloc and free.
TEST_F(FencedAllocatorWrapperTest, TestBasic) {
  allocator()->CheckConsistency();

  const unsigned int kSize = 16;
  void *pointer = allocator()->Alloc(kSize);
  ASSERT_TRUE(pointer);
  EXPECT_LE(base(), static_cast<char *>(pointer));
  EXPECT_GE(kBufferSize, static_cast<char *>(pointer) - base() + kSize);
  EXPECT_TRUE(allocator()->CheckConsistency());

  allocator()->Free(pointer);
  EXPECT_TRUE(allocator()->CheckConsistency());

  char *pointer_char = allocator()->AllocTyped<char>(kSize);
  ASSERT_TRUE(pointer_char);
  EXPECT_LE(base(), pointer_char);
  EXPECT_GE(base() + kBufferSize, pointer_char + kSize);
  allocator()->Free(pointer_char);
  EXPECT_TRUE(allocator()->CheckConsistency());

  unsigned int *pointer_uint = allocator()->AllocTyped<unsigned int>(kSize);
  ASSERT_TRUE(pointer_uint);
  EXPECT_LE(base(), reinterpret_cast<char *>(pointer_uint));
  EXPECT_GE(base() + kBufferSize,
            reinterpret_cast<char *>(pointer_uint + kSize));

  // Check that it did allocate kSize * sizeof(unsigned int). We can't tell
  // directly, except from the remaining size.
  EXPECT_EQ(kBufferSize - kSize * sizeof(*pointer_uint),
            allocator()->GetLargestFreeSize());
  allocator()->Free(pointer_uint);
}

// Checks out-of-memory condition.
TEST_F(FencedAllocatorWrapperTest, TestOutOfMemory) {
  allocator()->CheckConsistency();

  const unsigned int kSize = 16;
  const unsigned int kAllocCount = kBufferSize / kSize;
  CHECK(kAllocCount * kSize == kBufferSize);

  // Allocate several buffers to fill in the memory.
  void *pointers[kAllocCount];
  for (unsigned int i = 0; i < kAllocCount; ++i) {
    pointers[i] = allocator()->Alloc(kSize);
    EXPECT_TRUE(pointers[i]);
    EXPECT_TRUE(allocator()->CheckConsistency());
  }

  // This allocation should fail.
  void *pointer_failed = allocator()->Alloc(kSize);
  EXPECT_FALSE(pointer_failed);
  EXPECT_TRUE(allocator()->CheckConsistency());

  // Free one successful allocation, reallocate with half the size
  allocator()->Free(pointers[0]);
  EXPECT_TRUE(allocator()->CheckConsistency());
  pointers[0] = allocator()->Alloc(kSize/2);
  EXPECT_TRUE(pointers[0]);
  EXPECT_TRUE(allocator()->CheckConsistency());

  // This allocation should fail as well.
  pointer_failed = allocator()->Alloc(kSize);
  EXPECT_FALSE(pointer_failed);
  EXPECT_TRUE(allocator()->CheckConsistency());

  // Free up everything.
  for (unsigned int i = 0; i < kAllocCount; ++i) {
    allocator()->Free(pointers[i]);
    EXPECT_TRUE(allocator()->CheckConsistency());
  }
}

// Checks the free-pending-token mechanism.
TEST_F(FencedAllocatorWrapperTest, TestFreePendingToken) {
  allocator()->CheckConsistency();

  const unsigned int kSize = 16;
  const unsigned int kAllocCount = kBufferSize / kSize;
  CHECK(kAllocCount * kSize == kBufferSize);

  // Allocate several buffers to fill in the memory.
  void *pointers[kAllocCount];
  for (unsigned int i = 0; i < kAllocCount; ++i) {
    pointers[i] = allocator()->Alloc(kSize);
    EXPECT_TRUE(pointers[i]);
    EXPECT_TRUE(allocator()->CheckConsistency());
  }

  // This allocation should fail.
  void *pointer_failed = allocator()->Alloc(kSize);
  EXPECT_FALSE(pointer_failed);
  EXPECT_TRUE(allocator()->CheckConsistency());

  // Free one successful allocation, pending fence.
  unsigned int token = helper()->InsertToken();
  allocator()->FreePendingToken(pointers[0], token);
  EXPECT_TRUE(allocator()->CheckConsistency());

  // The way we hooked up the helper and engine, it won't process commands
  // until it has to wait for something. Which means the token shouldn't have
  // passed yet at this point.
  EXPECT_GT(token, engine()->GetToken());

  // This allocation will need to reclaim the space freed above, so that should
  // process the commands until the token is passed.
  pointers[0] = allocator()->Alloc(kSize);
  EXPECT_TRUE(pointers[0]);
  EXPECT_TRUE(allocator()->CheckConsistency());
  // Check that the token has indeed passed.
  EXPECT_LE(token, engine()->GetToken());

  // Free up everything.
  for (unsigned int i = 0; i < kAllocCount; ++i) {
    allocator()->Free(pointers[i]);
    EXPECT_TRUE(allocator()->CheckConsistency());
  }
}


}  // namespace command_buffer
}  // namespace o3d
