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


// This file contains the tests of class Serializer.

#include <vector>

#include "base/basictypes.h"
#include "core/cross/buffer.h"
#include "core/cross/curve.h"
#include "core/cross/matrix4_translation.h"
#include "core/cross/object_manager.h"
#include "core/cross/pack.h"
#include "core/cross/param_array.h"
#include "core/cross/primitive.h"
#include "core/cross/service_dependency.h"
#include "core/cross/shape.h"
#include "core/cross/skin.h"
#include "core/cross/texture.h"
#include "core/cross/transform.h"
#include "import/cross/memory_buffer.h"
#include "import/cross/memory_stream.h"
#include "serializer/cross/serializer.h"
#include "serializer/cross/serializer_binary.h"
#include "serializer/cross/version.h"
#include "tests/common/win/testing_common.h"
#include "utils/cross/string_writer.h"
#include "utils/cross/json_writer.h"

using std::vector;

namespace o3d {

struct AddFileRecord {
  String file_name_;
  size_t file_size_;
  vector<uint8> file_contents_;
};

class MockArchiveGenerator : public IArchiveGenerator {
 public:
  virtual void AddFile(const String& file_name,
                       size_t file_size) {
    AddFileRecord record;
    record.file_name_ = file_name;
    record.file_size_ = file_size;
    add_file_records_.push_back(record);
  }

  virtual int AddFileBytes(MemoryReadStream* stream, size_t numBytes) {
    const uint8* direct_pointer = stream->GetDirectMemoryPointer();
    add_file_records_.back().file_contents_.insert(
        add_file_records_.back().file_contents_.begin(),
        direct_pointer,
        direct_pointer + numBytes);
    return 0;
  }

  vector<AddFileRecord> add_file_records_;
};

class SerializerTest : public testing::Test {
 protected:
  SerializerTest()
      : object_manager_(g_service_locator),
        output_(StringWriter::CR_LF),
        json_writer_(&output_, 2),
        serializer_(g_service_locator, &json_writer_, &archive_generator_) {
    json_writer_.BeginCompacting();
  }

  virtual void SetUp() {
    pack_ = object_manager_->CreatePack();
  }

  virtual void TearDown() {
    object_manager_->DestroyPack(pack_);
  }

