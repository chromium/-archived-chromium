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


// This file has the unit tests for the IdAllocator class.

#include "tests/common/win/testing_common.h"
#include "command_buffer/client/cross/id_allocator.h"

namespace o3d {
namespace command_buffer {

using command_buffer::ResourceID;

class IdAllocatorTest : public testing::Test {
 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}

  IdAllocator* id_allocator() { return &id_allocator_; }

 private:
  IdAllocator id_allocator_;
};

// Checks basic functionality: AllocateID, FreeID, InUse.
TEST_F(IdAllocatorTest, TestBasic) {
  IdAllocator *allocator = id_allocator();
  // Check that resource 0 is not in use
  EXPECT_FALSE(allocator->InUse(0));

  // Allocate an ID, check that it's in use.
  ResourceID id1 = allocator->AllocateID();
  EXPECT_TRUE(allocator->InUse(id1));

  // Allocate another ID, check that it's in use, and different from the first
  // one.
  ResourceID id2 = allocator->AllocateID();
  EXPECT_TRUE(allocator->InUse(id2));
  EXPECT_NE(id1, id2);

  // Free one of the IDs, check that it's not in use any more.
  allocator->FreeID(id1);
  EXPECT_FALSE(allocator->InUse(id1));

  // Frees the other ID, check that it's not in use any more.
  allocator->FreeID(id2);
  EXPECT_FALSE(allocator->InUse(id2));
}

// Checks that the resource IDs are allocated conservatively, and re-used after
// being freed.
TEST_F(IdAllocatorTest, TestAdvanced) {
  IdAllocator *allocator = id_allocator();

  // Allocate a significant number of resources.
  const int kNumResources = 100;
  ResourceID ids[kNumResources];
  for (int i = 0; i < kNumResources; ++i) {
    ids[i] = allocator->AllocateID();
    EXPECT_TRUE(allocator->InUse(ids[i]));
  }

  // Check that the allocation is conservative with resource IDs, that is that
  // the resource IDs don't go over kNumResources - so that the service doesn't
  // have to allocate too many internal structures when the resources are used.
  for (int i = 0; i < kNumResources; ++i) {
    EXPECT_GT(kNumResources, ids[i]);
  }

  // Check that the next resources are still free.
  for (int i = 0; i < kNumResources; ++i) {
    EXPECT_FALSE(allocator->InUse(kNumResources + i));
  }

  // Check that a new allocation re-uses the resource we just freed.
  ResourceID id1 = ids[kNumResources / 2];
  allocator->FreeID(id1);
  EXPECT_FALSE(allocator->InUse(id1));
  ResourceID id2 = allocator->AllocateID();
  EXPECT_TRUE(allocator->InUse(id2));
  EXPECT_EQ(id1, id2);
}

}  // namespace command_buffer
}  // namespace o3d
