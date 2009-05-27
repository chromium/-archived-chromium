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


// This file implements unit tests for various Param operations.

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/param_operation.h"

namespace o3d {

class ParamOp2FloatsToFloat2Test : public testing::Test {
 protected:

  ParamOp2FloatsToFloat2Test()
      : object_manager_(g_service_locator),
        error_status_(g_service_locator) {
  }

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

 private:

  ServiceDependency<ObjectManager> object_manager_;
  ErrorStatus error_status_;
  Pack* pack_;
};

void ParamOp2FloatsToFloat2Test::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void ParamOp2FloatsToFloat2Test::TearDown() {
  pack_->Destroy();
}

// Tests ParamOp2FloatsToFloat2.
TEST_F(ParamOp2FloatsToFloat2Test, Basic) {
  ParamOp2FloatsToFloat2* operation = pack()->Create<ParamOp2FloatsToFloat2>();

  // Check that it got created.
  ASSERT_TRUE(operation != NULL);

  // Check that the correct params got created.
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp2FloatsToFloat2::kInput0ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp2FloatsToFloat2::kInput1ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat2>(
      ParamOp2FloatsToFloat2::kOutputParamName) != NULL);

  // Check that it evaluates correctly.
  operation->set_input_0(5.0f);
  operation->set_input_1(6.0f);
  EXPECT_EQ(operation->input_0(), 5.0f);
  EXPECT_EQ(operation->input_1(), 6.0f);
  Float2 output(operation->output());
  EXPECT_EQ(output[0], 5.0f);
  EXPECT_EQ(output[1], 6.0f);
}

class ParamOp3FloatsToFloat3Test : public testing::Test {
 protected:

  ParamOp3FloatsToFloat3Test()
      : object_manager_(g_service_locator),
        error_status_(g_service_locator) {
  }

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

 private:

  ServiceDependency<ObjectManager> object_manager_;
  ErrorStatus error_status_;
  Pack* pack_;
};

void ParamOp3FloatsToFloat3Test::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void ParamOp3FloatsToFloat3Test::TearDown() {
  pack_->Destroy();
}

// Tests ParamOp3FloatsToFloat3.
TEST_F(ParamOp3FloatsToFloat3Test, Basic) {
  ParamOp3FloatsToFloat3* operation = pack()->Create<ParamOp3FloatsToFloat3>();

  // Check that it got created.
  ASSERT_TRUE(operation != NULL);

  // Check that the correct params got created.
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp3FloatsToFloat3::kInput0ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp3FloatsToFloat3::kInput1ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp3FloatsToFloat3::kInput2ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat3>(
      ParamOp3FloatsToFloat3::kOutputParamName) != NULL);

  // Check that it evaluates correctly.
  operation->set_input_0(5.0f);
  operation->set_input_1(6.0f);
  operation->set_input_2(7.0f);
  EXPECT_EQ(operation->input_0(), 5.0f);
  EXPECT_EQ(operation->input_1(), 6.0f);
  EXPECT_EQ(operation->input_2(), 7.0f);
  Float3 output(operation->output());
  EXPECT_EQ(output[0], 5.0f);
  EXPECT_EQ(output[1], 6.0f);
  EXPECT_EQ(output[2], 7.0f);
}

class ParamOp4FloatsToFloat4Test : public testing::Test {
 protected:

  ParamOp4FloatsToFloat4Test()
      : object_manager_(g_service_locator),
        error_status_(g_service_locator) {
  }

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

 private:

  ServiceDependency<ObjectManager> object_manager_;
  ErrorStatus error_status_;
  Pack* pack_;
};

void ParamOp4FloatsToFloat4Test::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void ParamOp4FloatsToFloat4Test::TearDown() {
  pack_->Destroy();
}

// Tests ParamOp4FloatsToFloat4.
TEST_F(ParamOp4FloatsToFloat4Test, Basic) {
  ParamOp4FloatsToFloat4* operation = pack()->Create<ParamOp4FloatsToFloat4>();

  // Check that it got created.
  ASSERT_TRUE(operation != NULL);

  // Check that the correct params got created.
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp4FloatsToFloat4::kInput0ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp4FloatsToFloat4::kInput1ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp4FloatsToFloat4::kInput2ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp4FloatsToFloat4::kInput3ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat4>(
      ParamOp4FloatsToFloat4::kOutputParamName) != NULL);

  // Check that it evaluates correctly.
  operation->set_input_0(5.0f);
  operation->set_input_1(6.0f);
  operation->set_input_2(7.0f);
  operation->set_input_3(8.0f);
  EXPECT_EQ(operation->input_0(), 5.0f);
  EXPECT_EQ(operation->input_1(), 6.0f);
  EXPECT_EQ(operation->input_2(), 7.0f);
  EXPECT_EQ(operation->input_3(), 8.0f);
  Float4 output(operation->output());
  EXPECT_EQ(output[0], 5.0f);
  EXPECT_EQ(output[1], 6.0f);
  EXPECT_EQ(output[2], 7.0f);
  EXPECT_EQ(output[3], 8.0f);
}

class ParamOp16FloatsToMatrix4Test : public testing::Test {
 protected:

