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


// Tests Field, FloatField, UInt32Field, UByteNField.

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/error_status.h"
#include "core/cross/field.h"

namespace o3d {

namespace {

static const float kInFloats[][4] = {
  { 1.0f, 2.0f, 3.0f, 3.5f, },
  { 4.0f, 5.0f, 6.0f, 7.0f, },
  { 10.0f, 11.0f, 12.0f, 13.0f, },
  { 0.3f, 0.4f, 0.5f, -1.3f, },
};
const unsigned kFloatsNumComponents = arraysize(kInFloats[0]);
const unsigned kFloatsNumElements = arraysize(kInFloats);
const unsigned kFloatsStride = kFloatsNumComponents;

static const uint32 kInUInt32s[][4] = {
  { 1234, 67, 160000, 667, },
  { 0, 342353, 13443, 13, },
};

const unsigned kUInt32sNumComponents = arraysize(kInUInt32s[0]);
const unsigned kUInt32sNumElements = arraysize(kInUInt32s);
const unsigned kUInt32sStride = kUInt32sNumComponents;

static const uint8 kInUByteNs[][4] = {
  { 64, 255, 128, 254, },
  { 192, 0, 32, 1, },
};

const unsigned kUByteNsNumComponents = arraysize(kInUByteNs[0]);
const unsigned kUByteNsNumElements = arraysize(kInUByteNs);
const unsigned kUByteNsStride = kUByteNsNumComponents;

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

bool CompareUInt32sAsFloat(const uint32* uint32s,
                           const float* floats,
                           unsigned num_elements,
                           unsigned num_components) {
  for (; num_elements; --num_elements) {
    for (unsigned ii = 0; ii < num_components; ++ii) {
      float check = static_cast<float>(*uint32s);
      if (check != *floats) {
        return false;
      }
      ++uint32s;
      ++floats;
    }
  }
  return true;
}

bool CompareUByteNsAsFloat(const uint8* uint8s,
                           const float* floats,
                           unsigned num_elements,
                           unsigned num_components) {
  for (; num_elements; --num_elements) {
    for (unsigned ii = 0; ii < num_components; ++ii) {
      float check = static_cast<float>(*uint8s) / 255.0f;
      // The following EXPECT_EQ is in there to get around a bug with gcc
      // on linux where, most likely due to overly aggressive optimization, the
      // check != *floats test fails even though the values are identical.
      EXPECT_EQ(check, check);
      if (check != *floats) {
        return false;
      }
      ++uint8s;
      ++floats;
    }
  }
  return true;
}

}  // anonymous namespace

class FieldTest : public testing::Test {
 protected:
  FieldTest()
      : object_manager_(g_service_locator),
        error_status_(g_service_locator) {
  }

  virtual void SetUp();
  virtual void TearDown();

  IErrorStatus* error_status() { return &error_status_; }
  Pack* pack() { return pack_; }
  Buffer* buffer() { return buffer_; }

 private:
  ServiceDependency<ObjectManager> object_manager_;
  ErrorStatus error_status_;
  Pack* pack_;
  Buffer* buffer_;
};

void FieldTest::SetUp() {
  pack_ = object_manager_->CreatePack();
  buffer_ = pack()->Create<SourceBuffer>();
}

void FieldTest::TearDown() {
  object_manager_->DestroyPack(pack_);
}

// Test Field.
TEST_F(FieldTest, TestBasic) {
  const unsigned kNumComponents = 3;
  const unsigned kNumElements = 4;
  Field::Ref field_1_ref = Field::Ref(
      buffer()->CreateField(FloatField::GetApparentClass(), kNumComponents));
  ASSERT_FALSE(field_1_ref.IsNull());
  Field* field_1 = field_1_ref.Get();

  // Check things are as expected.
  EXPECT_EQ(field_1->num_components(), kNumComponents);
  EXPECT_EQ(field_1->offset(), 0);
  EXPECT_EQ(field_1->size(),
            field_1->num_components() * field_1->GetFieldComponentSize());
  EXPECT_EQ(field_1->buffer(), buffer());

  // Put some elements in the buffer.
  ASSERT_TRUE(buffer()->AllocateElements(kNumElements));
  EXPECT_EQ(buffer()->num_elements(), kNumElements);

  // Check various ranges
  EXPECT_TRUE(field_1->RangeValid(0, kNumElements));
  EXPECT_FALSE(field_1->RangeValid(0, kNumElements + 1));
  EXPECT_FALSE(field_1->RangeValid(kNumElements + 1, 0));
  EXPECT_FALSE(field_1->RangeValid(kNumElements - 1, -1));

  // Check if we create another field it's offset is correct.
  Field::Ref field_2_ref = Field::Ref(
      buffer()->CreateField(FloatField::GetApparentClass(), kNumComponents));
  ASSERT_FALSE(field_2_ref.IsNull());
  Field* field_2 = field_2_ref.Get();
  EXPECT_EQ(field_1->offset(), 0);
  EXPECT_EQ(field_2->offset(), field_1->size());
  EXPECT_EQ(field_2->buffer(), buffer());
  EXPECT_EQ(buffer()->num_elements(), kNumElements);

  // Check if we remove the first field the second field gets updated correctly.
  buffer()->RemoveField(field_1);
  EXPECT_TRUE(field_1->buffer() == NULL);
  EXPECT_EQ(field_2->offset(), 0);

  // Check we can't create a field of 0 components.
  EXPECT_TRUE(buffer()->CreateField(FloatField::GetApparentClass(), 0) ==
              NULL);
}

class FloatFieldTest : public testing::Test {
 protected:
  FloatFieldTest()
      : object_manager_(g_service_locator),
        error_status_(g_service_locator) {
  }

