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


// This file implements unit tests for class Primitive.

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/primitive.h"
#include "core/cross/fake_vertex_source.h"

namespace o3d {

class PrimitiveTest : public testing::Test {
 protected:

  PrimitiveTest()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

 private:
  ServiceDependency<ObjectManager> object_manager_;
  Pack *pack_;
};

void PrimitiveTest::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void PrimitiveTest::TearDown() {
  pack_->Destroy();
}

TEST_F(PrimitiveTest, Basic) {
  static uint32 cube_indices[] = {
    0, 1, 4,  // triangle v0,v1,v4
    1, 5, 4,  // triangle v1,v5,v4
    1, 2, 5,  // triangle v1,v2,v5
    2, 6, 5,  // triangle v2,v6,v5
    2, 3, 6,  // triangle v2,v3,v6
    3, 7, 6,  // triangle v3,v7,v6
    3, 0, 7,  // triangle v3,v0,v7
    0, 4, 7,  // triangle v0,v4,v7
    4, 5, 7,  // triangle v4,v5,v7
    5, 6, 7,  // triangle v5,v6,v7
    3, 2, 0,  // triangle v3,v2,v0
    2, 1, 0   // triangle v2,v1,v0
  };

  Primitive* primitive = pack()->Create<Primitive>();
  StreamBank* stream_bank = pack()->Create<StreamBank>();
  // Check that primitive and StreamBank got created.
  ASSERT_TRUE(primitive != NULL);
  ASSERT_TRUE(stream_bank != NULL);
  primitive->set_stream_bank(stream_bank);
  EXPECT_EQ(primitive->stream_bank(), stream_bank);

  // Check Setting Index Streams.
  IndexBuffer* index_buffer = pack()->Create<IndexBuffer>();

  ASSERT_TRUE(index_buffer->AllocateElements(arraysize(cube_indices)));
  ASSERT_TRUE(index_buffer != NULL);
  index_buffer->index_field()->SetFromUInt32s(cube_indices, 1, 0,
                                              arraysize(cube_indices));
  primitive->set_index_buffer(index_buffer);
  primitive->set_primitive_type(o3d::Primitive::TRIANGLELIST);
  primitive->set_number_primitives(12);
  primitive->set_number_vertices(8);

  EXPECT_EQ(primitive->primitive_type(), o3d::Primitive::TRIANGLELIST);
  EXPECT_EQ(primitive->number_primitives(), 12);
  EXPECT_EQ(primitive->number_vertices(), 8);

  // Check getting the index buffer.
  EXPECT_EQ(primitive->index_buffer(), index_buffer);

  // Check removing the index buffer.
  EXPECT_TRUE(primitive->indexed());
  primitive->set_index_buffer(NULL);
  EXPECT_FALSE(primitive->indexed());
}

}  // namespace o3d
