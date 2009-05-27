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


// This file implements unit tests for class VertexSource.

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/vertex_source.h"
#include "core/cross/fake_vertex_source.h"
#include "core/cross/evaluation_counter.h"

namespace o3d {

namespace {

static const float kEpsilon = 0.0000001f;

bool CompareVertices(FakeVertexSource* source,
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

// TODO: Remove the need for this.
void InvalidateAllParameters(Pack* pack) {
  EvaluationCounter* evaluation_counter(
      pack->service_locator()->GetService<EvaluationCounter>());
  evaluation_counter->InvalidateAllParameters();
}

}  // anonymous namespace

class VertexSourceTest : public testing::Test {
 protected:

  VertexSourceTest()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

 private:
  ServiceDependency<ObjectManager> object_manager_;
  Pack *pack_;
  ErrorStatus* error_status_;
};

void VertexSourceTest::SetUp() {
  error_status_ = new ErrorStatus(g_service_locator);
  pack_ = object_manager_->CreatePack();
}

void VertexSourceTest::TearDown() {
  pack_->Destroy();
  delete error_status_;
}

TEST_F(VertexSourceTest, BindStream) {
  static float some_vertices[][3] = {
    { 1.0f, 2.0f, 3.0f, },
    { 7.0f, 8.0f, 9.0f, },
    { 4.0f, 5.0f, 6.0f, },
  };
  const unsigned kNumVertices = arraysize(some_vertices);
  const unsigned kNumComponents = arraysize(some_vertices[0]);

  scoped_ptr<FakeVertexSource> destination(new FakeVertexSource(
      pack()->service_locator()));
  ASSERT_TRUE(destination != NULL);

  // Create 2 Buffers.
  SourceBuffer* destination_buffer_1 = pack()->Create<SourceBuffer>();
  SourceBuffer* destination_buffer_2 = pack()->Create<SourceBuffer>();
  ASSERT_TRUE(destination_buffer_1 != NULL);
  ASSERT_TRUE(destination_buffer_2 != NULL);

  // Create a field for each buffer.
  Field* destination_field_1 = destination_buffer_1->CreateField(
      FloatField::GetApparentClass(), kNumComponents);
  Field* destination_field_2 = destination_buffer_2->CreateField(
      FloatField::GetApparentClass(), kNumComponents);
  ASSERT_TRUE(destination_field_1 != NULL);
  ASSERT_TRUE(destination_field_2 != NULL);
  ASSERT_TRUE(destination_buffer_1->AllocateElements(kNumVertices));
  ASSERT_TRUE(destination_buffer_2->AllocateElements(kNumVertices));

  // Set up streams on primitive.
  EXPECT_TRUE(destination->SetVertexStream(Stream::POSITION,
                                           0,
                                           destination_field_1,
                                           0));
  EXPECT_TRUE(destination->SetVertexStream(Stream::POSITION,
                                           1,
                                           destination_field_2,
                                           0));

  // Create 2 source buffers.
  SourceBuffer* source_buffer_1 = pack()->Create<SourceBuffer>();
  SourceBuffer* source_buffer_2 = pack()->Create<SourceBuffer>();
  ASSERT_TRUE(source_buffer_1 != NULL);
  ASSERT_TRUE(source_buffer_2 != NULL);

  // Create a field for each buffer.
  Field* source_field_1 = source_buffer_1->CreateField(
      FloatField::GetApparentClass(), kNumComponents);
  Field* source_field_2 = source_buffer_2->CreateField(
      FloatField::GetApparentClass(), kNumComponents);
  ASSERT_TRUE(source_field_1 != NULL);
  ASSERT_TRUE(source_field_2 != NULL);
  ASSERT_TRUE(source_buffer_1->AllocateElements(kNumVertices));
  ASSERT_TRUE(source_buffer_2->AllocateElements(kNumVertices));


  // Put some vertices in the source streams.
  ASSERT_TRUE(source_buffer_1->AllocateElements(kNumVertices));
  source_field_1->SetFromFloats(&some_vertices[0][0], kNumComponents, 0,
                                kNumVertices);
  ASSERT_TRUE(source_buffer_2->AllocateElements(kNumVertices));
  source_field_2->SetFromFloats(&some_vertices[0][0], kNumComponents, 0,
                                kNumVertices);

  scoped_ptr<FakeVertexSource>source(new FakeVertexSource(
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
  EXPECT_TRUE(destination->BindStream(source.get(), Stream::POSITION, 0));
  EXPECT_TRUE(destination->BindStream(source.get(), Stream::POSITION, 1));
  // Non-existant streams should fail.
  EXPECT_FALSE(destination->BindStream(source.get(), Stream::POSITION, 2));
  EXPECT_FALSE(destination->BindStream(source.get(), Stream::COLOR, 10));

  // Cause the vertices to get updated.
  destination->UpdateStreams();

  // Test that vertices get updated.
  EXPECT_TRUE(CompareVertices(destination.get(),
                              Stream::POSITION,
                              0,
                              &some_vertices[0][0],
                              2.0f));
  EXPECT_TRUE(CompareVertices(destination.get(),
                              Stream::POSITION,
                              1,
                              &some_vertices[0][0],
                              3.0f));

  // Check that UpdateOutputs only got called once.
  EXPECT_EQ(destination->update_outputs_call_count(), 1);
  EXPECT_EQ(source->update_outputs_call_count(), 1);

  // Test that if we chain another VertexSource UpdateOutputs only gets called
  // once.
  SourceBuffer* source_buffer_1b = pack()->Create<SourceBuffer>();
  ASSERT_TRUE(source_buffer_1b != NULL);
  Field* source_field_1b = source_buffer_1b->CreateField(
      FloatField::GetApparentClass(), kNumComponents);
  ASSERT_TRUE(source_field_1b != NULL);

  // Put some vertices in the source streams.
  ASSERT_TRUE(source_buffer_1b->AllocateElements(kNumVertices));
  source_field_1b->SetFromFloats(&some_vertices[0][0], kNumComponents, 0,
                                 kNumVertices);

  scoped_ptr<FakeVertexSource>source_b(new FakeVertexSource(
      pack()->service_locator()));
  ASSERT_TRUE(source_b != NULL);

  // Set up streams on source
  EXPECT_TRUE(source_b->SetVertexStream(Stream::POSITION,
                                        0,
                                        source_field_1b,
                                        0));

  // Bind the vertices to streams.
  EXPECT_TRUE(source->BindStream(source_b.get(), Stream::POSITION, 0));

  // Cause the vertices to get updated.
  InvalidateAllParameters(pack());
  destination->UpdateStreams();

  // Test that vertices get updated.
  EXPECT_TRUE(CompareVertices(destination.get(),
                              Stream::POSITION,
                              0,
                              &some_vertices[0][0],
                              4.0f));
  // Test that these vertices stayed the same.
  EXPECT_TRUE(CompareVertices(destination.get(),
                              Stream::POSITION,
                              1,
                              &some_vertices[0][0],
                              3.0f));

  // Check that UpdateOutputs only got called once.
  EXPECT_EQ(destination->update_outputs_call_count(), 2);
  EXPECT_EQ(source->update_outputs_call_count(), 2);
  EXPECT_EQ(source_b->update_outputs_call_count(), 1);

  // Test that if we ask again UpdateOutputs does not get called.
  destination->UpdateStreams();
  EXPECT_EQ(destination->update_outputs_call_count(), 2);
  EXPECT_EQ(source->update_outputs_call_count(), 2);
  EXPECT_EQ(source_b->update_outputs_call_count(), 1);

  // Test that we can unbind.
  EXPECT_TRUE(source->UnbindStream(Stream::POSITION, 0));
  // Test that unbinding fails if we try a stream that does not exist.
  EXPECT_FALSE(source->UnbindStream(Stream::POSITION, 0));
}

}  // namespace o3d