  virtual void SetUp();
  virtual void TearDown();

  IErrorStatus* error_status() { return &error_status_; }
  Pack* pack() { return pack_; }
  Buffer* buffer() { return buffer_; }

 private:
  ServiceDependency<ObjectManager> object_manager_;
  ErrorStatus error_status_;
  Pack* pack_;
  Buffer* buffer_;
};

void FloatFieldTest::SetUp() {
  pack_ = object_manager_->CreatePack();
  buffer_ = pack()->Create<SourceBuffer>();
}

void FloatFieldTest::TearDown() {
  object_manager_->DestroyPack(pack_);
}

// Test FloatField.
TEST_F(FloatFieldTest, TestBasic) {
  Field* field = buffer()->CreateField(FloatField::GetApparentClass(),
                                       kFloatsNumComponents);
  ASSERT_TRUE(field != NULL);
  ASSERT_TRUE(field->IsA(FloatField::GetApparentClass()));
  ASSERT_TRUE(field->IsA(Field::GetApparentClass()));

  EXPECT_EQ(field->GetFieldComponentSize(), sizeof(float));  // NOLINT

  ASSERT_TRUE(buffer()->AllocateElements(kFloatsNumElements));
  field->SetFromFloats(&kInFloats[0][0], kFloatsStride, 0, kFloatsNumElements);

  float out_floats[kFloatsNumElements][kFloatsNumComponents];
  memset(&out_floats, 0, sizeof(out_floats));
  field->GetAsFloats(0, &out_floats[0][0], kFloatsStride, kFloatsNumElements);

  EXPECT_TRUE(CompareElements(&kInFloats[0][0],
                              &out_floats[0][0],
                              kFloatsNumElements,
                              kFloatsNumComponents));

  Field* new_field = buffer()->CreateField(FloatField::GetApparentClass(),
                                           kFloatsNumComponents);
  ASSERT_TRUE(new_field != NULL);
  new_field->Copy(*field);
  memset(&out_floats, 0, sizeof(out_floats));
  new_field->GetAsFloats(0, &out_floats[0][0], kFloatsStride,
                         kFloatsNumElements);

  EXPECT_TRUE(CompareElements(&kInFloats[0][0],
                              &out_floats[0][0],
                              kFloatsNumElements,
                              kFloatsNumComponents));

  field->SetFromUInt32s(&kInUInt32s[0][0], kUInt32sStride, 0,
                        kUInt32sNumElements);

  memset(&out_floats, 0, sizeof(out_floats));
  field->GetAsFloats(0, &out_floats[0][0], kFloatsStride, kUInt32sNumElements);

  EXPECT_TRUE(CompareUInt32sAsFloat(&kInUInt32s[0][0],
                                    &out_floats[0][0],
                                    kUInt32sNumElements,
                                    kUInt32sNumComponents));

  field->SetFromUByteNs(&kInUByteNs[0][0], kUByteNsStride, 0,
                        kUByteNsNumElements);

  memset(&out_floats, 0, sizeof(out_floats));
  field->GetAsFloats(0, &out_floats[0][0], kFloatsStride, kUByteNsNumElements);

  EXPECT_TRUE(CompareUByteNsAsFloat(&kInUByteNs[0][0],
                                    &out_floats[0][0],
                                    kUByteNsNumElements,
                                    kUByteNsNumComponents));
}

class UInt32FieldTest : public testing::Test {
 protected:
  UInt32FieldTest()
      : object_manager_(g_service_locator),
        error_status_(g_service_locator) {
  }

  virtual void SetUp();
  virtual void TearDown();