  ParamOp16FloatsToMatrix4Test()
      : object_manager_(g_service_locator),
        error_status_(g_service_locator) {
  }

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

 private:

  ServiceDependency<ObjectManager> object_manager_;
  ErrorStatus error_status_;
  Pack* pack_;
};

void ParamOp16FloatsToMatrix4Test::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void ParamOp16FloatsToMatrix4Test::TearDown() {
  pack_->Destroy();
}

// Tests ParamOp16FloatsToMatrix4.
TEST_F(ParamOp16FloatsToMatrix4Test, Basic) {
  ParamOp16FloatsToMatrix4* operation =
      pack()->Create<ParamOp16FloatsToMatrix4>();

  // Check that it got created.
  ASSERT_TRUE(operation != NULL);

  // Check that the correct params got created.
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp16FloatsToMatrix4::kInput0ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp16FloatsToMatrix4::kInput1ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp16FloatsToMatrix4::kInput2ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp16FloatsToMatrix4::kInput3ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp16FloatsToMatrix4::kInput4ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp16FloatsToMatrix4::kInput5ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp16FloatsToMatrix4::kInput6ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp16FloatsToMatrix4::kInput7ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp16FloatsToMatrix4::kInput8ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp16FloatsToMatrix4::kInput9ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp16FloatsToMatrix4::kInput10ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp16FloatsToMatrix4::kInput11ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp16FloatsToMatrix4::kInput12ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp16FloatsToMatrix4::kInput13ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp16FloatsToMatrix4::kInput14ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      ParamOp16FloatsToMatrix4::kInput15ParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamMatrix4>(
      ParamOp16FloatsToMatrix4::kOutputParamName) != NULL);

  // Check that it defaults to the identity.
  Matrix4 output(operation->output());
  EXPECT_EQ(output[0][0], 1.0f);
  EXPECT_EQ(output[0][1], 0.0f);
  EXPECT_EQ(output[0][2], 0.0f);
  EXPECT_EQ(output[0][3], 0.0f);
  EXPECT_EQ(output[1][0], 0.0f);
  EXPECT_EQ(output[1][1], 1.0f);
  EXPECT_EQ(output[1][2], 0.0f);
  EXPECT_EQ(output[1][3], 0.0f);
  EXPECT_EQ(output[2][0], 0.0f);
  EXPECT_EQ(output[2][1], 0.0f);
  EXPECT_EQ(output[2][2], 1.0f);
  EXPECT_EQ(output[2][3], 0.0f);
  EXPECT_EQ(output[3][0], 0.0f);
  EXPECT_EQ(output[3][1], 0.0f);
  EXPECT_EQ(output[3][2], 0.0f);
  EXPECT_EQ(output[3][3], 1.0f);

  // Check that it evaluates correctly.
  operation->set_input_0(0.1f);
  operation->set_input_1(1.0f);
  operation->set_input_2(2.0f);
  operation->set_input_3(3.0f);
  operation->set_input_4(4.0f);
  operation->set_input_5(5.0f);
  operation->set_input_6(6.0f);
  operation->set_input_7(7.0f);
  operation->set_input_8(8.0f);
  operation->set_input_9(9.0f);
  operation->set_input_10(10.0f);
  operation->set_input_11(11.0f);
  operation->set_input_12(12.0f);
  operation->set_input_13(13.0f);
  operation->set_input_14(14.0f);
  operation->set_input_15(15.0f);
  EXPECT_EQ(operation->input_0(), 0.1f);
  EXPECT_EQ(operation->input_1(), 1.0f);
  EXPECT_EQ(operation->input_2(), 2.0f);
  EXPECT_EQ(operation->input_3(), 3.0f);
  EXPECT_EQ(operation->input_4(), 4.0f);
  EXPECT_EQ(operation->input_5(), 5.0f);
  EXPECT_EQ(operation->input_6(), 6.0f);
  EXPECT_EQ(operation->input_7(), 7.0f);
  EXPECT_EQ(operation->input_8(), 8.0f);
  EXPECT_EQ(operation->input_9(), 9.0f);
  EXPECT_EQ(operation->input_10(), 10.0f);
  EXPECT_EQ(operation->input_11(), 11.0f);
  EXPECT_EQ(operation->input_12(), 12.0f);
  EXPECT_EQ(operation->input_13(), 13.0f);
  EXPECT_EQ(operation->input_14(), 14.0f);
  EXPECT_EQ(operation->input_15(), 15.0f);
  output = operation->output();
  EXPECT_EQ(output[0][0], 0.1f);
  EXPECT_EQ(output[0][1], 1.0f);
  EXPECT_EQ(output[0][2], 2.0f);
  EXPECT_EQ(output[0][3], 3.0f);
  EXPECT_EQ(output[1][0], 4.0f);
  EXPECT_EQ(output[1][1], 5.0f);
  EXPECT_EQ(output[1][2], 6.0f);
  EXPECT_EQ(output[1][3], 7.0f);
  EXPECT_EQ(output[2][0], 8.0f);
  EXPECT_EQ(output[2][1], 9.0f);
  EXPECT_EQ(output[2][2], 10.0f);
  EXPECT_EQ(output[2][3], 11.0f);
  EXPECT_EQ(output[3][0], 12.0f);
  EXPECT_EQ(output[3][1], 13.0f);
  EXPECT_EQ(output[3][2], 14.0f);
  EXPECT_EQ(output[3][3], 15.0f);
}

