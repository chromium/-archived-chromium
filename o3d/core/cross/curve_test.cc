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


// This file implements unit tests for class Curve.

#include <algorithm>
#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/curve.h"
#include "import/cross/memory_buffer.h"
#include "import/cross/memory_stream.h"
#include "serializer/cross/serializer_binary.h"

namespace o3d {

class CurveTest : public testing::Test {
 protected:

  CurveTest()
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

void CurveTest::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void CurveTest::TearDown() {
  pack_->Destroy();
}

namespace {

const float kEpsilon = 0.001f;
const float kEpsilon2 = 0.1f;

// Returns true of key is in keys.
bool KeyInKeys(CurveKey* key, const CurveKeyRefArray& keys) {
  return std::find(keys.begin(), keys.end(), CurveKey::Ref(key)) !=
      keys.end();
}

// Checks that a output on a curve is close to the expected output.
bool CheckCurve(Curve* curve,
                float input,
                float expected_output) {
  float output = curve->Evaluate(input, NULL);
  return fabsf(output - expected_output) < kEpsilon;
}

// Checks that a output on a curve is close to the expected output.
bool CheckCurveWithContext(Curve* curve,
                           float input,
                           float expected_output,
                           FunctionContext* context) {
  float output = curve->Evaluate(input, context);
  return fabsf(output - expected_output) < kEpsilon2;
}

}  // anonymous namespace.

// Tests Curve.
TEST_F(CurveTest, Basic) {
  Curve* curve = pack()->Create<Curve>();
  // Check that it got created.
  EXPECT_TRUE(curve != NULL);
  // Check that it derives from NamedObject
  EXPECT_TRUE(curve->IsA(NamedObject::GetApparentClass()));
}

// Tests CreateFunctionContext.
TEST_F(CurveTest, CreateFunctionContext) {
  Curve* curve = pack()->Create<Curve>();
  ASSERT_TRUE(curve != NULL);

  FunctionContext* context = curve->CreateFunctionContext();
  ASSERT_TRUE(context != NULL);
  EXPECT_TRUE(context->IsA(CurveFunctionContext::GetApparentClass()));

  delete context;
}

// Tests GetFunctionContextClass.
TEST_F(CurveTest, GetFunctionContextClass) {
  Curve* curve = pack()->Create<Curve>();
  ASSERT_TRUE(curve != NULL);

  EXPECT_EQ(curve->GetFunctionContextClass(),
            CurveFunctionContext::GetApparentClass());
}

// Tests CreateKey and RemoveKey.
TEST_F(CurveTest, CreateKeyRemoveKey) {
  // Create one of each kind of key.
  Curve* curve = pack()->Create<Curve>();
  ASSERT_TRUE(curve != NULL);

  StepCurveKey::Ref step_key(curve->Create<StepCurveKey>());
  LinearCurveKey::Ref linear_key(curve->Create<LinearCurveKey>());
  BezierCurveKey::Ref bezier_key(curve->Create<BezierCurveKey>());

  ASSERT_TRUE(step_key != NULL);
  ASSERT_TRUE(linear_key != NULL);
  ASSERT_TRUE(bezier_key != NULL);

  const CurveKeyRefArray& keys = curve->keys();
  EXPECT_EQ(keys.size(), 3);
  EXPECT_TRUE(KeyInKeys(step_key, keys));
  EXPECT_TRUE(KeyInKeys(linear_key, keys));
  EXPECT_TRUE(KeyInKeys(bezier_key, keys));

  step_key->Destroy();
  EXPECT_EQ(keys.size(), 2);
  EXPECT_FALSE(KeyInKeys(step_key, keys));
  EXPECT_TRUE(KeyInKeys(linear_key, keys));
  EXPECT_TRUE(KeyInKeys(bezier_key, keys));

  bezier_key->Destroy();
  EXPECT_EQ(keys.size(), 1);
  EXPECT_TRUE(KeyInKeys(linear_key, keys));
  EXPECT_FALSE(KeyInKeys(bezier_key, keys));

  linear_key->Destroy();
  EXPECT_EQ(keys.size(), 0);
}

// Tests ResortKeys and IsDiscontinuous
TEST_F(CurveTest, ResortKeysDiscontinuous) {
  Curve* curve = pack()->Create<Curve>();
  ASSERT_TRUE(curve != NULL);

  // No reason to rebuild the cache everytime
  // since we are not checking outputs.
  curve->set_use_cache(false);

  CurveKey* key1 = curve->Create<LinearCurveKey>();
  CurveKey* key2 = curve->Create<LinearCurveKey>();
  CurveKey* key3 = curve->Create<LinearCurveKey>();
  CurveKey* key4 = curve->Create<LinearCurveKey>();

  ASSERT_TRUE(key1 != NULL);
  ASSERT_TRUE(key2 != NULL);
  ASSERT_TRUE(key3 != NULL);
  ASSERT_TRUE(key4 != NULL);

  const CurveKeyRefArray& keys = curve->keys();
  EXPECT_EQ(keys.size(), 4);

  key1->SetInput(1.0f);
  key2->SetInput(2.0f);
  key3->SetInput(3.0f);
  key4->SetInput(4.0f);
  EXPECT_FALSE(curve->IsDiscontinuous());  // causes keys to be re-sorted.

  EXPECT_EQ(keys.size(), 4);
  EXPECT_EQ(keys[0], key1);
  EXPECT_EQ(keys[1], key2);
  EXPECT_EQ(keys[2], key3);
  EXPECT_EQ(keys[3], key4);

  key4->SetInput(0.0f);
  EXPECT_FALSE(curve->IsDiscontinuous());  // causes keys to be re-sorted.

  EXPECT_EQ(keys.size(), 4);
  EXPECT_EQ(keys[0], key4);
  EXPECT_EQ(keys[1], key1);
  EXPECT_EQ(keys[2], key2);
  EXPECT_EQ(keys[3], key3);

  key4->SetInput(2.5f);
  EXPECT_FALSE(curve->IsDiscontinuous());  // causes keys to be re-sorted.

  EXPECT_EQ(keys.size(), 4);
  EXPECT_EQ(keys[0], key1);
  EXPECT_EQ(keys[1], key2);
  EXPECT_EQ(keys[2], key4);
  EXPECT_EQ(keys[3], key3);

  key4->SetOutput(1.0f);
  EXPECT_FALSE(curve->IsDiscontinuous());  // causes keys to be re-sorted.

  key4->SetInput(2.0f);
  EXPECT_TRUE(curve->IsDiscontinuous());  // causes keys to be re-sorted.
  EXPECT_EQ(keys.size(), 4);
  EXPECT_EQ(keys[0], key1);
  EXPECT_EQ(keys[1], key2);
  EXPECT_EQ(keys[2], key4);
  EXPECT_EQ(keys[3], key3);

  key4->SetInput(1.5f);  // should move it before key2
  EXPECT_FALSE(curve->IsDiscontinuous());  // causes keys to be re-sorted.
  EXPECT_EQ(keys.size(), 4);
  EXPECT_EQ(keys[0], key1);
  EXPECT_EQ(keys[1], key4);
  EXPECT_EQ(keys[2], key2);
  EXPECT_EQ(keys[3], key3);

  key4->SetInput(2.0f);  // should still be before key2.
  EXPECT_TRUE(curve->IsDiscontinuous());  // causes keys to be re-sorted.
  EXPECT_EQ(keys.size(), 4);
  EXPECT_EQ(keys[0], key1);
  EXPECT_EQ(keys[1], key4);
  EXPECT_EQ(keys[2], key2);
  EXPECT_EQ(keys[3], key3);

  CurveKey* key5 = curve->Create<LinearCurveKey>();
  CurveKey* key6 = curve->Create<LinearCurveKey>();
  EXPECT_TRUE(curve->IsDiscontinuous());  // causes keys to be re-sorted.
  EXPECT_EQ(keys.size(), 6);
  EXPECT_EQ(keys[0], key5);
  EXPECT_EQ(keys[1], key6);
  EXPECT_EQ(keys[2], key1);
  EXPECT_EQ(keys[3], key4);
  EXPECT_EQ(keys[4], key2);
  EXPECT_EQ(keys[5], key3);

  // Move 2 keys together. They should still be in the same order relative to
  // each other.
  key5->SetInput(5.0f);
  key6->SetInput(5.0f);
  EXPECT_TRUE(curve->IsDiscontinuous());  // causes keys to be re-sorted.
  EXPECT_EQ(keys.size(), 6);
  EXPECT_EQ(keys[0], key1);
  EXPECT_EQ(keys[1], key4);
  EXPECT_EQ(keys[2], key2);
  EXPECT_EQ(keys[3], key3);
  EXPECT_EQ(keys[4], key5);
  EXPECT_EQ(keys[5], key6);

  // Add a StepKey
  key1->SetInput(1.0f);
  key2->SetInput(2.0f);
  key3->SetInput(3.0f);
  key4->SetInput(4.0f);
  EXPECT_FALSE(curve->IsDiscontinuous());  // causes keys to be re-sorted.
  CurveKey* stepkey1 = curve->Create<StepCurveKey>();
  EXPECT_TRUE(curve->IsDiscontinuous());  // causes keys to be re-sorted.

  // Try all step keys.
  CurveKey* stepkey2 = curve->Create<StepCurveKey>();
  key1->Destroy();
  key2->Destroy();
  key3->Destroy();
  key4->Destroy();
  key5->Destroy();
  key6->Destroy();
  EXPECT_FALSE(curve->IsDiscontinuous());  // causes keys to be re-sorted.
}

// Tests Evaluate for StepCurveKey.
TEST_F(CurveTest, EvaluateStepCurveKey) {
  Curve* curve = pack()->Create<Curve>();
  ASSERT_TRUE(curve != NULL);

  // No cache since we want to evaluate the actual curves.
  curve->set_use_cache(false);

  // Try no key
  EXPECT_EQ(curve->Evaluate(0.0f, NULL), 0.0f);

  // Try 1 key
  CurveKey* key1 = curve->Create<StepCurveKey>();
  ASSERT_TRUE(key1 != NULL);
  key1->SetOutput(1.0f);

  EXPECT_EQ(curve->Evaluate(0.0f, NULL), 1.0f);
  EXPECT_EQ(curve->Evaluate(1.0f, NULL), 1.0f);
  EXPECT_EQ(curve->Evaluate(2.0f, NULL), 1.0f);

  // Try 2 keys
  CurveKey* key2 = curve->Create<StepCurveKey>();
  ASSERT_TRUE(key2 != NULL);
  key2->SetOutput(2.0f);

  // Because the keys are both at input 0 everything should be 2.0 from zero
  // on.
  EXPECT_EQ(curve->Evaluate(-1.0f, NULL), 1.0f);
  EXPECT_EQ(curve->Evaluate(0.0f, NULL), 2.0f);
  EXPECT_EQ(curve->Evaluate(1.0f, NULL), 2.0f);
  EXPECT_EQ(curve->Evaluate(2.0f, NULL), 2.0f);

  // Move key 2
  key2->SetInput(1.0f);
  EXPECT_EQ(curve->Evaluate(-1.0f, NULL), 1.0f);
  EXPECT_EQ(curve->Evaluate(0.0f, NULL), 1.0f);
  EXPECT_EQ(curve->Evaluate(1.0f, NULL), 2.0f);
  EXPECT_EQ(curve->Evaluate(2.0f, NULL), 2.0f);
}
//
// Tests Evaluate for LinearCurveKey.
TEST_F(CurveTest, EvaluateLinearCurveKey) {
  Curve* curve = pack()->Create<Curve>();
  ASSERT_TRUE(curve != NULL);

  // No cache since we want to evaluate the actual curves.
  curve->set_use_cache(false);

  // Try 2 keys
  CurveKey* key1 = curve->Create<LinearCurveKey>();
  CurveKey* key2 = curve->Create<LinearCurveKey>();
  ASSERT_TRUE(key1 != NULL);
  ASSERT_TRUE(key2 != NULL);

  const float kStartInput = 1.0f;
  const float kStartOutput = 1.0f;
  const float kEndInput = 2.0f;
  const float kEndOutput = 5.0f;
  const float kOutputRange = kEndOutput - kStartOutput;

  key1->SetOutput(kStartOutput);
  key1->SetInput(kStartInput);
  key2->SetOutput(kEndOutput);
  key2->SetInput(kEndInput);

  curve->set_use_cache(false);
  for (int ii = 0; ii < 2; ++ii) {
    EXPECT_TRUE(CheckCurve(curve, 1.0f, kStartOutput));
    EXPECT_TRUE(CheckCurve(curve, 1.2f, kStartOutput + kOutputRange * 0.2f));
    EXPECT_TRUE(CheckCurve(curve, 1.5f, kStartOutput + kOutputRange * 0.5f));
    EXPECT_TRUE(CheckCurve(curve, 1.7f, kStartOutput + kOutputRange * 0.7f));
    EXPECT_TRUE(CheckCurve(curve, 2.0f, kEndOutput));
    curve->set_use_cache(true);
  }
}

// Tests Evaluate for Pre/Post Infintiy.
TEST_F(CurveTest, EvaluateInfinity) {
  Curve* curve = pack()->Create<Curve>();
  ASSERT_TRUE(curve != NULL);

  // Try 2 keys
  CurveKey* key1 = curve->Create<LinearCurveKey>();
  CurveKey* key2 = curve->Create<LinearCurveKey>();
  ASSERT_TRUE(key1 != NULL);
  ASSERT_TRUE(key2 != NULL);

  const float kStartInput = 1.0f;
  const float kStartOutput = 1.0f;
  const float kEndInput = 2.0f;
  const float kEndOutput = 5.0f;
  const float kOutputRange = kEndOutput - kStartOutput;

  key1->SetOutput(kStartOutput);
  key1->SetInput(kStartInput);
  key2->SetOutput(kEndOutput);
  key2->SetInput(kEndInput);

  EXPECT_TRUE(CheckCurve(curve, -1.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, 0.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, 1.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, 1.2f, kStartOutput + kOutputRange * 0.2f));
  EXPECT_TRUE(CheckCurve(curve, 1.5f, kStartOutput + kOutputRange * 0.5f));
  EXPECT_TRUE(CheckCurve(curve, 1.7f, kStartOutput + kOutputRange * 0.7f));
  EXPECT_TRUE(CheckCurve(curve, 2.0f, kEndOutput));
  EXPECT_TRUE(CheckCurve(curve, 3.0f, kEndOutput));

  // Change Pre-Infinity to LINEAR
  curve->set_pre_infinity(Curve::LINEAR);
  EXPECT_TRUE(CheckCurve(curve, -1.5f, kStartOutput - kOutputRange * 2.5f));
  EXPECT_TRUE(CheckCurve(curve, -1.0f, kStartOutput - kOutputRange * 2.0f));
  EXPECT_TRUE(CheckCurve(curve, 0.0f, kStartOutput - kOutputRange));
  EXPECT_TRUE(CheckCurve(curve, 1.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, 1.2f, kStartOutput + kOutputRange * 0.2f));
  EXPECT_TRUE(CheckCurve(curve, 1.5f, kStartOutput + kOutputRange * 0.5f));
  EXPECT_TRUE(CheckCurve(curve, 1.7f, kStartOutput + kOutputRange * 0.7f));
  EXPECT_TRUE(CheckCurve(curve, 2.0f, kEndOutput));
  EXPECT_TRUE(CheckCurve(curve, 3.0f, kEndOutput));

  // Change Post-Infinity to LINEAR
  curve->set_post_infinity(Curve::LINEAR);
  EXPECT_TRUE(CheckCurve(curve, -1.5f, kStartOutput - kOutputRange * 2.5f));
  EXPECT_TRUE(CheckCurve(curve, -1.0f, kStartOutput - kOutputRange * 2.0f));
  EXPECT_TRUE(CheckCurve(curve, 0.0f, kStartOutput - kOutputRange));
  EXPECT_TRUE(CheckCurve(curve, 1.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, 1.2f, kStartOutput + kOutputRange * 0.2f));
  EXPECT_TRUE(CheckCurve(curve, 1.5f, kStartOutput + kOutputRange * 0.5f));
  EXPECT_TRUE(CheckCurve(curve, 1.7f, kStartOutput + kOutputRange * 0.7f));
  EXPECT_TRUE(CheckCurve(curve, 2.0f, kEndOutput));
  EXPECT_TRUE(CheckCurve(curve, 3.0f, kEndOutput + kOutputRange));
  EXPECT_TRUE(CheckCurve(curve, 4.0f, kEndOutput + kOutputRange * 2.0f));
  EXPECT_TRUE(CheckCurve(curve, 4.5f, kEndOutput + kOutputRange * 2.5f));

  // Change Pre-Infinity to CYCLE
  curve->set_pre_infinity(Curve::CYCLE);
  EXPECT_TRUE(CheckCurve(curve, -1.2f, kStartOutput + kOutputRange * 0.8f));
  EXPECT_TRUE(CheckCurve(curve, -1.5f, kStartOutput + kOutputRange * 0.5f));
  EXPECT_TRUE(CheckCurve(curve, -1.7f, kStartOutput + kOutputRange * 0.3f));
  EXPECT_TRUE(CheckCurve(curve, -1.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, 0.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, 1.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, 1.2f, kStartOutput + kOutputRange * 0.2f));
  EXPECT_TRUE(CheckCurve(curve, 1.5f, kStartOutput + kOutputRange * 0.5f));
  EXPECT_TRUE(CheckCurve(curve, 1.7f, kStartOutput + kOutputRange * 0.7f));
  EXPECT_TRUE(CheckCurve(curve, 2.0f, kEndOutput));
  EXPECT_TRUE(CheckCurve(curve, 3.0f, kEndOutput + kOutputRange));
  EXPECT_TRUE(CheckCurve(curve, 4.0f, kEndOutput + kOutputRange * 2.0f));
  EXPECT_TRUE(CheckCurve(curve, 4.5f, kEndOutput + kOutputRange * 2.5f));

  // Change Post-Infinity to CYCLE
  curve->set_post_infinity(Curve::CYCLE);
  EXPECT_TRUE(CheckCurve(curve, -1.2f, kStartOutput + kOutputRange * 0.8f));
  EXPECT_TRUE(CheckCurve(curve, -1.5f, kStartOutput + kOutputRange * 0.5f));
  EXPECT_TRUE(CheckCurve(curve, -1.7f, kStartOutput + kOutputRange * 0.3f));
  EXPECT_TRUE(CheckCurve(curve, -1.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, 0.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, 1.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, 1.2f, kStartOutput + kOutputRange * 0.2f));
  EXPECT_TRUE(CheckCurve(curve, 1.5f, kStartOutput + kOutputRange * 0.5f));
  EXPECT_TRUE(CheckCurve(curve, 1.7f, kStartOutput + kOutputRange * 0.7f));
  EXPECT_TRUE(CheckCurve(curve, 2.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, 3.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, 4.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, 4.2f, kStartOutput + kOutputRange * 0.2f));
  EXPECT_TRUE(CheckCurve(curve, 4.5f, kStartOutput + kOutputRange * 0.5f));
  EXPECT_TRUE(CheckCurve(curve, 4.7f, kStartOutput + kOutputRange * 0.7f));

  // Change Pre-Infinity to OSCILLATE
  curve->set_pre_infinity(Curve::OSCILLATE);
  EXPECT_TRUE(CheckCurve(curve, -1.7f, kStartOutput + kOutputRange * 0.7f));
  EXPECT_TRUE(CheckCurve(curve, -1.5f, kStartOutput + kOutputRange * 0.5f));
  EXPECT_TRUE(CheckCurve(curve, -1.2f, kStartOutput + kOutputRange * 0.2f));
  EXPECT_TRUE(CheckCurve(curve, -1.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, -0.7f, kStartOutput + kOutputRange * 0.3f));
  EXPECT_TRUE(CheckCurve(curve, -0.5f, kStartOutput + kOutputRange * 0.5f));
  EXPECT_TRUE(CheckCurve(curve, -0.2f, kStartOutput + kOutputRange * 0.8f));
  EXPECT_TRUE(CheckCurve(curve, 0.0f, kEndOutput));
  EXPECT_TRUE(CheckCurve(curve, 1.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, 1.2f, kStartOutput + kOutputRange * 0.2f));
  EXPECT_TRUE(CheckCurve(curve, 1.5f, kStartOutput + kOutputRange * 0.5f));
  EXPECT_TRUE(CheckCurve(curve, 1.7f, kStartOutput + kOutputRange * 0.7f));
  EXPECT_TRUE(CheckCurve(curve, 2.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, 3.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, 4.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, 4.2f, kStartOutput + kOutputRange * 0.2f));
  EXPECT_TRUE(CheckCurve(curve, 4.5f, kStartOutput + kOutputRange * 0.5f));
  EXPECT_TRUE(CheckCurve(curve, 4.7f, kStartOutput + kOutputRange * 0.7f));

  // Change Post-Infinity to OSCILLATE
  curve->set_post_infinity(Curve::OSCILLATE);
  EXPECT_TRUE(CheckCurve(curve, -1.7f, kStartOutput + kOutputRange * 0.7f));
  EXPECT_TRUE(CheckCurve(curve, -1.5f, kStartOutput + kOutputRange * 0.5f));
  EXPECT_TRUE(CheckCurve(curve, -1.2f, kStartOutput + kOutputRange * 0.2f));
  EXPECT_TRUE(CheckCurve(curve, -1.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, -0.7f, kStartOutput + kOutputRange * 0.3f));
  EXPECT_TRUE(CheckCurve(curve, -0.5f, kStartOutput + kOutputRange * 0.5f));
  EXPECT_TRUE(CheckCurve(curve, -0.2f, kStartOutput + kOutputRange * 0.8f));
  EXPECT_TRUE(CheckCurve(curve, 0.0f, kEndOutput));
  EXPECT_TRUE(CheckCurve(curve, 1.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, 1.2f, kStartOutput + kOutputRange * 0.2f));
  EXPECT_TRUE(CheckCurve(curve, 1.5f, kStartOutput + kOutputRange * 0.5f));
  EXPECT_TRUE(CheckCurve(curve, 1.7f, kStartOutput + kOutputRange * 0.7f));
  EXPECT_TRUE(CheckCurve(curve, 2.0f, kEndOutput));
  EXPECT_TRUE(CheckCurve(curve, 3.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, 3.2f, kStartOutput + kOutputRange * 0.2f));
  EXPECT_TRUE(CheckCurve(curve, 3.5f, kStartOutput + kOutputRange * 0.5f));
  EXPECT_TRUE(CheckCurve(curve, 3.7f, kStartOutput + kOutputRange * 0.7f));
  EXPECT_TRUE(CheckCurve(curve, 4.0f, kEndOutput));
  EXPECT_TRUE(CheckCurve(curve, 4.2f, kStartOutput + kOutputRange * 0.8f));
  EXPECT_TRUE(CheckCurve(curve, 4.5f, kStartOutput + kOutputRange * 0.5f));
  EXPECT_TRUE(CheckCurve(curve, 4.7f, kStartOutput + kOutputRange * 0.3f));

  // Check Discontinuous Curves.
  CurveKey* key3 = curve->Create<LinearCurveKey>();
  CurveKey* key4 = curve->Create<LinearCurveKey>();
  ASSERT_TRUE(key3 != NULL);
  ASSERT_TRUE(key4 != NULL);

  const float kMidInput = 1.5f;
  const float kPreMidOutput = 3.0f;
  const float kPostMidOutput = 0.0f;
  const float kMidStartRange = kPreMidOutput - kStartOutput;
  const float kMidEndRange = kEndOutput - kPostMidOutput;

  // 5|         E
  // 4|        /
  // 3|   PRE /
  // 2|  / | /
  // 1| S  |/
  // 0|   POST
  // -+-----------
  //    1 0.5   2

  key3->SetOutput(kPreMidOutput);
  key3->SetInput(kMidInput);
  key4->SetOutput(kPostMidOutput);
  key4->SetInput(kMidInput);

  const float O2Output = kStartOutput + kMidStartRange * 0.2f * 2.0f;
  const float O3Output = kStartOutput + kMidStartRange * 0.3f * 2.0f;
  const float O49Output = kStartOutput + kMidStartRange * 0.49999f * 2.0f;
  const float O7Output = kPostMidOutput + kMidEndRange * 0.2f * 2.0f;
  const float O8Output = kPostMidOutput + kMidEndRange * 0.3f * 2.0f;

  EXPECT_TRUE(CheckCurve(curve, 1.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, kStartInput + 0.2f, O2Output));
  EXPECT_TRUE(CheckCurve(curve, kStartInput + 0.3f, O3Output));
  EXPECT_TRUE(CheckCurve(curve, kStartInput + 0.49999f, O49Output));
  EXPECT_TRUE(CheckCurve(curve, kStartInput + 0.5f, kPostMidOutput));
  EXPECT_TRUE(CheckCurve(curve, kStartInput + 0.7f, O7Output));
  EXPECT_TRUE(CheckCurve(curve, kStartInput + 0.8f, O8Output));
  EXPECT_TRUE(CheckCurve(curve, 2.0f, kEndOutput));

  // Check Pre-Infinity CYCLE_RELATIVE.
  curve->set_pre_infinity(Curve::CYCLE_RELATIVE);
  EXPECT_TRUE(CheckCurve(curve, -2.0f + 0.2f, O2Output - kOutputRange * 3.0f));
  EXPECT_TRUE(CheckCurve(curve, -2.0f + 0.3f, O3Output - kOutputRange * 3.0f));
  EXPECT_TRUE(CheckCurve(curve,
                         -2.0f + 0.49999f,
                         O49Output - kOutputRange * 3.0f));
  EXPECT_TRUE(CheckCurve(curve,
                         -2.0f + 0.5f,
                         kPostMidOutput - kOutputRange * 3.0f));
  EXPECT_TRUE(CheckCurve(curve, -2.0f + 0.7f, O7Output - kOutputRange * 3.0f));
  EXPECT_TRUE(CheckCurve(curve, -2.0f + 0.8f, O8Output - kOutputRange * 3.0f));
  EXPECT_TRUE(CheckCurve(curve, -1.0f, kStartOutput - kOutputRange * 2.0f));
  EXPECT_TRUE(CheckCurve(curve, 0.0f, kStartOutput - kOutputRange * 1.0f));
  EXPECT_TRUE(CheckCurve(curve, 1.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, kStartInput + 0.2f, O2Output));
  EXPECT_TRUE(CheckCurve(curve, kStartInput + 0.3f, O3Output));
  EXPECT_TRUE(CheckCurve(curve, kStartInput + 0.49999f, O49Output));
  EXPECT_TRUE(CheckCurve(curve, kStartInput + 0.5f, kPostMidOutput));
  EXPECT_TRUE(CheckCurve(curve, kStartInput + 0.7f, O7Output));
  EXPECT_TRUE(CheckCurve(curve, kStartInput + 0.8f, O8Output));
  EXPECT_TRUE(CheckCurve(curve, 2.0f, kEndOutput));
  EXPECT_TRUE(CheckCurve(curve, 3.0f, kStartOutput));
  // still oscillate..
  EXPECT_TRUE(CheckCurve(curve, 3.2f, O2Output));
  EXPECT_TRUE(CheckCurve(curve, 3.5f, kPostMidOutput));
  EXPECT_TRUE(CheckCurve(curve, 3.7f, O7Output));
  EXPECT_TRUE(CheckCurve(curve, 4.0f, kEndOutput));
  EXPECT_TRUE(CheckCurve(curve, 4.2f, O8Output));
  EXPECT_TRUE(CheckCurve(curve, 4.5f, kPostMidOutput));
  EXPECT_TRUE(CheckCurve(curve, 4.7f, O3Output));

  // Check Post-Infinity CYCLE_RELATIVE.
  curve->set_post_infinity(Curve::CYCLE_RELATIVE);
  EXPECT_TRUE(CheckCurve(curve, -2.0f + 0.2f, O2Output - kOutputRange * 3.0f));
  EXPECT_TRUE(CheckCurve(curve, -2.0f + 0.3f, O3Output - kOutputRange * 3.0f));
  EXPECT_TRUE(CheckCurve(curve,
                         -2.0f + 0.49999f,
                         O49Output - kOutputRange * 3.0f));
  EXPECT_TRUE(CheckCurve(curve,
                         -2.0f + 0.5f,
                         kPostMidOutput - kOutputRange * 3.0f));
  EXPECT_TRUE(CheckCurve(curve, -2.0f + 0.7f, O7Output - kOutputRange * 3.0f));
  EXPECT_TRUE(CheckCurve(curve, -2.0f + 0.8f, O8Output - kOutputRange * 3.0f));
  EXPECT_TRUE(CheckCurve(curve, -1.0f, kStartOutput - kOutputRange * 2.0f));
  EXPECT_TRUE(CheckCurve(curve, 0.0f, kStartOutput - kOutputRange * 1.0f));
  EXPECT_TRUE(CheckCurve(curve, 1.0f, kStartOutput));
  EXPECT_TRUE(CheckCurve(curve, kStartInput + 0.2f, O2Output));
  EXPECT_TRUE(CheckCurve(curve, kStartInput + 0.3f, O3Output));
  EXPECT_TRUE(CheckCurve(curve, kStartInput + 0.49999f, O49Output));
  EXPECT_TRUE(CheckCurve(curve, kStartInput + 0.5f, kPostMidOutput));
  EXPECT_TRUE(CheckCurve(curve, kStartInput + 0.7f, O7Output));
  EXPECT_TRUE(CheckCurve(curve, kStartInput + 0.8f, O8Output));
  EXPECT_TRUE(CheckCurve(curve, 2.0f, kEndOutput));
  EXPECT_TRUE(CheckCurve(curve, 3.0f, kEndOutput + kOutputRange * 1.0f));
  EXPECT_TRUE(CheckCurve(curve, 4.0f, kEndOutput + kOutputRange * 2.0f));
  EXPECT_TRUE(CheckCurve(curve, 4.0f + 0.2f, O2Output + kOutputRange * 3.0f));
  EXPECT_TRUE(CheckCurve(curve, 5.0f + 0.2f, O2Output + kOutputRange * 4.0f));
  EXPECT_TRUE(CheckCurve(curve, 5.0f + 0.3f, O3Output + kOutputRange * 4.0f));
  EXPECT_TRUE(CheckCurve(curve,
                         5.0f + 0.49999f,
                         O49Output + kOutputRange * 4.0f));
  EXPECT_TRUE(CheckCurve(curve,
                         5.0f + 0.5f,
                         kPostMidOutput + kOutputRange * 4.0f));
  EXPECT_TRUE(CheckCurve(curve, 5.0f + 0.7f, O7Output + kOutputRange * 4.0f));
  EXPECT_TRUE(CheckCurve(curve, 5.0f + 0.8f, O8Output + kOutputRange * 4.0f));
}

namespace {
struct BezierKey {
  float input;
  float output;
  float in_tangent_x;
  float in_tangent_y;
  float out_tangent_x;
  float out_tangent_y;
};

struct ExpectedResult {
  float input;  // in 24hz frames
  float output;
  float maya_output;
};

struct KeyInfo {
  unsigned num_keys;
  const BezierKey* keys;
  unsigned num_tests;
  const ExpectedResult* expected_results;
};
}  // anonymous namespace

static const BezierKey bezier_data_0[] = {
  { 0.083333f, 3, 0.013888f, -2.66667f, 0.152778f, 8.66667f, },
  { 0.291667f, 20, 0.222222f, -9.22377f, 0.388889f, 60.9133f, },
  { 0.583333f, -5, 0.486111f, 3.33333f, 0.680556f, -13.3333f, }
};

static const BezierKey bezier_data_1[] = {
  { 0.083333f, 3, 0.013888f, -2.66667f, 0.152778f, 8.66667f, },
  { 0.291667f, 20, 0.222222f, -9.22377f, 0.740598f, 19.9773f, },
  { 0.583333f, -5, 0.486111f, 3.33333f, 0.680556f, -13.3333f, },
};

static const ExpectedResult expected_results_0[] = {
  { 2.00f, 3.0524368f, 3.000000f, },
  { 3.00f, 4.1385164f, 4.139000f, },
  { 4.00f, 3.0155535f, 3.016000f, },
  { 5.00f, 3.0233288f, 3.023000f, },
  { 6.00f, 7.5540328f, 7.554000f, },
  { 7.00f, 19.727404f, 20.00000f, },
  { 8.00f, 31.934761f, 31.93500f, },
  { 9.00f, 34.393578f, 34.39400f, },
  { 10.0f, 29.960693f, 29.96100f, },
  { 11.0f, 21.220503f, 21.22100f, },
  { 12.0f, 10.757395f, 10.75700f, },
  { 13.0f, 1.1557499f, 1.156000f, },
  { 14.0f, -5.000000f, -5.00000f, },
};

static const ExpectedResult expected_results_1[] = {
  { 2.00f, 3.0524368f, 3.000000f, },
  { 3.00f, 4.1385164f, 4.139000f, },
  { 4.00f, 3.0155535f, 3.016000f, },
  { 5.00f, 3.0233288f, 3.023000f, },
  { 6.00f, 7.5540328f, 7.554000f, },
  { 7.00f, 19.727404f, 20.00000f, },
  { 8.00f, 19.946407f, 19.92800f, },
  { 9.00f, 19.764795f, 19.68200f, },
  { 10.0f, 19.417355f, 19.18900f, },
  { 11.0f, 18.804613f, 18.29900f, },  // TODO: fix so these match.
  { 12.0f, 17.725473f, 16.63900f, },  // notice these numbers don't match maya
  { 13.0f, 15.654317f, 12.23100f, },  // notice these numbers don't match maya
  { 14.0f, -5.000000f, -5.00000f, },
};

TEST_F(CurveTest, EvaluateBezierCurveKey) {
  const float kFrameRate = 24.0f;

  static const KeyInfo key_infos[] = {
    { arraysize(bezier_data_0), bezier_data_0,
      arraysize(expected_results_0), expected_results_0, },
    { arraysize(bezier_data_1), bezier_data_1,
      arraysize(expected_results_1), expected_results_1, },
  };

  for (unsigned tt = 0; tt < arraysize(key_infos); ++tt) {
    Curve* curve = pack()->Create<Curve>();
    ASSERT_TRUE(curve != NULL);

    // No cache since we want to evaluate the actual curves.
    curve->set_use_cache(false);

    const KeyInfo& key_info = key_infos[tt];

    for (unsigned ii = 0; ii < key_info.num_keys; ++ii) {
      const BezierKey& bezier_key = key_info.keys[ii];
      BezierCurveKey* key = curve->Create<BezierCurveKey>();
      key->SetInput(bezier_key.input);
      key->SetOutput(bezier_key.output);
      key->SetInTangent(Float2(bezier_key.in_tangent_x,
                               bezier_key.in_tangent_y));
      key->SetOutTangent(Float2(bezier_key.out_tangent_x,
                                bezier_key.out_tangent_y));
    }

    for (unsigned ii = 0; ii < key_info.num_tests; ++ii) {
      const ExpectedResult& expected_result = key_info.expected_results[ii];
      EXPECT_TRUE(CheckCurve(curve,
                             expected_result.input / kFrameRate,
                             expected_result.output));
    }
  }
}

// Tests the CurveFunctionContext in use.
TEST_F(CurveTest, CurveFunctionContext) {
  Curve* curve = pack()->Create<Curve>();
  ASSERT_TRUE(curve != NULL);

  FunctionContext* context = curve->CreateFunctionContext();
  ASSERT_TRUE(context != NULL);

  // Create keys
  for (unsigned ii = 0; ii < 20; ++ii) {
    CurveKey* key = curve->Create<LinearCurveKey>();
    ASSERT_TRUE(key != NULL);
    key->SetInput(static_cast<float>(ii));
    key->SetOutput(static_cast<float>(ii * 2));
  }

  // Check the curve with linear access.
  for (unsigned ii = 0; ii < 39; ++ii) {
    EXPECT_TRUE(CheckCurveWithContext(curve,
                                      static_cast<float>(ii) * 0.5f,
                                      static_cast<float>(ii * 2) * 0.5f,
                                      context));
  }

  // Check the curve with semi random access.
  unsigned jj = 0;
  for (unsigned ii = 0; ii < 50; ++ii) {
    jj = (jj + 17) % 39;
    EXPECT_TRUE(CheckCurveWithContext(curve,
                                      static_cast<float>(jj) * 0.5f,
                                      static_cast<float>(jj * 2) * 0.5f,
                                      context));
  }

  delete context;
}


// Tests loading Curve from binary data

// Sanity check on empty data
TEST_F(CurveTest, CurveRawDataEmpty) {
  Curve* curve = pack()->Create<Curve>();
  ASSERT_TRUE(curve != NULL);

  uint8 p[2];
  MemoryReadStream read_stream(p, 0);  // empty stream

  bool success = curve->LoadFromBinaryData(&read_stream);

  // Make sure we don't like to load from empty data
  EXPECT_FALSE(success);
}

// Sanity check on corrupt data
TEST_F(CurveTest, CurveRawDataCorrupt) {
  Curve* curve = pack()->Create<Curve>();
  ASSERT_TRUE(curve != NULL);

  const int kDataLength = 256;
  uint8 p[kDataLength];
  for (int i = 0; i < kDataLength; ++i) p[i] = i;  // not valid curve data

  MemoryReadStream read_stream(p, kDataLength);

  bool success = curve->LoadFromBinaryData(&read_stream);

  // Make sure we don't like to load from corrupt data
  EXPECT_FALSE(success);
}

// Sanity check on incomplete data
TEST_F(CurveTest, CurveRawDataIncomplete) {
  Curve* curve = pack()->Create<Curve>();
  ASSERT_TRUE(curve != NULL);

  const int kDataLength = 512;  // enough storage for test
  MemoryBuffer<uint8> buffer(kDataLength);
  MemoryWriteStream write_stream(buffer, kDataLength);
  write_stream.WriteLittleEndianInt32(1);  // version 1
  write_stream.WriteByte(3);  // bezier
  write_stream.WriteLittleEndianFloat32(3.4f);
  write_stream.WriteLittleEndianFloat32(1.7f);
  // but DON'T write the tangent data

  // make note of amount we've written
  size_t data_size = write_stream.GetStreamPosition();

  // try to load what we just created
  MemoryReadStream read_stream(buffer, data_size);
  bool success = curve->LoadFromBinaryData(&read_stream);

  // Make sure we don't like to load from incomplete data
  EXPECT_FALSE(success);
}

// Check that valid curve data loads OK
TEST_F(CurveTest, CurveRawDataValid) {
  Curve* curve = pack()->Create<Curve>();
  ASSERT_TRUE(curve != NULL);

  // No cache since we want to evaluate the actual curves.
  curve->set_use_cache(false);

  const int kDataLength = 512;  // enough storage for test
  MemoryBuffer<uint8> buffer(kDataLength);
  MemoryWriteStream write_stream(buffer, kDataLength);

  // write out serialization ID
  write_stream.Write(Curve::kSerializationID, 4);

  // write out version
  write_stream.WriteLittleEndianInt32(1);

  // Write out some bezier data (one that we tested above)
  size_t n = arraysize(bezier_data_0);

  for (size_t i = 0; i < n; ++i) {
    const BezierKey &key = bezier_data_0[i];

    write_stream.WriteByte(3);  // bezier
    write_stream.WriteLittleEndianFloat32(key.input);
    write_stream.WriteLittleEndianFloat32(key.output);
    write_stream.WriteLittleEndianFloat32(key.in_tangent_x);
    write_stream.WriteLittleEndianFloat32(key.in_tangent_y);
    write_stream.WriteLittleEndianFloat32(key.out_tangent_x);
    write_stream.WriteLittleEndianFloat32(key.out_tangent_y);
  }

  // Make note of amount we've written
  size_t data_size = write_stream.GetStreamPosition();

  // Try to load what we just created
  MemoryReadStream read_stream(buffer, data_size);
  bool success = curve->LoadFromBinaryData(&read_stream);

  // Make sure curve data was accepted
  EXPECT_TRUE(success);

  // Validate some test points on curve
  size_t num_tests = arraysize(expected_results_0);
  const float kFrameRate = 24.0f;
  for (unsigned ii = 0; ii < num_tests; ++ii) {
    const ExpectedResult& expected_result = expected_results_0[ii];
    EXPECT_TRUE(CheckCurve(curve,
                           expected_result.input / kFrameRate,
                           expected_result.output));
  }

  // Now, let's try a very nice test to verify that we properly
  // serialize -- this is a round trip test
  MemoryBuffer<uint8> serialized_data;
  SerializeCurve(*curve, &serialized_data);

  // Make sure serialized data length is identical to what we made
  ASSERT_EQ(data_size, serialized_data.GetLength());

  // Make sure the data matches
  uint8 *original = buffer;
  uint8 *serialized = serialized_data;
  EXPECT_EQ(0, memcmp(original, serialized, data_size));
}

}  // namespace o3d
