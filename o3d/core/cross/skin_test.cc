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


// Tests functionality of the Skin class

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/error.h"
#include "core/cross/skin.h"
#include "core/cross/primitive.h"
#include "import/cross/memory_buffer.h"
#include "import/cross/memory_stream.h"
#include "serializer/cross/serializer_binary.h"

namespace o3d {

namespace {

bool CompareInfluences(const Skin::Influences& lhs,
                       const Skin::Influences& rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (unsigned ii = 0; ii < lhs.size(); ++ii) {
    if (lhs[ii].matrix_index != rhs[ii].matrix_index ||
        lhs[ii].weight != rhs[ii].weight) {
      return false;
    }
  }
  return true;
}

const float kEpsilon = 0.00001f;

// Compares two Vector4's for equality.
bool CompareVector4s(const Vector4& v1, const Vector4& v2) {
  return fabs(v1.getElem(0) - v2.getElem(0)) < kEpsilon &&
      fabs(v1.getElem(1) - v2.getElem(1)) < kEpsilon &&
      fabs(v1.getElem(2) - v2.getElem(2)) < kEpsilon &&
      fabs(v1.getElem(3) - v2.getElem(3)) < kEpsilon;
}

// Compares two Matrix4's for equality.
bool CompareMatrix4s(const Matrix4& m1, const Matrix4& m2) {
  return CompareVector4s(m1.getCol0(), m2.getCol0()) &&
      CompareVector4s(m1.getCol1(), m2.getCol1()) &&
      CompareVector4s(m1.getCol2(), m2.getCol2()) &&
      CompareVector4s(m1.getCol3(), m2.getCol3());
}

bool CompareVertices(StreamBank* source,
                     Stream::Semantic semantic,
                     int semantic_index,
                     const float* values) {
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
  unsigned num_components = field.num_components();

  const float* source_values = PointerFromVoidPointer<const float*>(
      data, field.offset());
  while (num_vertices) {
    for (unsigned jj = 0; jj < num_components; ++jj) {
      float difference = fabsf(source_values[jj] - values[jj]);
      if (difference > kEpsilon) {
        return false;
      }
    }
    values = AddPointerOffset(values, stride);
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

}  // anonymous namespace.

// Test fixture for Skin testing.
class SkinTest : public testing::Test {
 protected:
  SkinTest()
      : object_manager_(g_service_locator) { }

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

  ServiceDependency<ObjectManager> object_manager_;

 private:
  Pack* pack_;
};

void SkinTest::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void SkinTest::TearDown() {
  pack_->Destroy();
}

// Test Skin
TEST_F(SkinTest, Basic) {
  Skin* skin = pack()->Create<Skin>();
  // Check that it got created.
  ASSERT_TRUE(skin != NULL);

  // Check that it inherits from what we expect it to.
  EXPECT_TRUE(skin->IsA(NamedObject::GetApparentClass()));
}

// Test GetVertexInfluences, SetVertexInfluences, GetHighestMatrixIndex,
// GetHighestInfluences
TEST_F(SkinTest, GetSetVertexInfluence) {
  Skin* skin = pack()->Create<Skin>();
  // Check that it got created.
  ASSERT_TRUE(skin != NULL);

  // Check Highests are 0
  const Skin::InfluencesArray& influences = skin->influences();
  ASSERT_EQ(influences.size(), 0);
  EXPECT_EQ(skin->GetHighestMatrixIndex(), 0);
  EXPECT_EQ(skin->GetHighestInfluences(), 0);

  // Add some influences.
  Skin::Influences no_influences;
  Skin::Influences influences_0;
  influences_0.push_back(Skin::Influence(1, 123.0f));
  influences_0.push_back(Skin::Influence(2, 456.0f));
  Skin::Influences influences_4;
  influences_4.push_back(Skin::Influence(4, 23.0f));
  influences_4.push_back(Skin::Influence(3, 56.0f));

  skin->SetVertexInfluences(0, influences_0);
  skin->SetVertexInfluences(4, influences_4);

  // Check they got set.
  ASSERT_EQ(influences.size(), 5);
  EXPECT_TRUE(CompareInfluences(influences[0], influences_0));
  EXPECT_TRUE(CompareInfluences(influences[1], no_influences));
  EXPECT_TRUE(CompareInfluences(influences[2], no_influences));
  EXPECT_TRUE(CompareInfluences(influences[3], no_influences));
  EXPECT_TRUE(CompareInfluences(influences[4], influences_4));

  // Check the limits
  EXPECT_EQ(skin->GetHighestMatrixIndex(), 4);
  EXPECT_EQ(skin->GetHighestInfluences(), 2);

  // Add a new influence.
  Skin::Influences influences_2;
  influences_2.push_back(Skin::Influence(9, 1.0f));
  influences_2.push_back(Skin::Influence(2, 2.0f));
  influences_2.push_back(Skin::Influence(3, 3.0f));

  // Check they got set.
  skin->SetVertexInfluences(2, influences_2);
  ASSERT_EQ(influences.size(), 5);
  EXPECT_TRUE(CompareInfluences(influences[0], influences_0));
  EXPECT_TRUE(CompareInfluences(influences[1], no_influences));
  EXPECT_TRUE(CompareInfluences(influences[2], influences_2));
  EXPECT_TRUE(CompareInfluences(influences[3], no_influences));
  EXPECT_TRUE(CompareInfluences(influences[4], influences_4));

  // Check the limits
  EXPECT_EQ(skin->GetHighestMatrixIndex(), 9);
  EXPECT_EQ(skin->GetHighestInfluences(), 3);

  // Add one past the end.
  skin->SetVertexInfluences(6, influences_4);
  ASSERT_EQ(influences.size(), 7);
  EXPECT_TRUE(CompareInfluences(influences[0], influences_0));
  EXPECT_TRUE(CompareInfluences(influences[1], no_influences));
  EXPECT_TRUE(CompareInfluences(influences[2], influences_2));
  EXPECT_TRUE(CompareInfluences(influences[3], no_influences));
  EXPECT_TRUE(CompareInfluences(influences[4], influences_4));
  EXPECT_TRUE(CompareInfluences(influences[5], no_influences));
  EXPECT_TRUE(CompareInfluences(influences[6], influences_4));

  // Check the limits
  EXPECT_EQ(skin->GetHighestMatrixIndex(), 9);
  EXPECT_EQ(skin->GetHighestInfluences(), 3);
}

TEST_F(SkinTest, GetSetInverseBindPoseMatrices) {
  Skin* skin = pack()->Create<Skin>();
  // Check that it got created.
  ASSERT_TRUE(skin != NULL);

  const Skin::MatrixArray& matrices = skin->inverse_bind_pose_matrices();
  EXPECT_EQ(matrices.size(), 0);

  Matrix4 matrix_2(Vector4(1.0f, 2.0f, 3.0f, 4.0f),
                   Vector4(2.0f, 3.0f, 4.0f, 6.0f),
                   Vector4(3.0f, 4.0f, 5.0f, 7.0f),
                   Vector4(4.0f, 5.0f, 6.0f, 8.0f));
  Matrix4 matrix_4(Vector4(1.0f, 2.0f, 3.0f, 4.0f),
                   Vector4(2.0f, 3.0f, 4.0f, 6.0f),
                   Vector4(3.0f, 4.0f, 5.0f, 7.0f),
                   Vector4(4.0f, 5.0f, 6.0f, 8.0f));

  skin->SetInverseBindPoseMatrix(2, matrix_2);
  EXPECT_EQ(matrices.size(), 3);
  EXPECT_TRUE(CompareMatrix4s(matrices[0], Matrix4::identity()));
  EXPECT_TRUE(CompareMatrix4s(matrices[1], Matrix4::identity()));
  EXPECT_TRUE(CompareMatrix4s(matrices[2], matrix_2));

  skin->SetInverseBindPoseMatrix(4, matrix_4);
  EXPECT_EQ(matrices.size(), 5);
  EXPECT_TRUE(CompareMatrix4s(matrices[0], Matrix4::identity()));
  EXPECT_TRUE(CompareMatrix4s(matrices[1], Matrix4::identity()));
  EXPECT_TRUE(CompareMatrix4s(matrices[2], matrix_2));
  EXPECT_TRUE(CompareMatrix4s(matrices[3], Matrix4::identity()));
  EXPECT_TRUE(CompareMatrix4s(matrices[4], matrix_4));
}

// Test fixture for SkinEval testing.
class SkinEvalTest : public testing::Test {
 protected:
  SkinEvalTest()
      : object_manager_(g_service_locator) { }

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

  ServiceDependency<ObjectManager> object_manager_;
  ErrorStatus* error_status_;

 private:
  Pack* pack_;
};

void SkinEvalTest::SetUp() {
  error_status_ = new ErrorStatus(g_service_locator);
  pack_ = object_manager_->CreatePack();
}

void SkinEvalTest::TearDown() {
  pack_->Destroy();
  delete error_status_;
}

// Test SkinEval
TEST_F(SkinEvalTest, Basic) {
  SkinEval* skin_eval = pack()->Create<SkinEval>();
  // Check that it got created.
  ASSERT_TRUE(skin_eval != NULL);

  // Check that it inherits from what we expect it to.
  EXPECT_TRUE(skin_eval->IsA(VertexSource::GetApparentClass()));

  // Check our params exist.
  EXPECT_TRUE(skin_eval->GetParam<ParamSkin>(
      SkinEval::kSkinParamName) != NULL);
  EXPECT_TRUE(skin_eval->GetParam<ParamParamArray>(
      SkinEval::kMatricesParamName) != NULL);
  EXPECT_TRUE(skin_eval->GetParam<ParamMatrix4>(
      SkinEval::kBaseParamName) != NULL);

  // Check our accessors
  EXPECT_TRUE(skin_eval->skin() == NULL);
  EXPECT_TRUE(skin_eval->matrices() == NULL);
  EXPECT_TRUE(CompareMatrix4s(skin_eval->base(), Matrix4::identity()));

  Skin* skin = pack()->Create<Skin>();
  ASSERT_TRUE(skin != NULL);
  skin_eval->set_skin(skin);
  ParamArray* param_array = pack()->Create<ParamArray>();
  ASSERT_TRUE(param_array != NULL);
  skin_eval->set_matrices(param_array);
  Matrix4 matrix(Vector4(1.0f, 2.0f, 3.0f, 4.0f),
                 Vector4(2.0f, 3.0f, 4.0f, 6.0f),
                 Vector4(3.0f, 4.0f, 5.0f, 7.0f),
                 Vector4(4.0f, 5.0f, 6.0f, 8.0f));
  skin_eval->set_base(matrix);

  EXPECT_EQ(skin_eval->skin(), skin);
  EXPECT_EQ(skin_eval->matrices(), param_array);
  EXPECT_TRUE(CompareMatrix4s(skin_eval->base(), matrix));
}

// Tests Binding Streams on a Skin.
TEST_F(SkinEvalTest, BindStreams) {
  SkinEval* skin_eval = pack()->Create<SkinEval>();
  Skin* skin = pack()->Create<Skin>();
  ParamArray* matrices = pack()->Create<ParamArray>();
  StreamBank* stream_bank = pack()->Create<StreamBank>();
  VertexBuffer* vertex_buffer = pack()->Create<VertexBuffer>();
  SourceBuffer* source_buffer = pack()->Create<SourceBuffer>();

  ASSERT_TRUE(skin_eval != NULL);
  ASSERT_TRUE(skin != NULL);
  ASSERT_TRUE(matrices != NULL);
  ASSERT_TRUE(stream_bank != NULL);

  Field* vertex_position_field = vertex_buffer->CreateField(
      FloatField::GetApparentClass(), 3);
  Field* vertex_normal_field = vertex_buffer->CreateField(
      FloatField::GetApparentClass(), 3);
  Field* vertex_texcoord_field = vertex_buffer->CreateField(
      FloatField::GetApparentClass(), 4);
  Field* source_position_field = source_buffer->CreateField(
      FloatField::GetApparentClass(), 3);
  Field* source_normal_field = source_buffer->CreateField(
      FloatField::GetApparentClass(), 3);
  Field* source_texcoord_field = source_buffer->CreateField(
      FloatField::GetApparentClass(), 4);

  ASSERT_TRUE(vertex_position_field != NULL);
  ASSERT_TRUE(vertex_normal_field != NULL);
  ASSERT_TRUE(vertex_texcoord_field != NULL);
  ASSERT_TRUE(source_position_field != NULL);
  ASSERT_TRUE(source_normal_field != NULL);
  ASSERT_TRUE(source_texcoord_field != NULL);

  skin_eval->set_skin(skin);
  skin_eval->set_matrices(matrices);

  static float vertices[] = {
    1.0f, 2.0f, 3.0f,
    1.0f, 0.0f, 0.0f,
    0.5f, 1.0f, 1.5f, 1.0f,

    4.0f, 5.0f, 6.0f,
    0.0f, 1.0f, 0.0f,
    0.5f, 2.0f, 1.5f, 1.0f,

    7.0f, 8.0f, 9.0f,
    0.0f, 0.0f, 1.0f,
    0.5f, 3.0f, 1.5f, 1.0f,
  };

  static float expected_vertices[] = {
    1.0f * 2.0f, 2.0f * 2.0f, 3.0f * 2.0f,
    1.0f * 2.0f, 0.0f * 2.0f, 0.0f * 2.0f,
    0.5f * 2.0f, 1.0f * 2.0f, 1.5f * 2.0f, 1.0f,

    (4.0f * 2.0f + 4.0f + 1.0f) / 2.0f,
    (5.0f * 2.0f + 5.0f + 2.0f) / 2.0f,
    (6.0f * 2.0f + 6.0f + 3.0f) / 2.0f,

    (0.0f * 2.0f + 0.0f) / 2.0f,
    (1.0f * 2.0f + 1.0f) / 2.0f,
    (0.0f * 2.0f + 0.0f) / 2.0f,

    (0.5f * 2.0f + 0.5f + 1.0f) / 2.0f,
    (2.0f * 2.0f + 2.0f + 2.0f) / 2.0f,
    (1.5f * 2.0f + 1.5f + 3.0f) / 2.0f,
    (1.0f * 1.0f + 1.0f + 0.0f) / 2.0f,

    7.0f + 4.0f, 8.0f + 5.0f, 9.0f + 6.0f,
    0.0f, 0.0f, 1.0f,
    0.5f + 4.0f, 3.0f + 5.0f, 1.5f + 6.0f, 1.0f,
  };

  const unsigned kStride = 10 * sizeof(vertices[0]);
  const unsigned kNumElements = 3;

  // Create the buffers.

  ASSERT_TRUE(vertex_buffer->AllocateElements(kNumElements));
  ASSERT_TRUE(source_buffer->AllocateElements(kNumElements));
  float* data;
  ASSERT_TRUE(source_buffer->LockAs(Buffer::WRITE_ONLY, &data));
  memcpy(data, &vertices, sizeof(vertices));
  ASSERT_TRUE(source_buffer->Unlock());

  // Setup the streams.
  ASSERT_TRUE(stream_bank->SetVertexStream(Stream::POSITION,
                                           0,
                                           vertex_position_field,
                                           0));
  ASSERT_TRUE(stream_bank->SetVertexStream(Stream::NORMAL,
                                           0,
                                           vertex_normal_field,
                                           0));
  ASSERT_TRUE(stream_bank->SetVertexStream(Stream::TEXCOORD,
                                           0,
                                           vertex_texcoord_field,
                                           0));

  ASSERT_TRUE(skin_eval->SetVertexStream(Stream::POSITION,
                                         0,
                                         source_position_field,
                                         0));
  ASSERT_TRUE(skin_eval->SetVertexStream(Stream::NORMAL,
                                         0,
                                         source_normal_field,
                                         0));
  ASSERT_TRUE(skin_eval->SetVertexStream(Stream::TEXCOORD,
                                         0,
                                         source_texcoord_field,
                                         0));

  // Bind the streams
  ASSERT_TRUE(stream_bank->BindStream(skin_eval, Stream::POSITION, 0));
  ASSERT_TRUE(stream_bank->BindStream(skin_eval, Stream::NORMAL, 0));
  ASSERT_TRUE(stream_bank->BindStream(skin_eval, Stream::TEXCOORD, 0));

  // Create the matrices
  ParamMatrix4* params[3];
  matrices->CreateParam<ParamMatrix4>(2);
  for (unsigned ii = 0; ii < 3; ++ii) {
    params[ii] = matrices->GetParam<ParamMatrix4>(ii);
    ASSERT_TRUE(params[ii] != NULL);
  }

  // Set the influences.
  Skin::Influences influences_0;
  influences_0.push_back(Skin::Influence(0, 1.0f));
  Skin::Influences influences_1;
  influences_1.push_back(Skin::Influence(0, 0.5f));
  influences_1.push_back(Skin::Influence(1, 0.5f));
  Skin::Influences influences_2;
  influences_2.push_back(Skin::Influence(1, 0.0f));
  influences_2.push_back(Skin::Influence(2, 1.0f));

  skin->SetVertexInfluences(0, influences_0);
  skin->SetVertexInfluences(1, influences_1);
  skin->SetVertexInfluences(2, influences_2);

  // Set the inverse bind pose matrices
  skin->SetInverseBindPoseMatrix(0, Matrix4::identity());
  skin->SetInverseBindPoseMatrix(1, Matrix4::identity());
  skin->SetInverseBindPoseMatrix(2, Matrix4::identity());

  // Cause the vertices to get updated.
  stream_bank->UpdateStreams();

  // Test that vertices get updated.
  EXPECT_TRUE(CompareVertices(stream_bank,
                              Stream::POSITION,
                              0,
                              vertices));
  EXPECT_TRUE(CompareVertices(stream_bank,
                              Stream::NORMAL,
                              0,
                              &vertices[3]));
  EXPECT_TRUE(CompareVertices(stream_bank,
                              Stream::TEXCOORD,
                              0,
                              &vertices[6]));

  // Move the matrixes and check again.
  params[0]->set_value(Matrix4(Vector4(2.0f, 0.0f, 0.0f, 0.0f),
                               Vector4(0.0f, 2.0f, 0.0f, 0.0f),
                               Vector4(0.0f, 0.0f, 2.0f, 0.0f),
                               Vector4(0.0f, 0.0f, 0.0f, 1.0f)));
  params[1]->set_value(Matrix4(Vector4(1.0f, 0.0f, 0.0f, 0.0f),
                               Vector4(0.0f, 1.0f, 0.0f, 0.0f),
                               Vector4(0.0f, 0.0f, 1.0f, 0.0f),
                               Vector4(1.0f, 2.0f, 3.0f, 1.0f)));
  params[2]->set_value(Matrix4(Vector4(1.0f, 0.0f, 0.0f, 0.0f),
                               Vector4(0.0f, 1.0f, 0.0f, 0.0f),
                               Vector4(0.0f, 0.0f, 1.0f, 0.0f),
                               Vector4(4.0f, 5.0f, 6.0f, 1.0f)));

  // Cause the vertices to get updated.
  InvalidateAllParameters(pack());
  stream_bank->UpdateStreams();

  // Test that vertices are correct.
  EXPECT_TRUE(CompareVertices(stream_bank,
                              Stream::POSITION,
                              0,
                              expected_vertices));
  EXPECT_TRUE(CompareVertices(stream_bank,
                              Stream::NORMAL,
                              0,
                              &expected_vertices[3]));
  EXPECT_TRUE(CompareVertices(stream_bank,
                              Stream::TEXCOORD,
                              0,
                              &expected_vertices[6]));

  // check if skin references something outside matrices
  influences_2.push_back(Skin::Influence(3, 1.0f));
  skin->SetVertexInfluences(2, influences_2);

  // Cause the vertices to get updated.
  InvalidateAllParameters(pack());
  EXPECT_TRUE(error_status_->GetLastError().empty());
  stream_bank->UpdateStreams();
  EXPECT_FALSE(error_status_->GetLastError().empty());

  // put it back
  influences_2.pop_back();
  skin->SetVertexInfluences(2, influences_2);

  error_status_->ClearLastError();
  InvalidateAllParameters(pack());
  EXPECT_TRUE(error_status_->GetLastError().empty());
  stream_bank->UpdateStreams();
  EXPECT_TRUE(error_status_->GetLastError().empty());

  // check if buffers are not the same size.
  ASSERT_TRUE(vertex_buffer->AllocateElements(kNumElements * 2));

  // Cause the vertices to get updated.
  error_status_->ClearLastError();
  InvalidateAllParameters(pack());
  EXPECT_TRUE(error_status_->GetLastError().empty());
  stream_bank->UpdateStreams();
  EXPECT_FALSE(error_status_->GetLastError().empty());

  // put it back
  ASSERT_TRUE(vertex_buffer->AllocateElements(kNumElements));

  // Cause the vertices to get updated.
  error_status_->ClearLastError();
  InvalidateAllParameters(pack());
  EXPECT_TRUE(error_status_->GetLastError().empty());
  stream_bank->UpdateStreams();
  EXPECT_TRUE(error_status_->GetLastError().empty());

  // check if skin # vertex doesn't match stream
  skin->SetVertexInfluences(3, influences_2);

  // Cause the vertices to get updated.
  error_status_->ClearLastError();
  InvalidateAllParameters(pack());
  EXPECT_TRUE(error_status_->GetLastError().empty());
  stream_bank->UpdateStreams();
  EXPECT_FALSE(error_status_->GetLastError().empty());
}


// Sanity check on empty data
TEST_F(SkinTest, SkinRawDataEmpty) {
  Skin* skin = pack()->Create<Skin>();
  ASSERT_TRUE(skin != NULL);

  uint8 p[2];
  MemoryReadStream read_stream(p, 0);  // empty stream

  bool success = skin->LoadFromBinaryData(&read_stream);

  // Make sure we don't like to load from empty data
  EXPECT_FALSE(success);
}

// Sanity check on corrupt data
TEST_F(SkinTest, SkinRawDataCorrupt) {
  Skin* skin = pack()->Create<Skin>();
  ASSERT_TRUE(skin != NULL);

  const int kDataLength = 512;  // enough storage for test
  MemoryBuffer<uint8> buffer(kDataLength);
  MemoryWriteStream write_stream(buffer, kDataLength);

  // write out serialization ID
  write_stream.Write(Skin::kSerializationID, 4);

  // write out version 5 (which is illegal version!)
  write_stream.WriteLittleEndianInt32(5);


  // make note of amount we've written
  size_t data_size = write_stream.GetStreamPosition();

  // try to load what we just created
  MemoryReadStream read_stream(buffer, data_size);
  bool success = skin->LoadFromBinaryData(&read_stream);

  // Make sure we don't like to load from empty data
  EXPECT_FALSE(success);
}

// Sanity check on incomplete data
TEST_F(SkinTest, SkinRawDataIncomplete) {
  Skin* skin = pack()->Create<Skin>();
  ASSERT_TRUE(skin != NULL);

  const int kDataLength = 512;  // enough storage for test
  MemoryBuffer<uint8> buffer(kDataLength);
  MemoryWriteStream write_stream(buffer, kDataLength);

  // write out serialization ID
  write_stream.Write(Skin::kSerializationID, 4);

  // write out version
  write_stream.WriteLittleEndianInt32(1);

  write_stream.WriteByte(3);  // bezier
  write_stream.WriteLittleEndianInt32(5);
  // but DON'T write the actual influences!

  // make note of amount we've written
  size_t data_size = write_stream.GetStreamPosition();

  // try to load what we just created
  MemoryReadStream read_stream(buffer, data_size);
  bool success = skin->LoadFromBinaryData(&read_stream);

  // Make sure we don't like to load from incomplete data
  EXPECT_FALSE(success);
}


// Check that valid skin data loads OK
TEST_F(SkinTest, SkinRawDataValid) {
  Skin* skin = pack()->Create<Skin>();
  ASSERT_TRUE(skin != NULL);

  const int kDataLength = 512;  // enough storage for test
  MemoryBuffer<uint8> buffer(kDataLength);
  MemoryWriteStream write_stream(buffer, kDataLength);

  // write out serialization ID
  write_stream.Write(Skin::kSerializationID, 4);

  // write out version
  write_stream.WriteLittleEndianInt32(1);


  // Write out some influence data
  const size_t kNumInfluences = 32;
  write_stream.WriteLittleEndianInt32(kNumInfluences);

  for (int i = 0; i < kNumInfluences; ++i) {
    write_stream.WriteLittleEndianInt32(i);
    write_stream.WriteLittleEndianFloat32(1.0f + 0.2f * static_cast<float>(i));
  }

  // Make note of amount we've written
  size_t data_size = write_stream.GetStreamPosition();

  // Try to load what we just created
  MemoryReadStream read_stream(buffer, data_size);
  bool success = skin->LoadFromBinaryData(&read_stream);

  // Make sure skin data was accepted
  EXPECT_TRUE(success);

  // Validate the influences
  const Skin::InfluencesArray& influences_array = skin->influences();
  EXPECT_EQ(1, influences_array.size());

  const Skin::Influences& influences = influences_array[0];
  EXPECT_EQ(kNumInfluences, influences.size());

  for (Skin::Influences::size_type i = 0; i < influences.size(); ++i) {
    const Skin::Influence &influence = influences[i];
    EXPECT_EQ(i, influence.matrix_index);
    float expected_weight = 1.0f + 0.2f * static_cast<float>(i);
    EXPECT_EQ(expected_weight, influence.weight);
  }

  // Now, let's try a very nice test to verify that we properly
  // serialize -- this is a round trip test
  MemoryBuffer<uint8> serialized_data;
  SerializeSkin(*skin, &serialized_data);

  // Make sure serialized data length is identical to what we made
  ASSERT_EQ(data_size, serialized_data.GetLength());

  // Make sure the data matches
  uint8 *original = buffer;
  uint8 *serialized = serialized_data;
  EXPECT_EQ(0, memcmp(original, serialized, data_size));
}

}  // namespace o3d