  IErrorStatus* error_status() { return &error_status_; }
  Pack* pack() { return pack_; }
  Buffer* buffer() { return buffer_; }

 private:
  ServiceDependency<ObjectManager> object_manager_;
  ErrorStatus error_status_;
  Pack* pack_;
  Buffer* buffer_;
};

void UInt32FieldTest::SetUp() {
  pack_ = object_manager_->CreatePack();
  buffer_ = pack()->Create<SourceBuffer>();
}

void UInt32FieldTest::TearDown() {
  object_manager_->DestroyPack(pack_);
}

// Test UInt32Field.
TEST_F(UInt32FieldTest, TestBasic) {
  Field* field = buffer()->CreateField(UInt32Field::GetApparentClass(),
                                       kFloatsNumComponents);
  ASSERT_TRUE(field != NULL);
  ASSERT_TRUE(field->IsA(UInt32Field::GetApparentClass()));
  ASSERT_TRUE(field->IsA(Field::GetApparentClass()));

  EXPECT_EQ(field->GetFieldComponentSize(), sizeof(float));  // NOLINT

  ASSERT_TRUE(buffer()->AllocateElements(kFloatsNumElements));
  field->SetFromFloats(&kInFloats[0][0], kFloatsStride, 0, kFloatsNumElements);

  float out_floats[kFloatsNumElements][kFloatsNumComponents];

  memset(&out_floats, 0, sizeof(out_floats));
  field->GetAsFloats(0, &out_floats[0][0], kFloatsStride, kFloatsNumElements);

  for (unsigned jj = 0; jj < kFloatsNumElements; ++jj) {
    for (unsigned ii = 0; ii < kFloatsNumComponents; ++ii) {
      uint32 in_value = static_cast<uint32>(std::max(0.f, kInFloats[jj][ii]));
      uint32 out_value = static_cast<uint32>(std::max(0.f, out_floats[jj][ii]));
      EXPECT_EQ(in_value, out_value);
    }
  }

  field->SetFromUInt32s(&kInUInt32s[0][0], kUInt32sStride, 0,
                        kUInt32sNumElements);

  uint32 out_uint32s[kUInt32sNumElements][kUInt32sNumComponents];
  memset(&out_uint32s, 0, sizeof(out_uint32s));
  UInt32Field* uint32_field = down_cast<UInt32Field*>(field);
  uint32_field->GetAsUInt32s(0, &out_uint32s[0][0], kUInt32sStride,
                             kUInt32sNumElements);

  for (unsigned jj = 0; jj < kUInt32sNumElements; ++jj) {
    for (unsigned ii = 0; ii < kUInt32sNumComponents; ++ii) {
      EXPECT_EQ(kInUInt32s[jj][ii], out_uint32s[jj][ii]);
    }
  }

  Field* new_field = buffer()->CreateField(UInt32Field::GetApparentClass(),
                                           kUInt32sNumComponents);
  ASSERT_TRUE(new_field != NULL);
  new_field->Copy(*field);
  memset(&out_uint32s, 0, sizeof(out_uint32s));
  down_cast<UInt32Field*>(new_field)->GetAsUInt32s(
      0, &out_uint32s[0][0], kUInt32sStride, kUInt32sNumElements);

  for (unsigned jj = 0; jj < kUInt32sNumElements; ++jj) {
    for (unsigned ii = 0; ii < kUInt32sNumComponents; ++ii) {
      EXPECT_EQ(kInUInt32s[jj][ii], out_uint32s[jj][ii]);
    }
  }

  memset(&out_floats, 0, sizeof(out_floats));
  field->GetAsFloats(0, &out_floats[0][0], kFloatsStride, kUInt32sNumElements);

  EXPECT_TRUE(CompareUInt32sAsFloat(&kInUInt32s[0][0],
                                    &out_floats[0][0],
                                    kUInt32sNumElements,
                                    kUInt32sNumComponents));

  field->SetFromUByteNs(&kInUByteNs[0][0], kUByteNsStride, 0,
                        kUByteNsNumElements);

  memset(&out_floats, 0, sizeof(out_floats));
  field->GetAsFloats(0, &out_floats[0][0], kFloatsStride, kUByteNsNumElements);

  for (unsigned jj = 0; jj < kUByteNsNumElements; ++jj) {
    for (unsigned ii = 0; ii < kUByteNsNumComponents; ++ii) {
      EXPECT_EQ((kInUByteNs[jj][ii] > 0 ? 1.0f : 0.0f), out_floats[jj][ii]);
    }
  }
}

class UByteNFieldTest : public testing::Test {
 protected:
  UByteNFieldTest()
      : object_manager_(g_service_locator),
        error_status_(g_service_locator) {
  }

