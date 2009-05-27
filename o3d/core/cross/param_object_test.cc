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


// Tests functionality of the ParamObject class

#include <algorithm>
#include "core/cross/param_object.h"
#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/error.h"

namespace o3d {

// Test fixture for ParamObject testing. Creates a Client object
// and a ParamObject before each test and deletes it after
class ParamObjectTest : public testing::Test {
 protected:

  ParamObjectTest()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }
  ParamObject* param_obj() { return param_obj_; }

  ServiceDependency<ObjectManager> object_manager_;

 private:
  Pack* pack_;
  ParamObject* param_obj_;
};

void ParamObjectTest::SetUp() {
  pack_ = object_manager_->CreatePack();

  param_obj_ = pack()->Create<Transform>();
}

void ParamObjectTest::TearDown() {
  pack_->Destroy();
}

namespace {

class FakeParam : public Param {
 public:
  explicit FakeParam(ServiceLocator* service_locator)
      : Param(service_locator, false, false) {
  }

  virtual void CopyDataFromParam(Param* source_param) {
    // Do nothing. We are fake.
  }
};

class FakeDivModParamOperation : public ParamObject {
 public:
  typedef SmartPointer<FakeDivModParamOperation> Ref;

  static const char* kInput1Name;
  static const char* kInput2Name;
  static const char* kOutput1Name;
  static const char* kOutput2Name;

  explicit FakeDivModParamOperation(ServiceLocator* service_locator);

  void UpdateOutputs();

  int input1() const {
    return input1_param_->value();
  }

  void set_input1(int value) {
    input1_param_->set_value(value);
  }

  int input2() const {
    return input2_param_->value();
  }

  void set_input2(int value) {
    input2_param_->set_value(value);
  }

  int output1() const {
    return output1_param_->value();
  }

  int output2() const {
    return output2_param_->value();
  }

  unsigned NumberOfCallsToUpdateOutputs() const {
    return update_outputs_call_count_;
  }

 protected:
  // Overridden from ParamObject
  // For the given Param, returns all the inputs that affect that param through
  // this ParamObject.
  virtual void ConcreteGetInputsForParam(const Param* param,
                                         ParamVector* inputs) const;

  // Overridden from ParamObject
  // For the given Param, returns all the outputs that the given param will
  // affect through this ParamObject.
  virtual void ConcreteGetOutputsForParam(const Param* param,
                                          ParamVector* outputs) const;

 private:
  typedef SlaveParam<ParamInteger, FakeDivModParamOperation> SlaveParamInteger;

  ParamInteger::Ref input1_param_;
  ParamInteger::Ref input2_param_;
  SlaveParamInteger::Ref output1_param_;
  SlaveParamInteger::Ref output2_param_;

  unsigned update_outputs_call_count_;

  O3D_DECL_CLASS(FakeDivModParamOperation, ParamObject);
  DISALLOW_COPY_AND_ASSIGN(FakeDivModParamOperation);
};

O3D_DEFN_CLASS(FakeDivModParamOperation, ParamObject);

const char* FakeDivModParamOperation::kInput1Name = "input1";
const char* FakeDivModParamOperation::kInput2Name = "input2";
const char* FakeDivModParamOperation::kOutput1Name = "output1";
const char* FakeDivModParamOperation::kOutput2Name = "output2";

FakeDivModParamOperation::FakeDivModParamOperation(
    ServiceLocator* service_locator)
    : ParamObject(service_locator),
      update_outputs_call_count_(0) {
  RegisterParamRef(kInput1Name, &input1_param_);
  RegisterParamRef(kInput2Name, &input2_param_);
  SlaveParamInteger::RegisterParamRef(kOutput1Name, &output1_param_, this);
  SlaveParamInteger::RegisterParamRef(kOutput2Name, &output2_param_, this);
}

void FakeDivModParamOperation::ConcreteGetInputsForParam(
    const Param* param,
    ParamVector* inputs) const {
  if ((param == output1_param_ &&
       output1_param_->input_connection() == NULL) ||
      (param == output2_param_ &&
       output2_param_->input_connection() == NULL)) {
    inputs->push_back(input1_param_);
    inputs->push_back(input2_param_);
  }
}

void FakeDivModParamOperation::ConcreteGetOutputsForParam(
    const Param* param,
    ParamVector* outputs) const {
  if (param == input1_param_ || param == input2_param_) {
    if (output1_param_->input_connection() == NULL) {
      outputs->push_back(output1_param_);
    }
    if (output2_param_->input_connection() == NULL) {
      outputs->push_back(output2_param_);
    }
  }
}

void FakeDivModParamOperation::UpdateOutputs() {
  ++update_outputs_call_count_;

  int input1 = input1_param_->value();
  int input2 = input2_param_->value();

  if (input2 != 0) {
    output1_param_->set_dynamic_value(input1 / input2);
    output2_param_->set_dynamic_value(input1 % input2);
  } else {
    O3D_ERROR(g_service_locator) << "divide by zero in '" << name() << "'";
  }
}

// Check if a param is in the given param array
// Parameters:
//   param: param to search for.
//   params: ParamVector to search.
// Returns:
//   true if param is in params.
bool ParamInParams(Param* param, const ParamVector& params) {
  return std::find(params.begin(), params.end(), param) != params.end();
}

}  // anonymous namespace

