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


// Tests functionality of the Param class and its derived classes defined in
// param.cc

#include <algorithm>

#include "tests/common/win/testing_common.h"
#include "core/cross/object_manager.h"
#include "core/cross/error_status.h"
#include "core/cross/pack.h"
#include "core/cross/param_array.h"
#include "core/cross/render_surface.h"
#include "core/cross/sampler.h"
#include "core/cross/texture.h"
#include "core/cross/transform.h"
#include "core/cross/types.h"

namespace o3d {

namespace {
// TestTexture derives from o3d::Texture and provides dummy implementations
// for Texture's virtual methods.  It is used in this file instead of Texture
// which is an abstract class and cannot be instantiated
class TestTexture : public Texture {
 public:
  explicit TestTexture(ServiceLocator* service_locator)
      : Texture(service_locator, UNKNOWN_FORMAT, 1, true, false,
                false) {}
  void* GetTextureHandle() const { return NULL; }
  virtual const RGBASwizzleIndices& GetABGR32FSwizzleIndices();
};

const Texture::RGBASwizzleIndices& TestTexture::GetABGR32FSwizzleIndices() {
  static Texture::RGBASwizzleIndices swizzle_indices = {2, 1, 0, 3};
  return swizzle_indices;
};

// TestRenderSurface derives from o3d::RenderSurface, and provides dummy
// implementations of the abstract virtual functions of RenderSurface.
class TestRenderSurface : public RenderSurface {
 public:
  explicit TestRenderSurface(ServiceLocator* service_locator)
      : RenderSurface(service_locator, 1, 1, NULL) {}
  void* GetSurfaceHandle() const { return NULL; }
};

// TestSampler derives from o3d::Sampler and provides a public
// implementation of the constructor.
class TestSampler : public Sampler {
 public:
  explicit TestSampler(ServiceLocator* service_locator)
      : Sampler(service_locator) {}
};

}  // anonymous namespace

// Basic test fixture.
class ParamBasic : public testing::Test {
 protected:

  ParamBasic()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();
  virtual void TearDown();
  Transform *transform() { return transform_; }

  ServiceDependency<ObjectManager> object_manager_;

  Pack* pack() { return pack_; }

 private:
  ErrorStatus* error_status_;
  Transform* transform_;
  Pack* pack_;
};

void ParamBasic::SetUp() {
  error_status_ = new ErrorStatus(g_service_locator);
  pack_ = object_manager_->CreatePack();
  transform_ = pack_->Create<Transform>();
}

void ParamBasic::TearDown() {
  pack_->Destroy();
  delete error_status_;
}

namespace {

// Check if a param is in the given param array
// Parameters:
//   param: param to search for.
//   params: ParamVector to search.
// Returns:
//   true if param is in params.
bool ParamInParams(Param* param, const ParamVector& params) {
  return std::find(params.begin(), params.end(), param) != params.end();
}

// A non-cachable Param.
class ParamCounter : public ParamFloat {
 public:
  explicit ParamCounter(ServiceLocator* service_locator)
      : ParamFloat(service_locator, true, true),
        count_(0) {
    SetNotCachable();
  }
 private:
  virtual void ComputeValue() {
    count_ += 1.0f;
    set_read_only_value(count_);
  }