class TRSToMatrix4Test : public testing::Test {
 protected:

  TRSToMatrix4Test()
      : object_manager_(g_service_locator),
        error_status_(g_service_locator) {
  }

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

 private:

  ServiceDependency<ObjectManager> object_manager_;
  ErrorStatus error_status_;
  Pack* pack_;
};

void TRSToMatrix4Test::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void TRSToMatrix4Test::TearDown() {
  pack_->Destroy();
}

// Tests TRSToMatrix4.
TEST_F(TRSToMatrix4Test, Basic) {
  TRSToMatrix4* operation = pack()->Create<TRSToMatrix4>();

  // Check that it got created.
  ASSERT_TRUE(operation != NULL);

  // Check that the correct params got created.
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      TRSToMatrix4::kTranslateXParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      TRSToMatrix4::kTranslateYParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      TRSToMatrix4::kTranslateZParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      TRSToMatrix4::kRotateXParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      TRSToMatrix4::kRotateYParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      TRSToMatrix4::kRotateZParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      TRSToMatrix4::kScaleXParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      TRSToMatrix4::kScaleYParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamFloat>(
      TRSToMatrix4::kScaleZParamName) != NULL);
  EXPECT_TRUE(operation->GetParam<ParamMatrix4>(
      TRSToMatrix4::kOutputParamName) != NULL);

  // Check that it defaults to the identity.
  Matrix4 output(operation->output());
  EXPECT_EQ(output[0][0], 1.0f);
  EXPECT_EQ(output[0][1], 0.0f);
  EXPECT_EQ(output[0][2], 0.0f);
  EXPECT_EQ(output[0][3], 0.0f);
  EXPECT_EQ(output[1][0], 0.0f);
  EXPECT_EQ(output[1][1], 1.0f);
  EXPECT_EQ(output[1][2], 0.0f);
  EXPECT_EQ(output[1][3], 0.0f);
  EXPECT_EQ(output[2][0], 0.0f);
  EXPECT_EQ(output[2][1], 0.0f);
  EXPECT_EQ(output[2][2], 1.0f);
  EXPECT_EQ(output[2][3], 0.0f);
  EXPECT_EQ(output[3][0], 0.0f);
  EXPECT_EQ(output[3][1], 0.0f);
  EXPECT_EQ(output[3][2], 0.0f);
  EXPECT_EQ(output[3][3], 1.0f);

  // Check that it evaluates correctly.
  operation->set_translate_x(1.0f);
  operation->set_translate_y(2.0f);
  operation->set_translate_z(3.0f);
  operation->set_rotate_x(4.0f);
  operation->set_rotate_y(5.0f);
  operation->set_rotate_z(6.0f);
  operation->set_scale_x(7.0f);
  operation->set_scale_y(8.0f);
  operation->set_scale_z(9.0f);
  EXPECT_EQ(operation->translate_x(), 1.0f);
  EXPECT_EQ(operation->translate_y(), 2.0f);
  EXPECT_EQ(operation->translate_z(), 3.0f);
  EXPECT_EQ(operation->rotate_x(), 4.0f);
  EXPECT_EQ(operation->rotate_y(), 5.0f);
  EXPECT_EQ(operation->rotate_z(), 6.0f);
  EXPECT_EQ(operation->scale_x(), 7.0f);
  EXPECT_EQ(operation->scale_y(), 8.0f);
  EXPECT_EQ(operation->scale_z(), 9.0f);
  output = operation->output();
  Matrix4 expected(
      Matrix4::translation(Vector3(1.0f, 2.0f, 3.0f)) *
      Matrix4::rotationZYX(Vector3(4.0f, 5.0f, 6.0f)) *
      Matrix4::scale(Vector3(7.0f, 8.0f, 9.0f)));
  EXPECT_EQ(output[0][0], expected[0][0]);
  EXPECT_EQ(output[0][1], expected[0][1]);
  EXPECT_EQ(output[0][2], expected[0][2]);
  EXPECT_EQ(output[0][3], expected[0][3]);
  EXPECT_EQ(output[1][0], expected[1][0]);
  EXPECT_EQ(output[1][1], expected[1][1]);
  EXPECT_EQ(output[1][2], expected[1][2]);
  EXPECT_EQ(output[1][3], expected[1][3]);
  EXPECT_EQ(output[2][0], expected[2][0]);
  EXPECT_EQ(output[2][1], expected[2][1]);
  EXPECT_EQ(output[2][2], expected[2][2]);
  EXPECT_EQ(output[2][3], expected[2][3]);
  EXPECT_EQ(output[3][0], expected[3][0]);
  EXPECT_EQ(output[3][1], expected[3][1]);
  EXPECT_EQ(output[3][2], expected[3][2]);
  EXPECT_EQ(output[3][3], expected[3][3]);
}

}  // namespace o3d
