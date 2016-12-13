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

#include "tests/common/win/testing_common.h"
#include "import/cross/destination_buffer.h"
#include "core/cross/class_manager.h"
#include "core/cross/object_manager.h"
#include "core/cross/pack.h"
#include "core/cross/service_dependency.h"

namespace o3d {

class DestinationBufferTest : public testing::Test {
 protected:
  DestinationBufferTest()
      : class_manager_(g_service_locator),
        object_manager_(g_service_locator) {
    class_manager_->AddTypedClass<DestinationBuffer>();
  }

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

 private:
  ServiceDependency<ClassManager> class_manager_;
  ServiceDependency<ObjectManager> object_manager_;
  Pack* pack_;
};

void DestinationBufferTest::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void DestinationBufferTest::TearDown() {
  object_manager_->DestroyPack(pack_);
}


// Creates a Destination buffer, tests basic properties, and checks that writing
// then reading data works.
TEST_F(DestinationBufferTest, TestDestinationBuffer) {
  Buffer *buffer = pack()->Create<DestinationBuffer>();
  EXPECT_TRUE(buffer->IsA(DestinationBuffer::GetApparentClass()));
  EXPECT_TRUE(buffer->IsA(VertexBuffer::GetApparentClass()));
  EXPECT_TRUE(buffer->IsA(Buffer::GetApparentClass()));

  const size_t kSize = 100;
  Field* field = buffer->CreateField(UInt32Field::GetApparentClass(), 1);
  ASSERT_TRUE(field != NULL);
  ASSERT_TRUE(buffer->AllocateElements(kSize));
  EXPECT_EQ(kSize * sizeof(uint32), buffer->GetSizeInBytes());  // NOLINT

  // Put some data into the buffer.
  uint32 *data = NULL;
  ASSERT_TRUE(buffer->LockAs(Buffer::WRITE_ONLY, &data));
  ASSERT_TRUE(data != NULL);
  for (uint32 i = 0; i < kSize; ++i) {
    data[i] = i;
  }
  ASSERT_TRUE(buffer->Unlock());

  data = NULL;
  // Read the data from the buffer, checks that it's the expected values.
  ASSERT_TRUE(buffer->LockAs(Buffer::READ_ONLY, &data));
  ASSERT_TRUE(data != NULL);
  for (uint32 i = 0; i < kSize; ++i) {
    EXPECT_EQ(i, data[i]);
  }
  ASSERT_TRUE(buffer->Unlock());
}


}  // namespace o3d

