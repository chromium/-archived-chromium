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


// This file implements unit tests for class StreamBank.

#include "core/cross/client.h"
#include "core/cross/skin.h"
#include "core/cross/stream_bank.h"
#include "core/cross/fake_vertex_source.h"
#include "tests/common/win/testing_common.h"

namespace o3d {

namespace {

static const float kEpsilon = 0.0000001f;

bool CompareVertices(StreamBank* source,
                     Stream::Semantic semantic,
                     int semantic_index,
                     const float* values,
                     float multiplier) {
  const Stream* stream = source->GetVertexStream(semantic, semantic_index);
  if (!stream) {
    return false;
  }

  const Field& field = stream->field();
  Buffer* buffer = field.buffer();
  if (!buffer) {
    return false;
  }
  BufferLockHelper helper(buffer);
  void* data = helper.GetData(Buffer::READ_ONLY);
  if (!data) {
    return false;
  }

  unsigned num_vertices = stream->GetMaxVertices();
  unsigned stride = buffer->stride();

  const float* source_values = PointerFromVoidPointer<const float*>(
      data, field.offset());
  while (num_vertices) {
    for (unsigned jj = 0; jj < 3; ++jj) {
      float difference = fabsf(source_values[jj] - values[jj] * multiplier);
      if (difference > kEpsilon) {
        return false;
      }
    }
    values += 3;
    source_values = AddPointerOffset(source_values, stride);
    --num_vertices;
  }
  return true;
}

}  // anonymous namespace

class StreamBankTest : public testing::Test {
 protected:

  StreamBankTest()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

