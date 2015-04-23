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


// Tests VertexBuffer and IndexBuffer.

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/error_status.h"
#include "core/cross/buffer.h"
#include "import/cross/memory_buffer.h"
#include "import/cross/memory_stream.h"
#include "import/cross/raw_data.h"
#include "serializer/cross/serializer_binary.h"

namespace o3d {

namespace {

// Checks if change_count != buffer->field_change_count and updates
// change_count.
bool ChangeCountChanged(unsigned int* change_count, Buffer* buffer) {
  bool changed = *change_count != buffer->field_change_count();
  *change_count = buffer->field_change_count();
  return changed;
}

// Checks if an error has occured on the client then clears the error.
bool CheckErrorExists(IErrorStatus* error_status) {
  bool have_error = !error_status->GetLastError().empty();
  error_status->ClearLastError();
  return have_error;
}

// Compares 2 sets of floats. Returns true if they are the same.
bool CompareElements(const float* floats_1,
                     const float* floats_2,
                     unsigned num_elements,
                     unsigned num_components) {
  for (; num_elements; --num_elements) {
    for (unsigned ii = 0; ii < num_components; ++ii) {
      if (*floats_1 != *floats_2) {
        return false;
      }
      ++floats_1;
      ++floats_2;
    }
  }
  return true;
}

}  // anonymous namespace

class BufferTest : public testing::Test {
 protected:
  BufferTest()
      : object_manager_(g_service_locator),
        error_status_(g_service_locator) {
  }

  virtual void SetUp();
  virtual void TearDown();

  IErrorStatus* error_status() { return &error_status_; }
  Pack* pack() { return pack_; }

