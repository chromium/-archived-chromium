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


// This file implements unit tests for class Function and FunctionEval.

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/function.h"

namespace o3d {

class FunctionTest : public testing::Test {
 protected:

  FunctionTest()
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

void FunctionTest::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void FunctionTest::TearDown() {
  pack_->Destroy();
}

namespace {

// Just multiplies the input by 2.0f
class TestFunction : public Function {
 public:
  explicit TestFunction(ServiceLocator* service_locator)
      : Function(service_locator) {
  }

  // overridden from Function
  virtual float Evaluate(float input, FunctionContext* context) const {
    return input * 2.0f;
  }

  // overridden from Function
  virtual FunctionContext* CreateFunctionContext() const {
    return NULL;
  }

  // overridden from Function
  virtual const ObjectBase::Class* GetFunctionContextClass() const {
    return NULL;
  }

 private:
  O3D_DECL_CLASS(TestFunction, Function);
  DISALLOW_COPY_AND_ASSIGN(TestFunction);
};

O3D_DEFN_CLASS(TestFunction, Function);

}  // anonymous namespace.

// Tests Function.
TEST_F(FunctionTest, Basic) {
  Function::Ref function = Function::Ref(new TestFunction(g_service_locator));
  // Check that it got created.
  ASSERT_FALSE(function.IsNull());
  // Check that it derives from NamedObject
  EXPECT_TRUE(function->IsA(NamedObject::GetApparentClass()));
}

// Tests Evaluate.
TEST_F(FunctionTest, Evaluate) {
  Function::Ref function = Function::Ref(new TestFunction(g_service_locator));
  // Check that it got created.
  ASSERT_FALSE(function.IsNull());

  EXPECT_EQ(function->Evaluate(2.0f, NULL), 2.0f * 2.0f);
  EXPECT_EQ(function->Evaluate(4.0f, NULL), 4.0f * 2.0f);
  EXPECT_EQ(function->Evaluate(-4.0f, NULL), -4.0f * 2.0f);
}

class FunctionEvalTest : public testing::Test {
 protected:

  FunctionEvalTest()
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

void FunctionEvalTest::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void FunctionEvalTest::TearDown() {
  pack_->Destroy();
}

// Tests FunctionEval.
TEST_F(FunctionEvalTest, Basic) {
  FunctionEval* function_eval = pack()->Create<FunctionEval>();
  // Check that it got created.
  ASSERT_TRUE(function_eval != NULL);
  // Check that it derives from ParamObject
  EXPECT_TRUE(function_eval->IsA(ParamObject::GetApparentClass()));
}

// Tests UpdateOutputs (indirectly)
TEST_F(FunctionEvalTest, Evaluate) {
  FunctionEval* function_eval = pack()->Create<FunctionEval>();
  // Check that it got created.
  ASSERT_TRUE(function_eval != NULL);

  // Check that the correct params got created.
  EXPECT_TRUE(function_eval->GetParam<ParamFloat>(
                  FunctionEval::kInputParamName) != NULL);
  EXPECT_TRUE(function_eval->GetParam<ParamFunction>(
                  FunctionEval::kFunctionObjectParamName) != NULL);
  EXPECT_TRUE(function_eval->GetParam<ParamFloat>(
                  FunctionEval::kOutputParamName) != NULL);

  // Check that with no function the input just gets passed through.
  function_eval->set_input(2.0f);
  EXPECT_EQ(function_eval->input(), 2.0f);
  EXPECT_EQ(function_eval->output(), 2.0f);
  function_eval->set_input(4.0f);
  EXPECT_EQ(function_eval->input(), 4.0f);
  EXPECT_EQ(function_eval->output(), 4.0f);

  // Attach a function
  Function::Ref function = Function::Ref(new TestFunction(g_service_locator));
  // Check that it got created.
  ASSERT_FALSE(function.IsNull());

  function_eval->set_function_object(function);
  EXPECT_EQ(function_eval->function_object(), function);

  function_eval->set_input(2.0f);
  EXPECT_EQ(function_eval->output(), 4.0f);
  function_eval->set_input(4.0f);
  EXPECT_EQ(function_eval->output(), 8.0f);
}

}  // namespace o3d