  ServiceDependency<ObjectManager> object_manager_;
  Pack* pack_;
  StringWriter output_;
  JsonWriter json_writer_;
  MockArchiveGenerator archive_generator_;
  Serializer serializer_;
};

TEST_F(SerializerTest,
       ShouldSerializeParamWithOnlyValueIfItIsNotBoundOrLateAdded) {
  Transform* transform = pack_->Create<Transform>();
  ParamMatrix4* param = transform->GetParam<ParamMatrix4>("localMatrix");
  param->set_value(Matrix4::identity());
  serializer_.SerializeParam(param);
  EXPECT_EQ("{\"value\":[[1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1]]}",
            output_.ToString());
}

TEST_F(SerializerTest, ShouldSerializeClassOfLateAddedParams) {
  Transform* transform = pack_->Create<Transform>();
  ParamInteger* param = transform->CreateParam<ParamInteger>("param");
  param->set_value(7);
  serializer_.SerializeParam(param);
  EXPECT_EQ("{\"class\":\"" O3D_NAMESPACE ".ParamInteger\",\"value\":7}",
            output_.ToString());
}

TEST_F(SerializerTest, ShouldSerializeIdOfOutputParams) {
  Transform* transform = pack_->Create<Transform>();
  ParamInteger* param = transform->CreateParam<ParamInteger>("param");
  param->set_value(7);
  ParamInteger* other_param = transform->CreateParam<ParamInteger>(
      "other_param");
  other_param->Bind(param);
  serializer_.SerializeParam(param);

  StringWriter expected(StringWriter::CR_LF);
  expected.WriteFormatted(
      "{\"class\":\"" O3D_NAMESPACE
      ".ParamInteger\",\"id\":%d,\"value\":7}",
      param->id());

  EXPECT_EQ(expected.ToString(), output_.ToString());
}

TEST_F(SerializerTest, ShouldSerializeInputIdOfBoundParams) {
  Transform* transform = pack_->Create<Transform>();
  ParamInteger* param = transform->CreateParam<ParamInteger>("param");
  ParamInteger* other_param = transform->CreateParam<ParamInteger>(
      "other_param");
  param->Bind(other_param);
  serializer_.SerializeParam(param);

  StringWriter expected(StringWriter::CR_LF);
  expected.WriteFormatted(
      "{\"class\":\"" O3D_NAMESPACE ".ParamInteger\",\"bind\":%d}",
      other_param->id());

  EXPECT_EQ(expected.ToString(), output_.ToString());
}

TEST_F(SerializerTest, SerializesPointer) {
  Transform* transform = pack_->Create<Transform>();
  Serialize(&json_writer_, transform);
  StringWriter expected(StringWriter::CR_LF);
  expected.WriteFormatted("{\"ref\":%d}", transform->id());
  EXPECT_EQ(expected.ToString(), output_.ToString());
}

TEST_F(SerializerTest, ShouldSerializeNullPointerToNullKeyword) {
  Serialize(&json_writer_, static_cast<ObjectBase*>(NULL));
  EXPECT_EQ("null", output_.ToString());
}

TEST_F(SerializerTest, SerializesFloat) {
  Serialize(&json_writer_, 1.25f);
  EXPECT_EQ("1.25", output_.ToString());
}

TEST_F(SerializerTest, SerializesFloat2) {
  Serialize(&json_writer_, Float2(1.25f, 2.5f));
  EXPECT_EQ("[1.25,2.5]", output_.ToString());
}

TEST_F(SerializerTest, SerializesFloat3) {
  Serialize(&json_writer_, Float3(1.25f, 2.5f, 5.0f));
  EXPECT_EQ("[1.25,2.5,5]", output_.ToString());
}

TEST_F(SerializerTest, SerializesFloat4) {
  Serialize(&json_writer_, Float4(1.25f, 2.5f, 5.0f, 10.0f));
  EXPECT_EQ("[1.25,2.5,5,10]", output_.ToString());
}

TEST_F(SerializerTest, SerializesInteger) {
  Serialize(&json_writer_, 7);
  EXPECT_EQ("7", output_.ToString());
}

TEST_F(SerializerTest, SerializesBoolean) {
  Serialize(&json_writer_, false);
  EXPECT_EQ("false", output_.ToString());
}

TEST_F(SerializerTest, SerializesString) {
  Serialize(&json_writer_, String("hello"));
  EXPECT_EQ("\"hello\"", output_.ToString());
}

TEST_F(SerializerTest, SerializesMatrix4) {
  Serialize(&json_writer_, Matrix4::identity());
  EXPECT_EQ("[[1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1]]", output_.ToString());
}

TEST_F(SerializerTest, SerializesValidBoundingBox) {
  BoundingBox bounding_box(Point3(-1.0f, -2.0f, -3.0f),
                           Point3(1.0f, 2.0f, 3.0f));
  Serialize(&json_writer_, bounding_box);
  EXPECT_EQ("[[-1,-2,-3],[1,2,3]]", output_.ToString());
}

TEST_F(SerializerTest, SerializesInvalidBoundingBox) {
  BoundingBox bounding_box;
  Serialize(&json_writer_, bounding_box);
  EXPECT_EQ("[]", output_.ToString());
}

TEST_F(SerializerTest, ShouldSerializeStreamProperties) {
  VertexBuffer* buffer = pack_->Create<VertexBuffer>();
  StreamBank* streamBank = pack_->Create<StreamBank>();
  Field* field = buffer->CreateField(FloatField::GetApparentClass(), 3);
  streamBank->SetVertexStream(Stream::NORMAL, 9, field, 1);
  const Stream* stream = streamBank->GetVertexStream(Stream::NORMAL, 9);
  Serialize(&json_writer_, *stream);
  StringWriter expected(StringWriter::CR_LF);
  expected.WriteFormatted(
      "{\"field\":%d,\"startIndex\":1,"
      "\"semantic\":2,\"semanticIndex\":9}",
      field->id());
  EXPECT_EQ(expected.ToString(), output_.ToString());
}

TEST_F(SerializerTest, ShouldSerializeCurveProperties) {
  Curve* curve = pack_->Create<Curve>();
  curve->set_pre_infinity(Curve::CONSTANT);
  curve->set_post_infinity(Curve::LINEAR);
  curve->set_use_cache(true);
  curve->SetSampleRate(0.1f);

  serializer_.SerializeSection(curve, Serializer::PROPERTIES_SECTION);

  EXPECT_EQ(
      "\"preInfinity\":0,"
      "\"postInfinity\":1,"
      "\"useCache\":true,"
      "\"sampleRate\":0.1",
      output_.ToString());
}

TEST_F(SerializerTest, ShouldSerializeCurveCustomSection) {
  // The purpose of this buffer is just to offset the following one in the
  // binary file.
  Curve* firstCurve = pack_->Create<Curve>();
  firstCurve->Create<StepCurveKey>();

  Curve* curve = pack_->Create<Curve>();

  StepCurveKey* step_key = curve->Create<StepCurveKey>();
  step_key->SetInput(1);
  step_key->SetOutput(2);

  LinearCurveKey* linear_key = curve->Create<LinearCurveKey>();
  linear_key->SetInput(3);
  linear_key->SetOutput(4);

  BezierCurveKey* bezier_key = curve->Create<BezierCurveKey>();
  bezier_key->SetInput(5);
  bezier_key->SetInTangent(Float2(6, 7));
  bezier_key->SetOutput(8);
  bezier_key->SetOutTangent(Float2(9, 10));

  serializer_.SerializePackBinary(pack_);
  serializer_.SerializeSection(curve, Serializer::CUSTOM_SECTION);

  // Make sure binaryRange is correct
  MemoryBuffer<uint8> contents1;
  MemoryBuffer<uint8> contents2;
  SerializeCurve(*firstCurve, &contents1);
  SerializeCurve(*curve, &contents2);
  size_t length1 = contents1.GetLength();
  size_t length2 = contents2.GetLength();

  StringWriter expected(StringWriter::CR_LF);
  expected.WriteFormatted(
      "\"binaryRange\":[%d,%d]",
      length1,
      length1 + length2);

  EXPECT_EQ(expected.ToString(), output_.ToString());
}

TEST_F(SerializerTest, ShouldSerializeCurveKeysToSingleBinaryFile) {
  // The purpose of this buffer is just to offset the following one in the
  // binary file.
  Curve* curve1 = pack_->Create<Curve>();
  StepCurveKey* step_key = curve1->Create<StepCurveKey>();
  step_key->SetInput(1);
  step_key->SetOutput(2);

  Curve* curve2 = pack_->Create<Curve>();
  LinearCurveKey* linear_key = curve2->Create<LinearCurveKey>();
  linear_key->SetInput(3);
  linear_key->SetOutput(4);

  serializer_.SerializePack(pack_);
  EXPECT_EQ(1, archive_generator_.add_file_records_.size());
  const AddFileRecord& record = archive_generator_.add_file_records_[0];

  EXPECT_EQ("curve-keys.bin", record.file_name_);

  // Test that the data matches what we get if we call SerializeCurve directly
  // The file should contain the concatenated contents of both curves
  MemoryBuffer<uint8> contents1;
  MemoryBuffer<uint8> contents2;
  SerializeCurve(*curve1, &contents1);
  SerializeCurve(*curve2, &contents2);
  size_t length1 = contents1.GetLength();
  size_t length2 = contents2.GetLength();
  size_t total_length = length1 + length2;
  ASSERT_EQ(total_length, record.file_size_);
  ASSERT_EQ(total_length, record.file_contents_.size());

  uint8 *p1 = contents1;
  uint8 *p2 = contents2;
  const uint8* data = &record.file_contents_[0];

  // Validate that first part of file data matches curve1 serialization
  // and that second part matches curve2...
  EXPECT_EQ(0, memcmp(p1, data, length1));
  EXPECT_EQ(0, memcmp(p2, data + length1, length2));
}

TEST_F(SerializerTest, ShouldSerializeNoCurvesIfCustomSectionIfNoKeys) {
  Curve* curve = pack_->Create<Curve>();
  serializer_.SerializeSection(curve, Serializer::CUSTOM_SECTION);
  EXPECT_EQ("\"binaryRange\":[0,0]", output_.ToString());
}

TEST_F(SerializerTest, SerializesIndexBuffer) {
  // The purpose of this buffer is just to offset the following one in the
  // binary file.
  Buffer* firstBuffer = pack_->Create<IndexBuffer>();
  firstBuffer->AllocateElements(1);

  Buffer* buffer = pack_->Create<IndexBuffer>();
  buffer->AllocateElements(2);
  {
    BufferLockHelper locker(buffer);
    uint32* data = locker.GetDataAs<uint32>(Buffer::WRITE_ONLY);
    data[0] = 3;
    data[1] = 7;
  }

  serializer_.SerializePackBinary(pack_);
  serializer_.SerializeSection(buffer, Serializer::CUSTOM_SECTION);

  // Make sure binaryRange is correct
  MemoryBuffer<uint8> contents1;
  MemoryBuffer<uint8> contents2;
  SerializeBuffer(*firstBuffer, &contents1);
  SerializeBuffer(*buffer, &contents2);
  size_t length1 = contents1.GetLength();
  size_t length2 = contents2.GetLength();

  StringWriter expected(StringWriter::CR_LF);
  expected.WriteFormatted(
      "\"fields\":[%d],"
      "\"binaryRange\":[%d,%d]",
      buffer->fields()[0]->id(),
      length1,
      length1 + length2);

  EXPECT_EQ(expected.ToString(), output_.ToString());
}

TEST_F(SerializerTest, SerializesAllIndexBufferBinaryToSingleFileInArchive) {
  Buffer* buffer1 = pack_->Create<IndexBuffer>();
  buffer1->AllocateElements(2);
  {
    BufferLockHelper locker(buffer1);
    uint32* data = locker.GetDataAs<uint32>(Buffer::WRITE_ONLY);
    data[0] = 1;
    data[1] = 2;
  }

  Buffer* buffer2 = pack_->Create<IndexBuffer>();
  buffer2->AllocateElements(1);
  {
    BufferLockHelper locker(buffer2);
    uint32* data = locker.GetDataAs<uint32>(Buffer::WRITE_ONLY);
    data[0] = 3;
  }

  serializer_.SerializePack(pack_);
  EXPECT_EQ(1, archive_generator_.add_file_records_.size());
  const AddFileRecord& record = archive_generator_.add_file_records_[0];
  EXPECT_EQ("index-buffers.bin", record.file_name_);

  // Test that the data matches what we get if we call SerializeBuffer directly
  // The file should contain the concatenated contents of both buffers
  MemoryBuffer<uint8> contents1;
  MemoryBuffer<uint8> contents2;
  SerializeBuffer(*buffer1, &contents1);
  SerializeBuffer(*buffer2, &contents2);
  size_t length1 = contents1.GetLength();
  size_t length2 = contents2.GetLength();
  size_t total_length = length1 + length2;
  ASSERT_EQ(total_length, record.file_size_);
  ASSERT_EQ(total_length, record.file_contents_.size());

  uint8 *p1 = contents1;
  uint8 *p2 = contents2;
  const uint8* data = &record.file_contents_[0];

  // Validate that first part of file data matches buffer1 serialization
  // and that second part matches buffer2...
  EXPECT_EQ(0, memcmp(p1, data, length1));
  EXPECT_EQ(0, memcmp(p2, data + length1, length2));
}

namespace {
class FakeNamedObject : public NamedObject {
 public:
  explicit FakeNamedObject(ServiceLocator* service_locator)
      : NamedObject(service_locator) {}
};
}

TEST_F(SerializerTest, ShouldSerializeNamedObjectProperties) {
  FakeNamedObject named_object(g_service_locator);
  named_object.set_name("ObjectName");
  serializer_.SerializeSection(&named_object, Serializer::PROPERTIES_SECTION);
  EXPECT_EQ("\"name\":\"ObjectName\"", output_.ToString());
}

TEST_F(SerializerTest, ShouldSerializeNoNameForNamelessNamedObjectProperties) {
  FakeNamedObject named_object(g_service_locator);
  serializer_.SerializeSection(&named_object, Serializer::PROPERTIES_SECTION);
  EXPECT_EQ("", output_.ToString());
}

TEST_F(SerializerTest, ShouldSerializeIdPropertiesAndCustomSections) {
  Curve* curve = pack_->Create<Curve>();
  curve->set_pre_infinity(Curve::CONSTANT);
  curve->set_post_infinity(Curve::LINEAR);
  curve->set_use_cache(true);
  curve->SetSampleRate(0.1f);

  serializer_.SerializeObject(curve);

  StringWriter expected(StringWriter::CR_LF);
  expected.WriteFormatted(
      "\"id\":%d,"
      "\"properties\":{"
      "\"preInfinity\":0,"
      "\"postInfinity\":1,"
      "\"useCache\":true,"
      "\"sampleRate\":0.1"
      "},"
      "\"custom\":{"
      "\"binaryRange\":[0,0]"
      "}",
      curve->id());
  EXPECT_EQ(expected.ToString(), output_.ToString());
}

namespace {
class FakeObjectBase : public ObjectBase {
 public:
  explicit FakeObjectBase(ServiceLocator* service_locator)
      : ObjectBase(service_locator) {
  }
};
}

TEST_F(SerializerTest,
       ShouldNotSerializePropertiesAndCustomSectionsIfTheyAreNotUsed) {
  FakeObjectBase object(g_service_locator);
  serializer_.SerializeObject(&object);

  StringWriter expected(StringWriter::CR_LF);
  expected.WriteFormatted(
      "\"id\":%d",
      object.id());
  EXPECT_EQ(expected.ToString(), output_.ToString());
}

TEST_F(SerializerTest, SerializesEmptyPack) {
  pack_->set_name("MyPack");

  Transform* root = pack_->Create<Transform>();
  root->set_name(String(Serializer::ROOT_PREFIX) + String("root"));

  serializer_.SerializePack(pack_);

  StringWriter expected(StringWriter::CR_LF);
  expected.WriteFormatted(
      "{"
        "\"version\":%d,"
        "\"o3d_rootObject_root\":%d,"
        "\"objects\":{}"
      "}",
      kSerializerVersion,
      root->id());
  EXPECT_EQ(expected.ToString(), output_.ToString());
}

TEST_F(SerializerTest, SerializesObjectsInPackExceptRootGroupedByClass) {
  FunctionEval* object1 = pack_->Create<FunctionEval>();
  object1->set_name("Object1");

  FunctionEval* object2 = pack_->Create<FunctionEval>();
  object2->set_name("Object2");

  Transform* root = pack_->Create<Transform>();
  root->set_name(String(Serializer::ROOT_PREFIX) + String("root"));

  serializer_.SerializePack(pack_);

  StringWriter expected(StringWriter::CR_LF);
  expected.WriteFormatted(
      "{"
        "\"version\":%d,"
        "\"o3d_rootObject_root\":%d,"
        "\"objects\":{"
          "\"" O3D_NAMESPACE ".FunctionEval\":["
            "{"
              "\"id\":%d,"
              "\"properties\":{"
                "\"name\":\"Object1\""
              "},"
              "\"params\":{"
                "\"o3d.functionObject\":{\"value\":null},"
                "\"o3d.input\":{\"value\":0}"
              "}"
            "},"
            "{"
              "\"id\":%d,"
              "\"properties\":{"
                "\"name\":\"Object2\""
              "},"
              "\"params\":{"
                "\"o3d.functionObject\":{\"value\":null},"
                "\"o3d.input\":{\"value\":0}"
              "}"
            "}"
          "]"
        "}"
      "}",
      kSerializerVersion,
      root->id(),
      object1->id(),
      object2->id());
  EXPECT_EQ(expected.ToString(), output_.ToString());
}

TEST_F(SerializerTest, SerializesNonDynamicParams) {
  Matrix4Translation* translation = pack_->Create<Matrix4Translation>();
  translation->set_input_matrix(Matrix4::identity());
  translation->set_translation(Float3(1, 2, 3));

  serializer_.SerializeObject(translation);

  StringWriter expected(StringWriter::CR_LF);
  expected.WriteFormatted(
      "\"id\":%d,"
      "\"properties\":{"
      "},"
      "\"params\":{"
        "\"o3d.inputMatrix\":{"
          "\"value\":[[1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1]]},"
        "\"o3d.translation\":{\"value\":[1,2,3]}"
      "}",
      translation->id());

  EXPECT_EQ(expected.ToString(), output_.ToString());
}

TEST_F(SerializerTest, SerializesDynamicParamsIfTheyAreOutputs) {
  Matrix4Translation* translation = pack_->Create<Matrix4Translation>();
  Param* input_matrix = translation->GetUntypedParam(
      Matrix4Translation::kInputMatrixParamName);
  Param* output_matrix = translation->GetUntypedParam(
      Matrix4Translation::kOutputMatrixParamName);
  input_matrix->Bind(output_matrix);

  serializer_.SerializeObject(translation);

  StringWriter expected(StringWriter::CR_LF);
  expected.WriteFormatted(
      "\"id\":%d,"
      "\"properties\":{"
      "},"
      "\"params\":{"
        "\"o3d.inputMatrix\":{\"bind\":%d},"
        "\"o3d.outputMatrix\":{\"id\":%d},"
        "\"o3d.translation\":{\"value\":[0,0,0]}"
      "}",
      translation->id(),
      output_matrix->id(),
      output_matrix->id());

  EXPECT_EQ(expected.ToString(), output_.ToString());
}

TEST_F(SerializerTest, SerializesParamArray) {
  ParamArray* param_array = pack_->Create<ParamArray>();
  param_array->CreateParam<ParamInteger>(0)->set_value(1);
  param_array->CreateParam<ParamInteger>(1)->set_value(2);
  serializer_.SerializeObject(param_array);
  StringWriter expected(StringWriter::CR_LF);
  expected.WriteFormatted(
      "\"id\":%d,"
      "\"properties\":{},"
      "\"params\":[{\"class\":\"" O3D_NAMESPACE
        ".ParamInteger\",\"value\":1},"
        "{\"class\":\"" O3D_NAMESPACE
        ".ParamInteger\",\"value\":2}]",
      param_array->id());
  EXPECT_EQ(expected.ToString(), output_.ToString());
}

TEST_F(SerializerTest, ShouldSerializePrimitiveProperties) {
  Primitive* primitive = pack_->Create<Primitive>();

  Shape* shape = pack_->Create<Shape>();
  primitive->SetOwner(shape);

  IndexBuffer* index_buffer = pack_->Create<IndexBuffer>();
  primitive->set_index_buffer(index_buffer);

  primitive->set_start_index(4);
  primitive->set_primitive_type(Primitive::LINELIST);
  primitive->set_number_vertices(8);
  primitive->set_number_primitives(9);

  serializer_.SerializeSection(primitive, Serializer::PROPERTIES_SECTION);

  StringWriter expected(StringWriter::CR_LF);
  expected.WriteFormatted(
      "\"owner\":{\"ref\":%d},"
      "\"numberVertices\":8,"
      "\"numberPrimitives\":9,"
      "\"primitiveType\":2,"
      "\"indexBuffer\":{\"ref\":%d},"
      "\"startIndex\":4",
      shape->id(),
      index_buffer->id());
  EXPECT_EQ(expected.ToString(), output_.ToString());
}

TEST_F(SerializerTest, ShouldSerializeShapeProperties) {
  Shape* shape = pack_->Create<Shape>();

  Element* element1 = pack_->Create<Primitive>();
  shape->AddElement(element1);

  Element* element2 = pack_->Create<Primitive>();
  shape->AddElement(element2);

  serializer_.SerializeSection(shape, Serializer::PROPERTIES_SECTION);

  StringWriter expected(StringWriter::CR_LF);
  expected.WriteFormatted(
      "\"elements\":[{\"ref\":%d},{\"ref\":%d}]",
      element1->id(),
      element2->id());
  EXPECT_EQ(expected.ToString(), output_.ToString());
}

TEST_F(SerializerTest, ShouldSerializeSkinProperties) {
  Skin* skin = pack_->Create<Skin>();
  skin->SetInverseBindPoseMatrix(0, Matrix4::identity());

  serializer_.SerializeSection(skin, Serializer::PROPERTIES_SECTION);

  EXPECT_EQ(
      "\"inverseBindPoseMatrices\":[[[1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1]]]",
      output_.ToString());
}

TEST_F(SerializerTest, ShouldSerializeSkinCustomSection) {
  Skin* skin1 = pack_->Create<Skin>();
  Skin::Influences influences(1);
  influences[0] = Skin::Influence(1, 2);
  skin1->SetVertexInfluences(0, influences);

  Skin* skin2 = pack_->Create<Skin>();
  influences[0] = Skin::Influence(3, 4);
  skin2->SetVertexInfluences(1, influences);

  serializer_.SerializePackBinary(pack_);
  serializer_.SerializeSection(skin2, Serializer::CUSTOM_SECTION);

  // Make sure binaryRange is correct
  MemoryBuffer<uint8> contents1;
  MemoryBuffer<uint8> contents2;
  SerializeSkin(*skin1, &contents1);
  SerializeSkin(*skin2, &contents2);
  size_t length1 = contents1.GetLength();
  size_t length2 = contents2.GetLength();

  StringWriter expected(StringWriter::CR_LF);
  expected.WriteFormatted(
      "\"binaryRange\":[%d,%d]",
      length1,
      length1 + length2);
  EXPECT_EQ(expected.ToString(), output_.ToString());
}

TEST_F(SerializerTest, ShouldSerializeSkinToSingleBinaryFile) {
  Skin* skin1 = pack_->Create<Skin>();
  Skin::Influences influences(1);
  influences[0] = Skin::Influence(1, 2);
  skin1->SetVertexInfluences(0, influences);
  skin1->SetInverseBindPoseMatrix(0, Matrix4::identity());

  Skin* skin2 = pack_->Create<Skin>();
  influences[0] = Skin::Influence(3, 4);
  skin2->SetVertexInfluences(1, influences);
  skin2->SetInverseBindPoseMatrix(0, Matrix4::identity());

  serializer_.SerializePack(pack_);
  EXPECT_EQ(1, archive_generator_.add_file_records_.size());
  const AddFileRecord& record = archive_generator_.add_file_records_[0];
  EXPECT_EQ("skins.bin", record.file_name_);

  // Test that the data matches what we get if we call SerializeSkin directly
  // The file should contain the concatenated contents of both skins
  MemoryBuffer<uint8> contents1;
  MemoryBuffer<uint8> contents2;
  SerializeSkin(*skin1, &contents1);
  SerializeSkin(*skin2, &contents2);
  size_t length1 = contents1.GetLength();
  size_t length2 = contents2.GetLength();
  size_t total_length = length1 + length2;
  ASSERT_EQ(total_length, record.file_size_);
  ASSERT_EQ(total_length, record.file_contents_.size());

  uint8 *p1 = contents1;
  uint8 *p2 = contents2;
  const uint8* data = &record.file_contents_[0];

  // Validate that first part of file data matches skin1 serialization
  // and that second part matches skin2...
  EXPECT_EQ(0, memcmp(p1, data, length1));
  EXPECT_EQ(0, memcmp(p2, data + length1, length2));
}

TEST_F(SerializerTest, ShouldSerializeSkinEval) {
  SkinEval* skin_eval = pack_->Create<SkinEval>();
  SourceBuffer* buffer1 = pack_->Create<SourceBuffer>();
  SourceBuffer* buffer2 = pack_->Create<SourceBuffer>();
  Field* field1 = buffer1->CreateField(FloatField::GetApparentClass(), 3);
  Field* field2 = buffer2->CreateField(FloatField::GetApparentClass(), 3);
  skin_eval->SetVertexStream(Stream::POSITION,
                             0,
                             field1,
                             0);
  skin_eval->SetVertexStream(Stream::NORMAL,
                             1,
                             field2,
                             0);

  serializer_.SerializeSection(skin_eval, Serializer::CUSTOM_SECTION);
  StringWriter expected(StringWriter::CR_LF);
  expected.WriteFormatted(
      "\"vertexStreams\":["
        "{"
          "\"stream\":{"
            "\"field\":%d,"
            "\"startIndex\":0,"
            "\"semantic\":1,"
            "\"semanticIndex\":0"
          "}"
        "},"
        "{"
          "\"stream\":{"
            "\"field\":%d,"
            "\"startIndex\":0,"
            "\"semantic\":2,"
            "\"semanticIndex\":1"
          "}"
        "}"
      "]",
      field1->id(),
      field2->id());
  EXPECT_EQ(expected.ToString(), output_.ToString());
}

TEST_F(SerializerTest, ShouldSerializeBoundSkinEval) {
  SkinEval* skin_eval1 = pack_->Create<SkinEval>();
  SourceBuffer* buffer1 = pack_->Create<SourceBuffer>();
  Field* field1 = buffer1->CreateField(FloatField::GetApparentClass(), 3);
  skin_eval1->SetVertexStream(Stream::POSITION,
                              0,
                              field1,
                              0);

  SkinEval* skin_eval2 = pack_->Create<SkinEval>();
  SourceBuffer* buffer2 = pack_->Create<SourceBuffer>();
  Field* field2 = buffer2->CreateField(FloatField::GetApparentClass(), 3);
  skin_eval2->SetVertexStream(Stream::POSITION,
                              0,
                              field2,
                              0);

  skin_eval1->BindStream(skin_eval2, Stream::POSITION, 0);
  ParamVertexBufferStream* param = skin_eval2->GetVertexStreamParam(
      Stream::POSITION, 0);

  serializer_.SerializeSection(skin_eval1, Serializer::CUSTOM_SECTION);
  StringWriter expected(StringWriter::CR_LF);
  expected.WriteFormatted(
      "\"vertexStreams\":["
        "{"
          "\"stream\":{"
            "\"field\":%d,"
            "\"startIndex\":0,"
            "\"semantic\":1,"
            "\"semanticIndex\":0"
          "},"
          "\"bind\":%d"
        "}"
      "]",
      field1->id(),
      skin_eval2->id());
  EXPECT_EQ(expected.ToString(), output_.ToString());
}

TEST_F(SerializerTest, ShouldSerializeStreamBank) {
  StreamBank* stream_bank = pack_->Create<StreamBank>();
  VertexBuffer* vertex_buffer_1 = pack_->Create<VertexBuffer>();
  Field* field_1 = vertex_buffer_1->CreateField(FloatField::GetApparentClass(),
                                                3);
  VertexBuffer* vertex_buffer_2 = pack_->Create<VertexBuffer>();
  Field* field_2 = vertex_buffer_1->CreateField(FloatField::GetApparentClass(),
                                                3);
  stream_bank->SetVertexStream(Stream::POSITION,
                               0,
                               field_1,
                               0);
  stream_bank->SetVertexStream(Stream::NORMAL,
                               1,
                               field_2,
                               0);

  serializer_.SerializeSection(stream_bank, Serializer::CUSTOM_SECTION);
  StringWriter expected(StringWriter::CR_LF);
  expected.WriteFormatted(
      "\"vertexStreams\":["
        "{"
          "\"stream\":{"
            "\"field\":%d,"
            "\"startIndex\":0,"
            "\"semantic\":1,"
            "\"semanticIndex\":0"
          "}"
        "},"
        "{"
          "\"stream\":{"
            "\"field\":%d,"
            "\"startIndex\":0,"
            "\"semantic\":2,"
            "\"semanticIndex\":1"
          "}"
        "}"
      "]",
      field_1->id(),
      field_2->id());
  EXPECT_EQ(expected.ToString(), output_.ToString());
}

TEST_F(SerializerTest, ShouldSerializeBoundStreamBank) {
  SkinEval* skin_eval = pack_->Create<SkinEval>();
  SourceBuffer* source_buffer = pack_->Create<SourceBuffer>();
  Field* source_field = source_buffer->CreateField(
      FloatField::GetApparentClass(), 3);
  skin_eval->SetVertexStream(Stream::POSITION,
                             0,
                             source_field,
                             0);

  StreamBank* stream_bank = pack_->Create<StreamBank>();
  VertexBuffer* vertex_buffer = pack_->Create<VertexBuffer>();
  Field* vertex_field = vertex_buffer->CreateField(
      FloatField::GetApparentClass(), 3);
  stream_bank->SetVertexStream(Stream::POSITION,
                               0,
                               vertex_field,
                               0);
  stream_bank->BindStream(skin_eval, Stream::POSITION, 0);

  serializer_.SerializeSection(stream_bank, Serializer::CUSTOM_SECTION);
  StringWriter expected(StringWriter::CR_LF);
  expected.WriteFormatted(
      "\"vertexStreams\":["
        "{"
          "\"stream\":{"
            "\"field\":%d,"
            "\"startIndex\":0,"
            "\"semantic\":1,"
            "\"semanticIndex\":0"
          "},"
          "\"bind\":%d"
        "}"
      "]",
      vertex_field->id(),
      skin_eval->id());
  EXPECT_EQ(expected.ToString(), output_.ToString());
}

TEST_F(SerializerTest, ShouldSerializeTexture2DCustomnSection) {
  Texture* texture = pack_->CreateTexture2D(256, 256, Texture::ARGB8,
                                            2, false);

  serializer_.SerializeSection(texture, Serializer::CUSTOM_SECTION);

  EXPECT_EQ(
      "\"width\":256,"
      "\"height\":256,"
      "\"format\":2,"
      "\"levels\":2,"
      "\"renderSurfacesEnabled\":false",
      output_.ToString());
}

TEST_F(SerializerTest, ShouldSerializeTextureCUBECustomnSection) {
  Texture* texture = pack_->CreateTextureCUBE(256, Texture::ARGB8,
                                              2, false);

  serializer_.SerializeSection(texture, Serializer::CUSTOM_SECTION);

  EXPECT_EQ(
      "\"edgeLength\":256,"
      "\"format\":2,"
      "\"levels\":2,"
      "\"renderSurfacesEnabled\":false",
      output_.ToString());
}

TEST_F(SerializerTest, ShouldSerializeTransformProperties) {
  Transform* transform = pack_->Create<Transform>();
  Transform* transform2 = pack_->Create<Transform>();
  ShapeArray shapes;
  Shape* shape1 = pack_->Create<Shape>();
  shapes.push_back(Shape::Ref(shape1));
  Shape* shape2 = pack_->Create<Shape>();
  shapes.push_back(Shape::Ref(shape2));
  transform->SetShapes(shapes);
  transform->SetParent(transform2);

  serializer_.SerializeSection(transform, Serializer::PROPERTIES_SECTION);

  StringWriter expected(StringWriter::CR_LF);
  expected.WriteFormatted(
      "\"shapes\":[{\"ref\":%d},{\"ref\":%d}],"
      "\"parent\":{\"ref\":%d}",
      shape1->id(),
      shape2->id(),
      transform2->id());

  EXPECT_EQ(expected.ToString(), output_.ToString());
}

TEST_F(SerializerTest, SerializesVertexBuffer) {
  // This buffer exists only to offset the second buffer in the buffer binary
  // file.
  Buffer* firstBuffer = pack_->Create<VertexBuffer>();
  firstBuffer->CreateField(FloatField::GetApparentClass(), 1);
  firstBuffer->AllocateElements(1);

  Buffer* buffer = pack_->Create<VertexBuffer>();
  Field* field = buffer->CreateField(FloatField::GetApparentClass(), 1);
  buffer->AllocateElements(2);
  {
    BufferLockHelper locker(buffer);
    float* data = locker.GetDataAs<float>(Buffer::WRITE_ONLY);
    data[0] = 1.25f;
    data[1] = -3.0f;
  }

  serializer_.SerializePackBinary(pack_);
  serializer_.SerializeSection(buffer, Serializer::CUSTOM_SECTION);

  // Make sure binaryRange is correct
  MemoryBuffer<uint8> contents1;
  MemoryBuffer<uint8> contents2;
  SerializeBuffer(*firstBuffer, &contents1);
  SerializeBuffer(*buffer, &contents2);
  size_t length1 = contents1.GetLength();
  size_t length2 = contents2.GetLength();

  StringWriter expected(StringWriter::CR_LF);
  expected.WriteFormatted(
      "\"fields\":[%d],"
      "\"binaryRange\":[%d,%d]",
      field->id(), length1, length1 + length2);

  EXPECT_EQ(expected.ToString(), output_.ToString());
}

TEST_F(SerializerTest, SerializesAllVertexBufferBinaryToSingleFileInArchive) {
  Buffer* buffer1 = pack_->Create<VertexBuffer>();
  buffer1->CreateField(FloatField::GetApparentClass(), 1);
  buffer1->AllocateElements(2);
  {
    BufferLockHelper locker(buffer1);
    float* data = locker.GetDataAs<float>(Buffer::WRITE_ONLY);
    data[0] = 1;
    data[1] = 2;
  }

  Buffer* buffer2 = pack_->Create<VertexBuffer>();
  buffer2->CreateField(FloatField::GetApparentClass(), 1);
  buffer2->AllocateElements(1);
  {
    BufferLockHelper locker(buffer2);
    float* data = locker.GetDataAs<float>(Buffer::WRITE_ONLY);
    data[0] = 3;
  }

  serializer_.SerializePack(pack_);
  EXPECT_EQ(1, archive_generator_.add_file_records_.size());
  const AddFileRecord& record = archive_generator_.add_file_records_[0];
  EXPECT_EQ("vertex-buffers.bin", record.file_name_);

  // Test that the data matches what we get if we call SerializeBuffer directly
  // The file should contain the concatenated contents of both buffers
  MemoryBuffer<uint8> contents1;
  MemoryBuffer<uint8> contents2;
  SerializeBuffer(*buffer1, &contents1);
  SerializeBuffer(*buffer2, &contents2);
  size_t length1 = contents1.GetLength();
  size_t length2 = contents2.GetLength();
  size_t total_length = length1 + length2;
  ASSERT_EQ(total_length, record.file_size_);
  ASSERT_EQ(total_length, record.file_contents_.size());

  uint8 *p1 = contents1;
  uint8 *p2 = contents2;
  const uint8* data = &record.file_contents_[0];

  // Validate that first part of file data matches buffer1 serialization
  // and that second part matches buffer2...
  EXPECT_EQ(0, memcmp(p1, data, length1));
  EXPECT_EQ(0, memcmp(p2, data + length1, length2));
}
}  // namespace o3d