  virtual void SetUp();
  virtual void TearDown();

  IErrorStatus* error_status() { return &error_status_; }
  Pack* pack() { return pack_; }
  Buffer* buffer() { return buffer_; }

 private:
  ServiceDependency<ObjectManager> object_manager_;
  ErrorStatus error_status_;
  Pack* pack_;
  Buffer* buffer_;
};

void UByteNFieldTest::SetUp() {
  pack_ = object_manager_->CreatePack();
  buffer_ = pack()->Create<SourceBuffer>();
}

void UByteNFieldTest::TearDown() {
  object_manager_->DestroyPack(pack_);
}

// Test UByteNField.
TEST_F(UByteNFieldTest, TestBasic) {
  Field* field = buffer()->CreateField(UByteNField::GetApparentClass(),
                                       kFloatsNumComponents);
  ASSERT_TRUE(field != NULL);
  ASSERT_TRUE(field->IsA(UByteNField::GetApparentClass()));
  ASSERT_TRUE(field->IsA(Field::GetApparentClass()));

  EXPECT_EQ(field->GetFieldComponentSize(), sizeof(uint8));  // NOLINT

  ASSERT_TRUE(buffer()->AllocateElements(kFloatsNumElements));
  field->SetFromFloats(&kInFloats[0][0], kFloatsStride, 0, kFloatsNumElements);

  float out_floats[kFloatsNumElements][kFloatsNumComponents];

  memset(&out_floats, 0, sizeof(out_floats));
  field->GetAsFloats(0, &out_floats[0][0], kFloatsStride, kFloatsNumElements);

  static const float kEpsilon = 0.002f;

  for (unsigned jj = 0; jj < kFloatsNumElements; ++jj) {
    for (unsigned ii = 0; ii < kFloatsNumComponents; ++ii) {
      float expect = std::max(std::min(1.0f, kInFloats[jj][ii]), 0.0f);
      float difference = fabsf(expect - out_floats[jj][ii]);
      EXPECT_TRUE(difference < kEpsilon);
    }
  }

  field->SetFromUInt32s(&kInUInt32s[0][0], kUInt32sStride, 0,
                        kUInt32sNumElements);

  memset(&out_floats, 0, sizeof(out_floats));
  field->GetAsFloats(0, &out_floats[0][0], kFloatsStride, kUInt32sNumElements);

  for (unsigned jj = 0; jj < kUInt32sNumElements; ++jj) {
    for (unsigned ii = 0; ii < kUByteNsNumComponents; ++ii) {
      float check =
          static_cast<float>(std::min<uint32>(255, kInUInt32s[jj][ii])) /
          255.0f;
      EXPECT_EQ(check, out_floats[jj][ii]);
    }
  }

  field->SetFromUByteNs(&kInUByteNs[0][0], kUByteNsStride, 0,
                        kUByteNsNumElements);
  uint8 out_ubytens[kUByteNsNumElements][kUByteNsNumComponents];
  memset(&out_ubytens, 0, sizeof(out_ubytens));
  down_cast<UByteNField*>(field)->GetAsUByteNs(
      0, &out_ubytens[0][0], kUByteNsStride, kUByteNsNumElements);

  for (unsigned jj = 0; jj < kUByteNsNumElements; ++jj) {
    for (unsigned ii = 0; ii < kUByteNsNumComponents; ++ii) {
      EXPECT_EQ(kInUByteNs[jj][ii], out_ubytens[jj][ii]);
    }
  }

  Field* new_field = buffer()->CreateField(UByteNField::GetApparentClass(),
                                           kUByteNsNumComponents);
  ASSERT_TRUE(new_field != NULL);
  new_field->Copy(*field);

  memset(&out_ubytens, 0, sizeof(out_ubytens));
  down_cast<UByteNField*>(new_field)->GetAsUByteNs(
      0, &out_ubytens[0][0], kUByteNsStride, kUByteNsNumElements);

  for (unsigned jj = 0; jj < kUByteNsNumElements; ++jj) {
    for (unsigned ii = 0; ii < kUByteNsNumComponents; ++ii) {
      EXPECT_EQ(kInUByteNs[jj][ii], out_ubytens[jj][ii]);
    }
  }


  // Test that we can't make a UByteN field that is not multiple of 4
  EXPECT_TRUE(buffer()->CreateField(UByteNField::GetApparentClass(), 1) ==
              NULL);
  EXPECT_TRUE(buffer()->CreateField(UByteNField::GetApparentClass(), 2) ==
              NULL);
  EXPECT_TRUE(buffer()->CreateField(UByteNField::GetApparentClass(), 3) ==
              NULL);
}

}  // namespace o3d