 private:
  ServiceDependency<ObjectManager> object_manager_;
  ErrorStatus error_status_;
  Pack* pack_;
};

void BufferTest::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void BufferTest::TearDown() {
  object_manager_->DestroyPack(pack_);
}

// Test Buffer.
TEST_F(BufferTest, TestBasic) {
  Buffer *buffer = pack()->Create<VertexBuffer>();
  const FieldRefArray& fields = buffer->fields();
  // Verify initial state.
  ASSERT_TRUE(buffer->IsA(Buffer::GetApparentClass()));
  EXPECT_EQ(buffer->num_elements(), 0);
  EXPECT_EQ(fields.size(), 0);
  EXPECT_EQ(buffer->stride(), 0);
  EXPECT_EQ(buffer->GetSizeInBytes(), 0);
}

// Test create fields and putting something in
TEST_F(BufferTest, CreateFields) {
  Buffer *buffer = pack()->Create<VertexBuffer>();
  const FieldRefArray& fields = buffer->fields();
  // Verify initial state.
  unsigned int change_count = buffer->field_change_count();

  static float in_floats_1[][3] = {
    { 1, 2, 3, },
    { 4, 5, 6, },
    { 10, 11, 12, },
    { 13, 14, 15, },
  };
  const unsigned kNumComponents1 = arraysize(in_floats_1[0]);
  const unsigned kNumElements = arraysize(in_floats_1);
  const unsigned kStride1 = kNumComponents1;
  const size_t kSize1 = sizeof(in_floats_1[0]);

  // Add a field
  Field::Ref field_1 = Field::Ref(buffer->CreateField(
      FloatField::GetApparentClass(),
      kNumComponents1));
  EXPECT_EQ(fields.size(), 1);
  EXPECT_EQ(field_1, fields[0].Get());
  EXPECT_EQ(field_1->offset(), 0);
  EXPECT_EQ(buffer->stride(), kSize1);
  EXPECT_EQ(buffer->total_components(), kNumComponents1);
  EXPECT_EQ(buffer->GetSizeInBytes(), 0);
  EXPECT_TRUE(ChangeCountChanged(&change_count, buffer));

  // Allocate some elements.
  buffer->AllocateElements(4);
  EXPECT_EQ(buffer->GetSizeInBytes(), kSize1 * kNumElements);

  // Put data in.
  field_1->SetFromFloats(&in_floats_1[0][0], kStride1, 0, kNumElements);

  // Get Data out
  float out_floats_1[kNumElements][kNumComponents1];
  memset(out_floats_1, 0, sizeof(out_floats_1));
  field_1->GetAsFloats(0, &out_floats_1[0][0], kStride1, kNumElements);

  EXPECT_TRUE(CompareElements(&in_floats_1[0][0],
                              &out_floats_1[0][0],
                              kNumElements,
                              kNumComponents1));

  // Check offset out of range.
  EXPECT_FALSE(CheckErrorExists(error_status()));
  field_1->SetFromFloats(&in_floats_1[0][0], kStride1, kNumElements, 1);
  EXPECT_TRUE(CheckErrorExists(error_status()));

  // Check offset in range, length out of range.
  field_1->SetFromFloats(&in_floats_1[0][0], kStride1, kNumElements - 1, -1);
  EXPECT_TRUE(CheckErrorExists(error_status()));

  // Check that we can lock the buffer around SetFromFloats.
  {
    BufferLockHelper helper(buffer);
    void* data = helper.GetData(Buffer::WRITE_ONLY);
    ASSERT_TRUE(data != NULL);
    field_1->SetFromFloats(&in_floats_1[0][0], kStride1, 0, kNumElements);
  }

  // Check that we can lock the buffer around GetAsFloats.
  {
    BufferLockHelper helper(buffer);
    void* data = helper.GetData(Buffer::READ_ONLY);
    ASSERT_TRUE(data != NULL);
    field_1->SetFromFloats(&in_floats_1[0][0], kStride1, 0, kNumElements);
  }

  // Check that deleting buffer clears the field buffer pointer.
  pack()->RemoveObject(buffer);
  EXPECT_TRUE(field_1->buffer() == NULL);
}

// Test creating a field, putting data in, then adding another field and
// removing the original.
TEST_F(BufferTest, ReshuffleFields) {
  Buffer *buffer = pack()->Create<VertexBuffer>();
  const FieldRefArray& fields = buffer->fields();
  unsigned int change_count = buffer->field_change_count();

  static float in_floats_1[][3] = {
    { 1, 2, 3, },
    { 4, 5, 6, },
    { 10, 11, 12, },
    { 13, 14, 15, },
  };
  const unsigned kNumComponents1 = arraysize(in_floats_1[0]);
  const unsigned kNumElements = arraysize(in_floats_1);
  const unsigned kStride1 = kNumComponents1;
  const size_t kSize1 = sizeof(in_floats_1[0]);

  // Add a field
  Field::Ref field_1 = Field::Ref(buffer->CreateField(
      FloatField::GetApparentClass(),
      kNumComponents1));

  // Allocate some elements.
  buffer->AllocateElements(4);
  EXPECT_EQ(buffer->GetSizeInBytes(), kSize1 * kNumElements);

  // Put data in.
  field_1->SetFromFloats(&in_floats_1[0][0], kStride1, 0, kNumElements);

  // Get Data out
  float out_floats_1[kNumElements][kNumComponents1];
  memset(out_floats_1, 0, sizeof(out_floats_1));
  field_1->GetAsFloats(0, &out_floats_1[0][0], kStride1, kNumElements);

  EXPECT_TRUE(CompareElements(&in_floats_1[0][0],
                              &out_floats_1[0][0],
                              kNumElements,
                              kNumComponents1));

  // Check offset out of range.
  EXPECT_FALSE(CheckErrorExists(error_status()));
  field_1->SetFromFloats(&in_floats_1[0][0], kStride1, kNumElements, 1);
  EXPECT_TRUE(CheckErrorExists(error_status()));

  // Check offset in range, length out of range.
  field_1->SetFromFloats(&in_floats_1[0][0], kStride1, kNumElements - 1, -1);
  EXPECT_TRUE(CheckErrorExists(error_status()));

  static float in_floats_2[kNumElements][1] = {
    { 2, },
    { 4, },
    { 5, },
    { 7, },
  };
  const unsigned kNumComponents2 = arraysize(in_floats_2[0]);
  const unsigned kStride2 = kNumComponents2;
  const size_t kSize2 = sizeof(in_floats_2[0]);

  // Check adding a second field.
  Field::Ref field_2 = Field::Ref(buffer->CreateField(
        FloatField::GetApparentClass(),
        kNumComponents2));
  EXPECT_EQ(fields.size(), 2);
  EXPECT_EQ(field_1, fields[0].Get());
  EXPECT_EQ(field_2, fields[1].Get());
  EXPECT_EQ(field_1->offset(), 0);
  EXPECT_EQ(field_2->offset(), kSize1);
  EXPECT_EQ(buffer->stride(), kSize1 + kSize2);
  EXPECT_EQ(buffer->total_components(), kNumComponents1 + kNumComponents2);
  EXPECT_EQ(buffer->GetSizeInBytes(), (kSize1 + kSize2) * kNumElements);
  EXPECT_TRUE(ChangeCountChanged(&change_count, buffer));

  // Put data in second field.
  field_2->SetFromFloats(&in_floats_2[0][0], kStride2, 0, kNumElements);

  // Get Data out of second field
  float out_floats_2[kNumElements][kNumComponents2];
  memset(out_floats_1, 0, sizeof(out_floats_1));
  memset(out_floats_2, 0, sizeof(out_floats_2));
  field_1->GetAsFloats(0, &out_floats_1[0][0], kStride1, kNumElements);
  field_2->GetAsFloats(0, &out_floats_2[0][0], kStride2, kNumElements);

  EXPECT_TRUE(CompareElements(&in_floats_1[0][0],
                              &out_floats_1[0][0],
                              kNumElements,
                              kNumComponents1));
  EXPECT_TRUE(CompareElements(&in_floats_2[0][0],
                              &out_floats_2[0][0],
                              kNumElements,
                              kNumComponents2));

  // Check deleting a field
  buffer->RemoveField(field_1);
  EXPECT_TRUE(field_1->buffer() == NULL);
  EXPECT_EQ(fields.size(), 1);
  EXPECT_EQ(field_2, fields[0].Get());
  EXPECT_EQ(field_2->offset(), 0);
  EXPECT_EQ(buffer->stride(), kSize2);
  EXPECT_EQ(buffer->total_components(), kNumComponents2);
  EXPECT_EQ(buffer->GetSizeInBytes(), kSize2 * kNumElements);
  EXPECT_TRUE(ChangeCountChanged(&change_count, buffer));

  // Check that the data got shuffled.
  memset(out_floats_2, 0, sizeof(out_floats_2));
  field_2->GetAsFloats(0, &out_floats_2[0][0], kStride2, kNumElements);
  EXPECT_TRUE(CompareElements(&in_floats_2[0][0],
                              &out_floats_2[0][0],
                              kNumElements,
                              kNumComponents2));

  // Check that we can lock the buffer around SetFromFloats.
  {
    BufferLockHelper helper(buffer);
    void* data = helper.GetData(Buffer::WRITE_ONLY);
    ASSERT_TRUE(data != NULL);
    field_2->SetFromFloats(&in_floats_2[0][0], kStride2, 0, kNumElements);
  }

  // Check that we can lock the buffer around GetAsFloats.
  {
    BufferLockHelper helper(buffer);
    void* data = helper.GetData(Buffer::READ_ONLY);
    ASSERT_TRUE(data != NULL);
    field_2->GetAsFloats(0, &out_floats_2[0][0], kStride2, kNumElements);
  }

  // Check that deleting buffer clears the field buffer pointer.
  pack()->RemoveObject(buffer);
  EXPECT_TRUE(field_2->buffer() == NULL);
}

// Creates a vertex buffer, tests basic properties, and checks that writing data
// works.
TEST_F(BufferTest, VertexBuffer) {
  Buffer *buffer = pack()->Create<VertexBuffer>();

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

// Creates a source buffer, tests basic properties, and checks that writing then
// reading data works.
TEST_F(BufferTest, TestSourceBuffer) {
  Buffer *buffer = pack()->Create<SourceBuffer>();
  EXPECT_TRUE(buffer->IsA(SourceBuffer::GetApparentClass()));
  EXPECT_TRUE(buffer->IsA(VertexBufferBase::GetApparentClass()));
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

// Creates an index buffer, tests basic properties, and checks that writing
// data works.
TEST_F(BufferTest, TestIndexBuffer) {
  IndexBuffer *buffer = pack()->Create<IndexBuffer>();
  EXPECT_TRUE(buffer->IsA(IndexBuffer::GetApparentClass()));
  EXPECT_TRUE(buffer->IsA(Buffer::GetApparentClass()));

  EXPECT_TRUE(buffer->index_field()->IsA(UInt32Field::GetApparentClass()));

  const size_t kSize = 100;
  ASSERT_TRUE(buffer->AllocateElements(kSize));
  EXPECT_EQ(kSize, buffer->num_elements());

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

TEST_F(BufferTest, TestIndexFieldIsFirstField) {
  IndexBuffer *buffer = pack()->Create<IndexBuffer>();
  buffer->RemoveField(buffer->fields()[0]);
  Field* field = buffer->CreateField(UInt32Field::GetApparentClass(), 1);
  EXPECT_EQ(field, buffer->index_field());
}

// Creates a vertex buffer, checks that setting values from a RawData
// object works.
TEST_F(BufferTest, TestVertexBufferFromRawData) {
  VertexBuffer *buffer = pack()->Create<VertexBuffer>();
  EXPECT_TRUE(buffer->IsA(VertexBuffer::GetApparentClass()));
  EXPECT_TRUE(buffer->IsA(VertexBufferBase::GetApparentClass()));
  EXPECT_TRUE(buffer->IsA(Buffer::GetApparentClass()));

  // Create a field to verify that setting the buffer from raw data deletes it.
  buffer->CreateField(FloatField::GetApparentClass(), 1);

  const int kMemBufferSize = 32768;  // more than enough for our needs here
  MemoryBuffer<uint8> mem_buffer(kMemBufferSize);
  MemoryWriteStream stream(mem_buffer, kMemBufferSize);

  // write out serialization ID
  stream.Write(Buffer::kSerializationID, 4);

  // write out version
  stream.WriteLittleEndianInt32(1);

  // write out number of fields
  const int kNumFields = 3;
  stream.WriteLittleEndianInt32(kNumFields);

  // Write out the specification for the fields
  struct FieldInfo {
    int id;
    int num_components;
  };

  const FieldInfo infos[kNumFields] =
      { {Field::FIELDID_FLOAT32, 3},
        {Field::FIELDID_UINT32, 2},
        {Field::FIELDID_BYTE, 4} };

  for (int i = 0; i < kNumFields; ++i) {
    const FieldInfo &info = infos[i];
    stream.WriteByte(info.id);
    stream.WriteByte(info.num_components);
  }

  // Write out the number of elements
  const int kNumElements = 4;

  stream.WriteLittleEndianInt32(kNumElements);

  // Make note of stream position at end of header
  size_t data_start_position = stream.GetStreamPosition();

  // Write out the data for each field

  float float_data[kNumElements * 3] = {
     1.2f,  2.3f,   4.7f,
    -4.1f,  3.14f, 17.8f,
    17.3f, -4.7f,  -1.1f,
    -0.1f,  0.123f, 5.720f
  };

  uint32 int_data[kNumElements * 2] = {
    1, 2,
    3, 4,
    10, 11,
    12, 13
  };

  uint8 byte_data[kNumElements * 4] = {
    0, 1, 2, 3,
    17, 16, 10, 11,
    100, 99, 87, 88,
    50, 51, 60, 65
  };

  // First write out the float data
  for (int j = 0; j < kNumElements; ++j) {
    stream.WriteLittleEndianFloat32(float_data[j * 3]);
    stream.WriteLittleEndianFloat32(float_data[j * 3 + 1]);
    stream.WriteLittleEndianFloat32(float_data[j * 3 + 2]);
  }

  // Write out the int data
  for (int j = 0; j < kNumElements; ++j) {
    stream.WriteLittleEndianInt32(int_data[j * 2]);
    stream.WriteLittleEndianInt32(int_data[j * 2 + 1]);
  }

  // Write out the byte data
  for (int j = 0; j < kNumElements; ++j) {
    stream.WriteByte(byte_data[j * 4]);
    stream.WriteByte(byte_data[j * 4 + 1]);
    stream.WriteByte(byte_data[j * 4 + 2]);
    stream.WriteByte(byte_data[j * 4 + 3]);
  }

  // Make note of exactly how much we've written
  size_t total_length_in_bytes = stream.GetStreamPosition();

  // Create RawData object
  String uri("test_filename");
  uint8 *p = mem_buffer;
  RawData::Ref ref = RawData::Create(g_service_locator,
                                     uri,
                                     p,
                                     total_length_in_bytes);
  RawData *raw_data = ref;

  bool success = buffer->Set(raw_data);  // set values from raw data object
  EXPECT_TRUE(success);

  // Check that the field that was originally created to verify that setting
  // the buffer from raw data would remove any existing fields was in fact
  // removed.
  EXPECT_EQ(3, buffer->fields().size());

  float buffer_float_data[kNumElements * 3];
  uint32 buffer_int_data[kNumElements * 2];
  uint8 buffer_byte_data[kNumElements * 4];

  buffer->fields()[0].Get()->GetAsFloats(
      0, &buffer_float_data[0], 3, kNumElements);
  down_cast<UInt32Field*>(buffer->fields()[1].Get())->GetAsUInt32s(
      0, &buffer_int_data[0], 2, kNumElements);
  down_cast<UByteNField*>(buffer->fields()[2].Get())->GetAsUByteNs(
      0, &buffer_byte_data[0], 4, kNumElements);

  for (int i = 0; i < kNumElements; ++i) {
    // Validate float field
    EXPECT_EQ(buffer_float_data[i * 3 + 0], float_data[i * 3 + 0]);
    EXPECT_EQ(buffer_float_data[i * 3 + 1], float_data[i * 3 + 1]);
    EXPECT_EQ(buffer_float_data[i * 3 + 2], float_data[i * 3 + 2]);

    // Validate int field
    EXPECT_EQ(buffer_int_data[i * 2 + 0], int_data[i * 2 + 0]);
    EXPECT_EQ(buffer_int_data[i * 2 + 1], int_data[i * 2 + 1]);

    // Validate byte field
    EXPECT_EQ(buffer_byte_data[i * 4 + 0], byte_data[i * 4 + 0]);
    EXPECT_EQ(buffer_byte_data[i * 4 + 1], byte_data[i * 4 + 1]);
    EXPECT_EQ(buffer_byte_data[i * 4 + 2], byte_data[i * 4 + 2]);
    EXPECT_EQ(buffer_byte_data[i * 4 + 3], byte_data[i * 4 + 3]);
  }

  // Now, let's try a very nice test to verify that we properly
  // serialize -- this is a round trip test
  MemoryBuffer<uint8> serialized_data;
  SerializeBuffer(*buffer, &serialized_data);

  // Make sure serialized data length is identical to what we made
  ASSERT_EQ(total_length_in_bytes, serialized_data.GetLength());

  // Make sure the data matches
  uint8 *original = mem_buffer;
  uint8 *serialized = serialized_data;
  EXPECT_EQ(0, memcmp(original, serialized, total_length_in_bytes));
}

}  // namespace o3d