 private:
  ServiceDependency<ObjectManager> object_manager_;
  Pack *pack_;
};

void StreamBankTest::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void StreamBankTest::TearDown() {
  pack_->Destroy();
}

TEST_F(StreamBankTest, Basic) {
  static float cube_vertices[][3] = {
    { -1.0f, -1.0f,  1.0f, },  // vertex v0
    { +1.0f, -1.0f,  1.0f, },  // vertex v1
    { +1.0f, -1.0f, -1.0f, },  // vertex v2
    { -1.0f, -1.0f, -1.0f, },  // vertex v3
    { -1.0f,  1.0f,  1.0f, },  // vertex v4
    { +1.0f,  1.0f,  1.0f, },  // vertex v5
    { +1.0f,  1.0f, -1.0f, },  // vertex v6
    { -1.0f,  1.0f, -1.0f, },  // vertex v7
  };
  const unsigned kNumVertices = arraysize(cube_vertices);
  const unsigned kNumComponents = arraysize(cube_vertices[0]);

  StreamBank* stream_bank = pack()->Create<StreamBank>();
  // Check that StreamBank got created.
  ASSERT_TRUE(stream_bank != NULL);

  // Check Setting Vertex Streams.
  VertexBuffer* vertex_buffer = pack()->Create<VertexBuffer>();
  ASSERT_TRUE(vertex_buffer != NULL);

  Field* field = vertex_buffer->CreateField(FloatField::GetApparentClass(),
                                            kNumComponents);
  ASSERT_TRUE(field != NULL);
  ASSERT_TRUE(vertex_buffer->AllocateElements(kNumVertices));
  field->SetFromFloats(&cube_vertices[0][0], kNumComponents, 0, kNumVertices);

  EXPECT_TRUE(stream_bank->SetVertexStream(Stream::POSITION,
                                           0,
                                           field,
                                           0));
  // Check getting a stream.
  const Stream* vertex_stream = stream_bank->GetVertexStream(Stream::POSITION,
                                                             0);
  ASSERT_TRUE(vertex_stream != NULL);
  EXPECT_EQ(&vertex_stream->field(), field);
  EXPECT_EQ(vertex_stream->semantic(), Stream::POSITION);
  EXPECT_EQ(vertex_stream->semantic_index(), 0);

  // Check removing the streams.
  EXPECT_FALSE(stream_bank->RemoveVertexStream(Stream::POSITION, 1));
  EXPECT_FALSE(stream_bank->RemoveVertexStream(Stream::BINORMAL, 0));
  EXPECT_TRUE(stream_bank->RemoveVertexStream(Stream::POSITION, 0));
  EXPECT_FALSE(stream_bank->RemoveVertexStream(Stream::POSITION, 0));
}

TEST_F(StreamBankTest, BindStream) {
  static float some_vertices[][3] = {
    { 1.0f, 2.0f, 3.0f, },
    { 7.0f, 8.0f, 9.0f, },
    { 4.0f, 5.0f, 6.0f, },
  };
  const unsigned kNumVertices = arraysize(some_vertices);
  const unsigned kNumComponents = arraysize(some_vertices[0]);

  StreamBank* stream_bank = pack()->Create<StreamBank>();
  ASSERT_TRUE(stream_bank != NULL);

  // Create 2 Vertex Buffers.
  VertexBuffer* vertex_buffer_1 = pack()->Create<VertexBuffer>();
  VertexBuffer* vertex_buffer_2 = pack()->Create<VertexBuffer>();
  ASSERT_TRUE(vertex_buffer_1 != NULL);
  ASSERT_TRUE(vertex_buffer_2 != NULL);

  // Create a field on each buffer.
  Field* vertex_field_1 = vertex_buffer_1->CreateField(
      FloatField::GetApparentClass(), kNumComponents);
  Field* vertex_field_2 = vertex_buffer_2->CreateField(
      FloatField::GetApparentClass(), kNumComponents);
  ASSERT_TRUE(vertex_field_1 != NULL);
  ASSERT_TRUE(vertex_field_2 != NULL);
  ASSERT_TRUE(vertex_buffer_1->AllocateElements(kNumVertices));
  ASSERT_TRUE(vertex_buffer_2->AllocateElements(kNumVertices));

  // Set up streams on StreamBank.
  EXPECT_TRUE(stream_bank->SetVertexStream(Stream::POSITION,
                                           0,
                                           vertex_field_1,
                                           0));
  EXPECT_TRUE(stream_bank->SetVertexStream(Stream::POSITION,
                                           1,
                                           vertex_field_2,
                                           0));

  // Create 2 source buffers.
  SourceBuffer* source_buffer_1 = pack()->Create<SourceBuffer>();
  SourceBuffer* source_buffer_2 = pack()->Create<SourceBuffer>();
  ASSERT_TRUE(source_buffer_1 != NULL);
  ASSERT_TRUE(source_buffer_2 != NULL);

  // Create a field on each buffer.
  Field* source_field_1 = source_buffer_1->CreateField(
      FloatField::GetApparentClass(), kNumComponents);
  Field* source_field_2 = source_buffer_2->CreateField(
      FloatField::GetApparentClass(), kNumComponents);
  ASSERT_TRUE(source_field_1 != NULL);
  ASSERT_TRUE(source_field_2 != NULL);

  // Put some vertices in the source streams.
  ASSERT_TRUE(source_buffer_1->AllocateElements(kNumVertices));
  source_field_1->SetFromFloats(&some_vertices[0][0], kNumComponents, 0,
                                kNumVertices);
  ASSERT_TRUE(source_buffer_2->AllocateElements(kNumVertices));
  source_field_2->SetFromFloats(&some_vertices[0][0], kNumComponents, 0,
                                kNumVertices);

  scoped_ptr<FakeVertexSource> source(new FakeVertexSource(
      pack()->service_locator()));
  ASSERT_TRUE(source != NULL);

  // Set up streams on source
  EXPECT_TRUE(source->SetVertexStream(Stream::POSITION,
                                      0,
                                      source_field_1,
                                      0));
  EXPECT_TRUE(source->SetVertexStream(Stream::POSITION,
                                      1,
                                      source_field_2,
                                      0));

  // Bind the vertices to both dest streams.
  EXPECT_TRUE(stream_bank->BindStream(source.get(), Stream::POSITION, 0));
  EXPECT_TRUE(stream_bank->BindStream(source.get(), Stream::POSITION, 1));
  // Non-existant streams should fail.
  EXPECT_FALSE(stream_bank->BindStream(source.get(), Stream::POSITION, 2));
  EXPECT_FALSE(stream_bank->BindStream(source.get(), Stream::COLOR, 10));

  // Cause the vertices to get updated.
  stream_bank->UpdateStreams();

  // Test that vertices get updated.
  EXPECT_TRUE(CompareVertices(stream_bank,
                              Stream::POSITION,
                              0,
                              &some_vertices[0][0],
                              2.0f));
  EXPECT_TRUE(CompareVertices(stream_bank,
                              Stream::POSITION,
                              1,
                              &some_vertices[0][0],
                              3.0f));

  // Check that UpdateOutputs only got called once.
  EXPECT_EQ(source->update_outputs_call_count(), 1);
}

}  // namespace o3d