  float count_;
};

}  // anonymous namespace

// Set/Get Float Params
TEST_F(ParamBasic, TestFloat) {
  ParamFloat *param = transform()->CreateParam<ParamFloat>("floatParam");
  EXPECT_EQ(ParamFloat::GetApparentClass(), param->GetClass());
  Float in_val = 10.0f;
  param->set_value(in_val);
  Float out_val = param->value();
  EXPECT_FLOAT_EQ(in_val, out_val);
}

// Set/Get Float2 Params
TEST_F(ParamBasic, TestFloat2) {
  ParamFloat2 *param = transform()->CreateParam<ParamFloat2>(
      "floatParam2");
  EXPECT_EQ(ParamFloat2::GetApparentClass(), param->GetClass());
  Float2 in_val(10.0f, 20.0f);
  param->set_value(in_val);
  Float2 out_val = param->value();
  EXPECT_FLOAT_EQ(in_val[0], out_val[0]);
  EXPECT_FLOAT_EQ(in_val[1], out_val[1]);
}

// Set/Get Float3 Params
TEST_F(ParamBasic, TestFloat3) {
  ParamFloat3 *param = transform()->CreateParam<ParamFloat3>(
      "floatParam3");
  EXPECT_EQ(ParamFloat3::GetApparentClass(), param->GetClass());
  Float3 in_val(10.0f, 20.0f, 30.0f);
  param->set_value(in_val);
  Float3 out_val = param->value();
  EXPECT_FLOAT_EQ(in_val[0], out_val[0]);
  EXPECT_FLOAT_EQ(in_val[1], out_val[1]);
  EXPECT_FLOAT_EQ(in_val[2], out_val[2]);
}

// Set/Get Float4 Params
TEST_F(ParamBasic, TestFloat4) {
  ParamFloat4 *param = transform()->CreateParam<ParamFloat4>(
      "floatParam4");
  EXPECT_EQ(ParamFloat4::GetApparentClass(), param->GetClass());
  Float4 in_val(10.0f, 20.0f, 30.0f, 40.f);
  param->set_value(in_val);
  Float4 out_val = param->value();
  EXPECT_FLOAT_EQ(in_val[0], out_val[0]);
  EXPECT_FLOAT_EQ(in_val[1], out_val[1]);
  EXPECT_FLOAT_EQ(in_val[2], out_val[2]);
  EXPECT_FLOAT_EQ(in_val[3], out_val[3]);
}

// Set/Get Integer param
TEST_F(ParamBasic, TestInt) {
  ParamInteger *param = transform()->CreateParam<ParamInteger>("IntParam");
  EXPECT_EQ(ParamInteger::GetApparentClass(), param->GetClass());
  int in_val = 10;
  param->set_value(in_val);
  int out_val = param->value();
  EXPECT_EQ(in_val, out_val);
}

// Set/Get Boolean param
TEST_F(ParamBasic, TestBoolean) {
  ParamBoolean *param = transform()->CreateParam<ParamBoolean>(
      "BoolParam");
  EXPECT_EQ(ParamBoolean::GetApparentClass(), param->GetClass());
  bool in_val = true;
  param->set_value(in_val);
  bool out_val = param->value();
  EXPECT_TRUE(out_val);
}

// Set/Get String param
TEST_F(ParamBasic, TestString) {
  ParamString *param = transform()->CreateParam<ParamString>(
      "StringParam");
  EXPECT_EQ(ParamString::GetApparentClass(), param->GetClass());
  String kInputString = "Test my\tstring\n";
  param->set_value(kInputString);
  String kOutputString = param->value();
  EXPECT_EQ(kInputString, kOutputString);
}

// Set/Get Matrix4 param
TEST_F(ParamBasic, TestMatrix4) {
  ParamMatrix4 *param = transform()->CreateParam<ParamMatrix4>(
      "MatrixParam");
  EXPECT_EQ(ParamMatrix4::GetApparentClass(), param->GetClass());
  Matrix4 in_val = Matrix4::rotationZYX(Vector3(10, 20, 30));
  in_val.setTranslation(Vector3(1, 2, 3));
  param->set_value(in_val);
  Matrix4 out_val = param->value();

  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      EXPECT_FLOAT_EQ(in_val[i][j], out_val[i][j]);
}

// Set/Get Texture param
TEST_F(ParamBasic, TestTexture) {
  ParamTexture *param =
      transform()->CreateParam<ParamTexture>("TextureParam");
  EXPECT_EQ(ParamTexture::GetApparentClass(), param->GetClass());
  TestTexture::Ref in_texture(new TestTexture(g_service_locator));
  param->set_value(in_texture);
  Texture *out_texture = param->value();
  EXPECT_EQ(in_texture, out_texture);
  in_texture.Reset();
}

TEST_F(ParamBasic, TestRenderSurface) {
  ParamRenderSurface *param =
      transform()->CreateParam<ParamRenderSurface>("RenderSurfaceParam");
  EXPECT_EQ(ParamRenderSurface::GetApparentClass(), param->GetClass());

  TestRenderSurface::Ref in_surface(new TestRenderSurface(
      g_service_locator));
  param->set_value(in_surface);
  RenderSurface *out_surface = param->value();
  EXPECT_EQ(in_surface, out_surface);
  in_surface.Reset();
}

// Set/Get Sampler param
TEST_F(ParamBasic, TestSampler) {
  ParamSampler *param =
      transform()->CreateParam<ParamSampler>("SamplerParam");
  EXPECT_EQ(ParamSampler::GetApparentClass(), param->GetClass());
  TestSampler::Ref in_sampler(new TestSampler(g_service_locator));
  param->set_value(in_sampler);
  Sampler *out_sampler = param->value();
  EXPECT_EQ(in_sampler, out_sampler);
  in_sampler.Reset();
}

// Output connection logic
TEST_F(ParamBasic, OutputConnections) {
  Param *param1 = transform()->CreateParam<ParamFloat>("FloatParam1");
  Param *param2 = transform()->CreateParam<ParamFloat>("FloatParam2");
  Param *param3 = transform()->CreateParam<ParamFloat>("FloatParam3");
  Param *param4 = transform()->CreateParam<ParamFloat>("FloatParam4");

  EXPECT_TRUE(param2->Bind(param1));
  EXPECT_TRUE(param3->Bind(param1));
  EXPECT_TRUE(param4->Bind(param1));

  ParamVector out_connections = param1->output_connections();
  EXPECT_EQ(3, out_connections.size());

  EXPECT_EQ(param2->input_connection(), param1);
  EXPECT_EQ(param3->input_connection(), param1);
  EXPECT_EQ(param4->input_connection(), param1);

  // Check that all three are listed as output connections for param1
  ParamVector::const_iterator pos;
  EXPECT_TRUE(find(out_connections.begin(), out_connections.end(), param2) !=
              out_connections.end());
  EXPECT_TRUE(find(out_connections.begin(), out_connections.end(), param3) !=
              out_connections.end());
  EXPECT_TRUE(find(out_connections.begin(), out_connections.end(), param4) !=
              out_connections.end());

  // Remove one and check again
  EXPECT_TRUE(param1->UnbindOutput(param3));
  out_connections = param1->output_connections();
  EXPECT_EQ(2, out_connections.size());
  EXPECT_TRUE(find(out_connections.begin(), out_connections.end(), param2) !=
              out_connections.end());
  EXPECT_FALSE(find(out_connections.begin(), out_connections.end(), param3) !=
               out_connections.end());
  EXPECT_TRUE(find(out_connections.begin(), out_connections.end(), param4) !=
              out_connections.end());
  EXPECT_TRUE(param3->input_connection() == NULL);

  EXPECT_TRUE(param4->input_connection() != NULL);
  param4->UnbindInput();

  // Check that input_connection is gone
  EXPECT_TRUE(param4->input_connection() == NULL);

  // and that output connections are updated
  out_connections = param1->output_connections();
  EXPECT_EQ(1, out_connections.size());
  EXPECT_TRUE(find(out_connections.begin(), out_connections.end(), param2) !=
              out_connections.end());
  EXPECT_FALSE(find(out_connections.begin(), out_connections.end(), param3) !=
               out_connections.end());
  EXPECT_FALSE(find(out_connections.begin(), out_connections.end(), param4) !=
               out_connections.end());
}

// UnbindInput on a Param
TEST_F(ParamBasic, UnbindInput) {
  Param *param1 = transform()->CreateParam<ParamFloat>("Param1");
  Param *param2 = transform()->CreateParam<ParamFloat>("Param2");

  EXPECT_TRUE(param2->Bind(param1));

  Param *source = param2->input_connection();

  EXPECT_TRUE(param2->input_connection() != NULL);

  param2->UnbindInput();

  EXPECT_TRUE(param2->input_connection() == NULL);
}

TEST_F(ParamBasic, BindToNullUnbindsInput) {
  Param *param1 = transform()->CreateParam<ParamFloat>("Param1");
  Param *param2 = transform()->CreateParam<ParamFloat>("Param2");
  EXPECT_TRUE(param2->Bind(param1));
  EXPECT_TRUE(param2->input_connection() == param1);

  param2->Bind(NULL);
  EXPECT_TRUE(param2->input_connection() == NULL);
}

// CopyDataFromParam for a FLOAT Param
TEST_F(ParamBasic, CopyDataFromParamFloat) {
  ParamFloat *param1 = transform()->CreateParam<ParamFloat>("Param1");
  ParamFloat *param2 = transform()->CreateParam<ParamFloat>("Param2");

  param1->set_value(10.0f);
  param2->CopyDataFromParam(param1);
  Float val = param2->value();
  EXPECT_FLOAT_EQ(10.0f, val);
}

// CopyDataFromParam for a Float2 Param
TEST_F(ParamBasic, CopyDataFromParamFloat2) {
  ParamFloat2 *param1 = transform()->CreateParam<ParamFloat2>("Param1");
  ParamFloat2 *param2 = transform()->CreateParam<ParamFloat2>("Param2");
  Float2 in_val(10.0f, 20.0f);
  param1->set_value(in_val);
  param2->CopyDataFromParam(param1);
  Float2 out_val = param2->value();
  EXPECT_FLOAT_EQ(in_val[0], out_val[0]);
  EXPECT_FLOAT_EQ(in_val[1], out_val[1]);
}

// CopyDataFromParam for a Float3 Param
TEST_F(ParamBasic, CopyDataFromParamFloat3) {
  ParamFloat3 *param1 = transform()->CreateParam<ParamFloat3>("Param1");
  ParamFloat3 *param2 = transform()->CreateParam<ParamFloat3>("Param2");
  Float3 in_val(10.0f, 20.0f, 30.0f);
  param1->set_value(in_val);
  param2->CopyDataFromParam(param1);
  Float3 out_val = param2->value();
  EXPECT_FLOAT_EQ(in_val[0], out_val[0]);
  EXPECT_FLOAT_EQ(in_val[1], out_val[1]);
  EXPECT_FLOAT_EQ(in_val[2], out_val[2]);
}

// CopyDataFromParam for a Float4 Param
TEST_F(ParamBasic, CopyDataFromParamFloat4) {
  ParamFloat4 *param1 = transform()->CreateParam<ParamFloat4>("Param1");
  ParamFloat4 *param2 = transform()->CreateParam<ParamFloat4>("Param2");
  Float4 in_val(10.0f, 20.0f, 30.0f, 40.0f);
  param1->set_value(in_val);
  param2->CopyDataFromParam(param1);
  Float4 out_val = param2->value();
  EXPECT_FLOAT_EQ(in_val[0], out_val[0]);
  EXPECT_FLOAT_EQ(in_val[1], out_val[1]);
  EXPECT_FLOAT_EQ(in_val[2], out_val[2]);
  EXPECT_FLOAT_EQ(in_val[3], out_val[3]);
}

// CopyDataFromParam for a INTEGER Param
TEST_F(ParamBasic, CopyDataFromParamInt) {
  ParamInteger *param1 = transform()->CreateParam<ParamInteger>("Param1");
  ParamInteger *param2 = transform()->CreateParam<ParamInteger>("Param2");

  param1->set_value(10);
  param2->CopyDataFromParam(param1);
  int val = param2->value();
  EXPECT_EQ(10, val);
}

// CopyDataFromParam for a BOOLEAN Param
TEST_F(ParamBasic, CopyDataFromParamBool) {
  ParamBoolean *param1 = transform()->CreateParam<ParamBoolean>("Param1");
  ParamBoolean *param2 = transform()->CreateParam<ParamBoolean>("Param2");

  param1->set_value(true);
  param2->CopyDataFromParam(param1);
  bool val = param2->value();
  EXPECT_TRUE(val);
}

// CopyDataFromParam for a STRING Param
TEST_F(ParamBasic, CopyDataFromParamString) {
  ParamString *param1 = transform()->CreateParam<ParamString>("Param1");
  ParamString *param2 = transform()->CreateParam<ParamString>("Param2");

  String kInputString = "Test my cr\\azy\ts\ntring";
  param1->set_value(kInputString);
  param2->CopyDataFromParam(param1);
  String kOutputString = param2->value();
  EXPECT_EQ(kInputString, kOutputString);
}

// CopyDataFromParam for a MATRIX_4 Param
TEST_F(ParamBasic, CopyDataFromParamMatrix4) {
  ParamMatrix4 *param1 = transform()->CreateParam<ParamMatrix4>("Param1");
  ParamMatrix4 *param2 = transform()->CreateParam<ParamMatrix4>("Param2");
  Matrix4 in_val = Matrix4::rotationZYX(Vector3(10, 20, 30));
  in_val.setTranslation(Vector3(1, 2, 3));

  param1->set_value(in_val);
  param2->CopyDataFromParam(param1);
  Matrix4 out_val = param2->value();

  for (int i = 0;i < 4;i++)
    for (int j = 0;j < 4;j++)
      EXPECT_FLOAT_EQ(in_val[i][j], out_val[i][j]);
}

// CopyDataFromParam for a TEXTURE Param
TEST_F(ParamBasic, CopyDataFromParamTexture) {
  ParamTexture *param1 = transform()->CreateParam<ParamTexture>("Param1");
  ParamTexture *param2 = transform()->CreateParam<ParamTexture>("Param2");

  TestTexture::Ref in_texture(new TestTexture(g_service_locator));
  param1->set_value(in_texture);
  param2->CopyDataFromParam(param1);
  Texture *out_texture = param2->value();
  EXPECT_TRUE(in_texture == out_texture);

  // Remove all references to the created texture to localize the test case.
  in_texture.Reset();
  param1->set_value(Texture::Ref(NULL));
}

// CopyDataFromParam for a RenderSurface Param
TEST_F(ParamBasic, CopyDataFromParamRenderSurface) {
  ParamRenderSurface *param1 =
      transform()->CreateParam<ParamRenderSurface>("Param1");
  ParamRenderSurface *param2 =
      transform()->CreateParam<ParamRenderSurface>("Param2");

  TestRenderSurface::Ref in_render_surface(new TestRenderSurface(
      g_service_locator));
  param1->set_value(in_render_surface);
  param2->CopyDataFromParam(param1);
  RenderSurface *out_render_surface = param2->value();
  EXPECT_TRUE(in_render_surface == out_render_surface);

  // Remove all references to the created texture to localize the test case.
  in_render_surface.Reset();
  param1->set_value(RenderSurface::Ref(NULL));
}


TEST_F(ParamBasic, TestReadOnly) {
  ParamMatrix4 *param = transform()->CreateParam<ParamMatrix4>("world");
  param->MarkAsReadOnly();
  EXPECT_EQ(ParamMatrix4::GetApparentClass(), param->GetClass());
  IErrorStatus* error_status = g_service_locator->GetService<IErrorStatus>();
  error_status->ClearLastError();
  param->set_value(Matrix4::identity());
  String error(error_status->GetLastError());
  EXPECT_TRUE(!error.empty());
}

// Tests ParamParamArray
TEST_F(ParamBasic, ParamParamArray) {
  ParamParamArray *array_param =
      transform()->CreateParam<ParamParamArray>("param_array");
  ASSERT_TRUE(array_param != NULL);
  EXPECT_TRUE(array_param->value() == NULL);
  ParamArray *array = pack()->Create<ParamArray>();
  ASSERT_TRUE(array != NULL);
  array_param->set_value(array);
  ASSERT_TRUE(array_param->value() == array);

  // Check we can add a param.
  ParamFloat* param_0 = array_param->value()->CreateParam<ParamFloat>(0);
  ASSERT_TRUE(param_0 != NULL);
  param_0->set_value(3.0f);
  EXPECT_TRUE(array_param->value()->GetParam<ParamFloat>(0) == param_0);
  EXPECT_TRUE(array_param->value()->GetParam<ParamFloat>(0)->value() == 3.0f);
}

// A Param bind test fixture
class ParamBindTest : public testing::Test {
 protected:

  ParamBindTest()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();

  virtual void TearDown();

  Pack* pack() { return pack_; }

  ParamFloat *float_param_1_, *float_param_2_, *float_param_3_;
  ParamFloat4 *float4_param_1_, *float4_param_2_;
  ParamMatrix4 *matrix_param_1_, *matrix_param_2_, *matrix_param_3_;

  ServiceDependency<ObjectManager> object_manager_;

 private:
  Pack* pack_;
};

void ParamBindTest::SetUp() {
  pack_ = object_manager_->CreatePack();

  Transform *t1 = pack_->Create<Transform>();
  Transform *t2 = pack_->Create<Transform>();

  float_param_1_ = t1->CreateParam<ParamFloat>("floatParam1");
  float_param_2_ = t2->CreateParam<ParamFloat>("floatParam2");
  float_param_3_ = t2->CreateParam<ParamFloat>("floatParam3");

  float4_param_1_ = t1->CreateParam<ParamFloat4>("float4Param1");
  float4_param_2_ = t2->CreateParam<ParamFloat4>("float4Param2");

  matrix_param_1_ = t1->CreateParam<ParamMatrix4>("matrixParam1");
  matrix_param_2_ = t2->CreateParam<ParamMatrix4>("matrixParam2");
  matrix_param_3_ = t2->CreateParam<ParamMatrix4>("matrixParam3");
}

void ParamBindTest::TearDown() {
  object_manager_->DestroyPack(pack_);
}

// Tests straight param bind
TEST_F(ParamBindTest, BindParams) {
  float_param_1_->set_value(10.0f);

  // Bind float_param_2_ to float_param_1_
  EXPECT_TRUE(float_param_2_->Bind(float_param_1_));

  // Read the value of float_param_2_ and make sure it's the same as
  // the value set to float_param_1_
  float val = float_param_2_->value();
  EXPECT_EQ(10.0f, val);
}

// Makes sure that binding two Params of different types fails
TEST_F(ParamBindTest, BindParamDifferentType) {
  EXPECT_FALSE(float_param_2_->Bind(matrix_param_1_));
}

// Tests UnbindInput on a direct bind
TEST_F(ParamBindTest, UnbindInputDirect) {
  float_param_2_->Bind(float_param_1_);

  // Break the connection
  float_param_2_->UnbindInput();

  // Make sure that the input connection is gone
  float_param_2_->set_value(123.0f);
  float_param_1_->set_value(456.0f);
  EXPECT_EQ(123.0, float_param_2_->value());
}

// Tests GetParamById
TEST_F(ParamBindTest, GetParamById) {
  EXPECT_TRUE(matrix_param_3_ == object_manager_->GetById<ParamMatrix4>(
      matrix_param_3_->id()));
  EXPECT_TRUE(float_param_2_ == object_manager_->GetById<ParamFloat>(
      float_param_2_->id()));
}

// Tests GetInputs and GetOutputs
TEST_F(ParamBindTest, GetInputsGetOuputs) {
  EXPECT_TRUE(float_param_1_->Bind(float_param_2_));
  EXPECT_TRUE(float_param_2_->Bind(float_param_3_));

  // 3->2->1

  ParamVector params;
  float_param_1_->GetInputs(&params);
  EXPECT_EQ(params.size(), 2);
  EXPECT_TRUE(ParamInParams(float_param_2_, params));
  EXPECT_TRUE(ParamInParams(float_param_3_, params));

  float_param_1_->GetOutputs(&params);
  EXPECT_EQ(params.size(), 0);

  float_param_2_->GetInputs(&params);
  EXPECT_EQ(params.size(), 1);
  EXPECT_TRUE(ParamInParams(float_param_3_, params));

  float_param_2_->GetOutputs(&params);
  EXPECT_EQ(params.size(), 1);
  EXPECT_TRUE(ParamInParams(float_param_1_, params));

  float_param_3_->GetInputs(&params);
  EXPECT_EQ(params.size(), 0);

  float_param_3_->GetOutputs(&params);
  EXPECT_EQ(params.size(), 2);
  EXPECT_TRUE(ParamInParams(float_param_1_, params));
  EXPECT_TRUE(ParamInParams(float_param_2_, params));

  // Check with cycles.
  EXPECT_TRUE(float_param_3_->Bind(float_param_1_));

  // 3->2->1->3

  float_param_1_->GetInputs(&params);
  EXPECT_EQ(params.size(), 2);
  EXPECT_TRUE(ParamInParams(float_param_2_, params));
  EXPECT_TRUE(ParamInParams(float_param_3_, params));

  float_param_1_->GetOutputs(&params);
  EXPECT_EQ(params.size(), 2);
  EXPECT_TRUE(ParamInParams(float_param_2_, params));
  EXPECT_TRUE(ParamInParams(float_param_3_, params));

  float_param_2_->GetInputs(&params);
  EXPECT_EQ(params.size(), 2);
  EXPECT_TRUE(ParamInParams(float_param_1_, params));
  EXPECT_TRUE(ParamInParams(float_param_3_, params));

  float_param_2_->GetOutputs(&params);
  EXPECT_EQ(params.size(), 2);
  EXPECT_TRUE(ParamInParams(float_param_1_, params));
  EXPECT_TRUE(ParamInParams(float_param_3_, params));

  float_param_3_->GetInputs(&params);
  EXPECT_EQ(params.size(), 2);
  EXPECT_TRUE(ParamInParams(float_param_1_, params));
  EXPECT_TRUE(ParamInParams(float_param_2_, params));

  float_param_3_->GetOutputs(&params);
  EXPECT_EQ(params.size(), 2);
  EXPECT_TRUE(ParamInParams(float_param_1_, params));
  EXPECT_TRUE(ParamInParams(float_param_2_, params));

  // Check with up stream cycle (up stream from 1).
  float_param_3_->UnbindInput();  // 3->2->1
  EXPECT_TRUE(float_param_3_->Bind(float_param_2_));

  // 3--->2-+->1
  //        |
  //        +->3

  float_param_3_->GetOutputs(&params);
  EXPECT_EQ(params.size(), 2);
  EXPECT_TRUE(ParamInParams(float_param_1_, params));
  EXPECT_TRUE(ParamInParams(float_param_2_, params));

  float_param_1_->GetInputs(&params);
  EXPECT_EQ(params.size(), 2);
  EXPECT_TRUE(ParamInParams(float_param_2_, params));
  EXPECT_TRUE(ParamInParams(float_param_3_, params));
}

// Tests update input
TEST_F(ParamBindTest, UpdateInput) {
  EXPECT_TRUE(float_param_3_->Bind(float_param_2_));
  EXPECT_TRUE(float_param_2_->Bind(float_param_1_));

  // 1->2->3

  // Check that the value comes through.
  float_param_1_->set_value(1.0f);
  EXPECT_EQ(float_param_3_->value(), 1.0f);

  // Check that if we set update_input it just gets the current value. (it's not
  // going to ask float_param_2_ to update.)
  float_param_3_->set_update_input(false);
  float_param_1_->set_value(3.0f);
  EXPECT_EQ(float_param_3_->value(), 1.0f);

  // Check it is actually getting the float_param_2_'s value.
  float_param_1_->set_value(5.0f);
  float value = float_param_2_->value();
  EXPECT_EQ(float_param_3_->value(), value);
}

// Tests non cachable params
TEST_F(ParamBindTest, NonCachable) {
  Param::Ref param = Param::Ref(new ParamCounter(g_service_locator));
  ASSERT_FALSE(param.IsNull());

  // Check that if we bind to a non-cachable param we are also not cachable
  EXPECT_TRUE(float_param_1_->Bind(param));
  EXPECT_FALSE(float_param_1_->cachable());

  // Check that if we unbind it goes back to cachable.
  float_param_1_->UnbindInput();
  EXPECT_TRUE(float_param_1_->cachable());

  // Check that it if we bind to a chain both get marked as non cachable.
  EXPECT_TRUE(float_param_2_->Bind(float_param_1_));
  EXPECT_TRUE(float_param_1_->Bind(param));
  EXPECT_FALSE(float_param_2_->cachable());
  EXPECT_FALSE(float_param_1_->cachable());

  // Check that if we unbind the change they get marked as uncachable.
  float_param_1_->UnbindInput();
  EXPECT_TRUE(float_param_2_->cachable());
  EXPECT_TRUE(float_param_1_->cachable());

  // Check that each time we ask for a value we get a different one.
  EXPECT_TRUE(float_param_1_->Bind(param));
  float value1 = float_param_2_->value();
  float value2 = float_param_2_->value();
  EXPECT_EQ(value1 + 1.0f, value2);

  // Check that unbinding from the other side things get marked as cachable.
  param->UnbindOutputs();
  EXPECT_TRUE(float_param_2_->cachable());
  EXPECT_TRUE(float_param_1_->cachable());
}
}  // namespace o3d