// Test ParamObject::AddParam().
TEST_F(ParamObjectTest, AddParam) {
  using o3d::NamedParamRefMap;

  Param* param = new FakeParam(g_service_locator);
  Param* param2 = new FakeParam(g_service_locator);
  param_obj()->AddParam("param", param);

  // Make sure the param ends up in the Param obj's Param Map.
  const NamedParamRefMap& param_map = param_obj()->params();
  NamedParamRefMap::const_iterator pos = param_map.find("param");
  EXPECT_TRUE(pos != param_map.end());

  // Make sure the param ends up in the client as well.
  EXPECT_TRUE(object_manager_->GetById<Param>(param->id()) == param);

  // Make sure if we add another of the same name it fails.
  EXPECT_FALSE(param_obj()->AddParam("param", param2));

  // Make sure the old param was uneffected.
  pos = param_map.find("param");
  EXPECT_TRUE(pos != param_map.end());
  EXPECT_EQ(param, param_obj()->GetUntypedParam("param"));

  // Note: Param is owned by the ParamObject now so we don't delete it here.
}

// Test ParamObject::RemoveParam().
TEST_F(ParamObjectTest, RemoveParam) {
  Param::Ref param(param_obj()->CreateParam<ParamFloat>("param"));
  ASSERT_FALSE(param.IsNull());

  // Should be able to remove it
  EXPECT_TRUE(param_obj()->RemoveParam(param));
  // Should not be able to remove it twice.
  EXPECT_FALSE(param_obj()->RemoveParam(param));
}

// Test ParamObject::CreateParam().
TEST_F(ParamObjectTest, CreateParam) {
  using o3d::NamedParamRefMap;

  Param *param = param_obj()->CreateParam<ParamFloat>("param");

  // Make sure the param ends up in the Param obj's Param Map.
  NamedParamRefMap param_map = param_obj()->params();
  NamedParamRefMap::const_iterator pos = param_map.find("param");
  EXPECT_TRUE(pos != param_map.end());

  // Make sure the param ends up in the client as well.
  EXPECT_TRUE(object_manager_->GetById<Param>(param->id()) == param);

  // Note: Param is owned by the ParamObject now so we don't delete it here.
}

// Test ParamObject::CreateParam().
TEST_F(ParamObjectTest, GetOrCreateParam) {
  using o3d::NamedParamRefMap;

  Param *param = param_obj()->GetOrCreateParam<ParamMatrix4>("param1");

  EXPECT_TRUE(param != NULL);
  EXPECT_EQ(ParamMatrix4::GetApparentClass(), param->GetClass());
  EXPECT_EQ("param1", param->name());
  size_t size = param_obj()->params().size();

  // Create an already-existing param, check that it returns the same one and
  // doesn't add anything else to the node.
  Param *param2 = param_obj()->GetOrCreateParam<ParamMatrix4>("param1");
  EXPECT_EQ(param, param2);
  EXPECT_EQ(size, param_obj()->params().size());
  EXPECT_EQ(param2, param_obj()->GetUntypedParam("param1"));

  // Note: Param is owned by the ParamObject now so we don't delete it here.
}

