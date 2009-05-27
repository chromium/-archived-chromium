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


// Tests functionality of the ParamArray class

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/id_manager.h"
#include "core/cross/error.h"
#include "core/cross/param_array.h"
#include "core/cross/standard_param.h"

namespace o3d {

// Test fixture for ParamArray testing. Creates a pack before each test and
// deletes it after.
class ParamArrayTest : public testing::Test {
 protected:

  ParamArrayTest()
      : object_manager_(g_service_locator),
        transformation_context_(g_service_locator) {
  }

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

  ServiceDependency<ObjectManager> object_manager_;
  TransformationContext transformation_context_;

 private:
  Pack* pack_;
};

void ParamArrayTest::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void ParamArrayTest::TearDown() {
  pack_->Destroy();
}

// Test ParamArray
TEST_F(ParamArrayTest, Basic) {
  ParamArray* param_array = pack()->Create<ParamArray>();
  // Check that it got created.
  ASSERT_TRUE(param_array != NULL);

  // Check that it inherits from what we expect it to.
  EXPECT_TRUE(param_array->IsA(NamedObject::GetApparentClass()));

  // Check that it's size is zero.
  EXPECT_EQ(param_array->size(), 0);
}

// Test ParamArray::CreateParam
TEST_F(ParamArrayTest, CreateParam) {
  ParamArray* param_array = pack()->Create<ParamArray>();
  // Check that it got created.
  ASSERT_TRUE(param_array != NULL);

  // Check we can add a param.
  ParamFloat* param_0 = param_array->CreateParam<ParamFloat>(0);
  ASSERT_TRUE(param_0 != NULL);

  // Check the length.
  EXPECT_EQ(param_array->size(), 1);

  // Add some more.
  ParamFloat* param_5 = param_array->CreateParam<ParamFloat>(5);
  ASSERT_TRUE(param_5 != NULL);

  // Check that the params in between got created.
  EXPECT_EQ(param_array->size(), 6);
  EXPECT_EQ(param_array->GetParam<ParamFloat>(0), param_0);
  EXPECT_TRUE(param_array->GetParam<ParamFloat>(1) != NULL);
  EXPECT_TRUE(param_array->GetParam<ParamFloat>(2) != NULL);
  EXPECT_TRUE(param_array->GetParam<ParamFloat>(3) != NULL);
  EXPECT_TRUE(param_array->GetParam<ParamFloat>(4) != NULL);
  EXPECT_EQ(param_array->GetParam<ParamFloat>(5), param_5);

  // Check that out of range params don't exit.
  EXPECT_TRUE(param_array->GetParam<ParamFloat>(6) == NULL);

  // Check replacing a param.
  ParamFloat* new_param_5 = param_array->CreateParam<ParamFloat>(5);
  ASSERT_TRUE(new_param_5 != NULL);

  EXPECT_EQ(param_array->size(), 6);
  EXPECT_EQ(param_array->GetParam<ParamFloat>(0), param_0);
  EXPECT_TRUE(param_array->GetParam<ParamFloat>(1) != NULL);
  EXPECT_TRUE(param_array->GetParam<ParamFloat>(2) != NULL);
  EXPECT_TRUE(param_array->GetParam<ParamFloat>(3) != NULL);
  EXPECT_TRUE(param_array->GetParam<ParamFloat>(4) != NULL);
  EXPECT_EQ(param_array->GetParam<ParamFloat>(5), new_param_5);

  // Check using ParamInArray.
  EXPECT_TRUE(param_array->ParamInArray(new_param_5));
  EXPECT_FALSE(param_array->ParamInArray(param_5));
}

// Test ParamArray::RemoveParams
TEST_F(ParamArrayTest, RemoveParam) {
  ParamArray* param_array = pack()->Create<ParamArray>();
  // Check that it got created.
  ASSERT_TRUE(param_array != NULL);

  // Check we can add params.
  ParamFloat* param_0 = param_array->CreateParam<ParamFloat>(6);
  ASSERT_TRUE(param_0 != NULL);

  // Check the length.
  EXPECT_EQ(param_array->size(), 7);

  // Get the last param so we can see that it moves.
  ParamFloat* param;
  param = param_array->GetParam<ParamFloat>(6);
  ASSERT_TRUE(param != NULL);

  // Remove a single param.
  param_array->RemoveParams(1, 1);

  EXPECT_EQ(param_array->size(), 6);
  EXPECT_EQ(param_array->GetParam<ParamFloat>(5), param);

  // Remove a range
  param_array->RemoveParams(1, 3);
  EXPECT_EQ(param_array->size(), 3);
  EXPECT_EQ(param_array->GetParam<ParamFloat>(2), param);

  // Remove the first param.
  param_array->RemoveParams(0, 1);
  EXPECT_EQ(param_array->size(), 2);
  EXPECT_EQ(param_array->GetParam<ParamFloat>(1), param);

  // Remove the end param
  param_array->RemoveParams(1, 1);
  EXPECT_EQ(param_array->size(), 1);
  EXPECT_NE(param_array->GetParam<ParamFloat>(0), param);

  // Remove the remaining param.
  param_array->RemoveParams(0, 1);
  EXPECT_EQ(param_array->size(), 0);
  EXPECT_TRUE(param_array->GetParam<ParamFloat>(0) == NULL);
}

}  // namespace o3d