// Tests GetInputsForParam and GetOutputsForParam
TEST_F(ParamObjectTest, GetInputsForParamGetOutputsForParam) {
  FakeDivModParamOperation::Ref operation = FakeDivModParamOperation::Ref(
      new FakeDivModParamOperation(g_service_locator));
  ASSERT_FALSE(operation.IsNull());

  // Verify that FakeDivModParamOperationWorks.
  operation->set_input1(19);
  operation->set_input2(5);
  EXPECT_EQ(operation->NumberOfCallsToUpdateOutputs(), 0);
  EXPECT_EQ(operation->output1(), 3);
  EXPECT_EQ(operation->NumberOfCallsToUpdateOutputs(), 1);
  EXPECT_EQ(operation->output2(), 4);
  // This should be 1 because calling operation->output1() should
  // have updated output2().
  EXPECT_EQ(operation->NumberOfCallsToUpdateOutputs(), 1);

  // Now check GetInputs
  Param* input1 = operation->GetUntypedParam(
      FakeDivModParamOperation::kInput1Name);
  Param* input2 = operation->GetUntypedParam(
      FakeDivModParamOperation::kInput2Name);
  Param* output1 = operation->GetUntypedParam(
      FakeDivModParamOperation::kOutput1Name);
  Param* output2 = operation->GetUntypedParam(
      FakeDivModParamOperation::kOutput2Name);
  ASSERT_TRUE(input1 != NULL);
  ASSERT_TRUE(input2 != NULL);
  ASSERT_TRUE(output1 != NULL);
  ASSERT_TRUE(output2 != NULL);

  ParamVector params;
  // There should be no params on the params
  operation->GetInputsForParam(input1, &params);
  EXPECT_EQ(params.size(), 0);
  operation->GetInputsForParam(input2, &params);
  EXPECT_EQ(params.size(), 0);
  // There should be two params for each output.
  operation->GetInputsForParam(output1, &params);
  EXPECT_EQ(params.size(), 2);
  EXPECT_TRUE(ParamInParams(input1, params));
  EXPECT_TRUE(ParamInParams(input2, params));
  params.clear();  // make sure we are not just getting the same result.
  operation->GetInputsForParam(output2, &params);
  EXPECT_EQ(params.size(), 2);
  EXPECT_TRUE(ParamInParams(input1, params));
  EXPECT_TRUE(ParamInParams(input2, params));
  // Make sure we don't just add more params.
  operation->GetInputsForParam(output2, &params);
  EXPECT_EQ(params.size(), 2);

  // Check GetOutputs
  operation->GetOutputsForParam(input1, &params);
  EXPECT_EQ(params.size(), 2);
  EXPECT_TRUE(ParamInParams(output1, params));
  EXPECT_TRUE(ParamInParams(output2, params));
  operation->GetOutputsForParam(input2, &params);
  EXPECT_EQ(params.size(), 2);
  EXPECT_TRUE(ParamInParams(output1, params));
  EXPECT_TRUE(ParamInParams(output2, params));
  operation->GetOutputsForParam(output1, &params);
  EXPECT_EQ(params.size(), 0);
  operation->GetOutputsForParam(output2, &params);
  EXPECT_EQ(params.size(), 0);

  // If we bind the output we should not get it as an input or an output.
  Param* other = operation->CreateParam<ParamInteger>("other");
  ASSERT_TRUE(other != NULL);
  ASSERT_TRUE(output2->Bind(other));

  // There should be two inputs for output1 but none for output2 since it is
  // bound.
  operation->GetInputsForParam(output1, &params);
  EXPECT_EQ(params.size(), 2);
  operation->GetInputsForParam(output2, &params);
  EXPECT_EQ(params.size(), 0);

  // There should only be 1 output for each of the inputs since output2 is
  // no longer effected by the inputs.
  operation->GetOutputsForParam(input1, &params);
  EXPECT_EQ(params.size(), 1);
  EXPECT_TRUE(ParamInParams(output1, params));
  operation->GetOutputsForParam(input2, &params);
  EXPECT_EQ(params.size(), 1);
  EXPECT_TRUE(ParamInParams(output1, params));
}

// Tests Implicit Cycles
TEST_F(ParamObjectTest, ImplicitCycles) {
  FakeDivModParamOperation::Ref operation_a = FakeDivModParamOperation::Ref(
      new FakeDivModParamOperation(g_service_locator));
  FakeDivModParamOperation::Ref operation_b = FakeDivModParamOperation::Ref(
      new FakeDivModParamOperation(g_service_locator));
  ASSERT_FALSE(operation_a.IsNull());
  ASSERT_FALSE(operation_b.IsNull());

  // Now check GetInputs
  Param* input_a_1 = operation_a->GetUntypedParam(
      FakeDivModParamOperation::kInput1Name);
  Param* input_a_2 = operation_a->GetUntypedParam(
      FakeDivModParamOperation::kInput2Name);
  Param* output_a_1 = operation_a->GetUntypedParam(
      FakeDivModParamOperation::kOutput1Name);
  Param* output_a_2 = operation_a->GetUntypedParam(
      FakeDivModParamOperation::kOutput2Name);

  Param* input_b_1 = operation_b->GetUntypedParam(
      FakeDivModParamOperation::kInput1Name);
  Param* input_b_2 = operation_b->GetUntypedParam(
      FakeDivModParamOperation::kInput2Name);
  Param* output_b_1 = operation_b->GetUntypedParam(
      FakeDivModParamOperation::kOutput1Name);
  Param* output_b_2 = operation_b->GetUntypedParam(
      FakeDivModParamOperation::kOutput2Name);

  ASSERT_TRUE(input_a_1 != NULL);
  ASSERT_TRUE(input_a_2 != NULL);
  ASSERT_TRUE(output_a_1 != NULL);
  ASSERT_TRUE(output_a_2 != NULL);

  ASSERT_TRUE(input_b_1 != NULL);
  ASSERT_TRUE(input_b_2 != NULL);
  ASSERT_TRUE(output_b_1 != NULL);
  ASSERT_TRUE(output_b_2 != NULL);

  ParamVector params;
  // Test with cycle in operation 2
  EXPECT_TRUE(input_b_1->Bind(output_a_1));
  EXPECT_TRUE(input_b_2->Bind(output_b_1));

  // IA1   OA1--->IB1   OB1--+
  //   [OPA]        [OPB]    |
  // IA2   OA2 +->IB2   OB2  |
  //           |             |
  //           +-------------+

  input_a_1->GetOutputs(&params);
  EXPECT_EQ(params.size(), 6);
  EXPECT_TRUE(ParamInParams(output_a_1, params));
  EXPECT_TRUE(ParamInParams(output_a_2, params));
  EXPECT_TRUE(ParamInParams(output_b_1, params));
  EXPECT_TRUE(ParamInParams(output_b_2, params));
  EXPECT_TRUE(ParamInParams(input_b_1, params));
  EXPECT_TRUE(ParamInParams(input_b_2, params));

  // Test with cycle in operation 1
  output_b_1->UnbindInput();
  input_a_1->Bind(output_a_1);

  // +------------+
  // |            |
  // +->IA1   OA1-+->IB1   OB1
  //      [OPA]        [OPB]
  //    IA2   OA2    IB2   OB2

  output_b_1->GetInputs(&params);
  EXPECT_EQ(params.size(), 5);
  EXPECT_TRUE(ParamInParams(input_a_1, params));
  EXPECT_TRUE(ParamInParams(input_a_2, params));
  EXPECT_TRUE(ParamInParams(input_b_1, params));
  EXPECT_TRUE(ParamInParams(input_b_2, params));
  EXPECT_TRUE(ParamInParams(output_a_1, params));
}

TEST_F(ParamObjectTest,
    ParametersRegisteredAtConstructionTimeAreNotRegardedAsAdded) {
  FakeDivModParamOperation::Ref operation = FakeDivModParamOperation::Ref(
      new FakeDivModParamOperation(g_service_locator));
  Param* input1 = operation->GetUntypedParam(
      FakeDivModParamOperation::kInput1Name);
  EXPECT_FALSE(operation->IsAddedParam(input1));
}

TEST_F(ParamObjectTest, ParametersAfterConstructionTimeAreRegardedAsAdded) {
  FakeDivModParamOperation::Ref operation = FakeDivModParamOperation::Ref(
      new FakeDivModParamOperation(g_service_locator));
  ParamFloat* param = operation->CreateParam<ParamFloat>("param");
  EXPECT_TRUE(operation->IsAddedParam(param));
}

TEST_F(ParamObjectTest, ParametersOwnedByOtherObjectsAreNotRegardedAsAdded) {
  FakeDivModParamOperation::Ref operation1 = FakeDivModParamOperation::Ref(
      new FakeDivModParamOperation(g_service_locator));
  ParamFloat* param = operation1->CreateParam<ParamFloat>("param");
  FakeDivModParamOperation::Ref operation2 = FakeDivModParamOperation::Ref(
      new FakeDivModParamOperation(g_service_locator));
  EXPECT_FALSE(operation2->IsAddedParam(param));
}
}  // namespace o3d
