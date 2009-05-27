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


// Tests functionality defined in transform.cc/h

#include <algorithm>

#include "core/cross/client.h"
#include "core/cross/shape.h"
#include "core/cross/primitive.h"
#include "core/cross/material.h"
#include "tests/common/win/testing_common.h"

namespace o3d {

// Use a looser bound on floating point compares - 8 units in the last
// place, allowing a loss of 3 bits of accuracy on a 23-bit mantissa.
#define EXPECT_FLOAT_EQ_O3D(expected, actual)                 \
  EXPECT_TRUE(AlmostEqual32BitFloat((expected), (actual), 8))

// ----------------------------------------------------------------------------

namespace {

union FloatAndInt {
  float float_value;
  int int_value;
};

// Bit casts a float into an int.
inline int FloatAsInt(float value) {
  FloatAndInt float_and_int;
  float_and_int.float_value = value;
  return float_and_int.int_value;
}

// Compares 32-bit floating point values by converting them into integers
// using a "type pun" and subtracting them to see how different they are as
// bit patterns, with a necessary fudge for the IEEE754 +0/-0 area.
// For more see:
//   http://www.cygnus-software.com/papers/comparingfloats/comparingfloats.htm
bool AlmostEqual32BitFloat(float first, float second, int max_ulps) {
  // Make sure max_ulps is non-negative and small enough to force NaN to not
  // compare as ordered.
  LOG_ASSERT(max_ulps > 0 && max_ulps < 4*1024*1024);
  int first_int = FloatAsInt(first);
  // Make first_int lexicographically ordered in twos-compliment.
  if (first_int < 0) first_int = 0x80000000 - first_int;
  // Make second_int lexicographically ordered in twos-complement
  int second_int = FloatAsInt(second);
  if (second_int < 0) second_int = 0x80000000 -  second_int;
  int difference = abs(first_int - second_int);
  if (difference <= max_ulps) return true;
  return false;
}

// Compares two Vector3's for equality
void CompareVector3s(const Vector3& v1, const Vector3& v2) {
  EXPECT_FLOAT_EQ_O3D(v1.getX(), v2.getX());
  EXPECT_FLOAT_EQ_O3D(v1.getY(), v2.getY());
  EXPECT_FLOAT_EQ_O3D(v1.getZ(), v2.getZ());
}

// Compares two Vector4's for equality
void CompareVector4s(const Vector4& v1, const Vector4& v2) {
  EXPECT_FLOAT_EQ_O3D(v1.getX(), v2.getX());
  EXPECT_FLOAT_EQ_O3D(v1.getY(), v2.getY());
  EXPECT_FLOAT_EQ_O3D(v1.getZ(), v2.getZ());
  EXPECT_FLOAT_EQ_O3D(v1.getW(), v2.getW());
}

// Compares two Matrix4's for equality
void CompareMatrix4s(const Matrix4& m1, const Matrix4& m2) {
  CompareVector4s(m1.getCol0(), m2.getCol0());
  CompareVector4s(m1.getCol1(), m2.getCol1());
  CompareVector4s(m1.getCol2(), m2.getCol2());
  CompareVector4s(m1.getCol3(), m2.getCol3());
}

bool MatricesAreSame(const Matrix4& m1, const Matrix4& m2) {
  for (int ii = 0; ii < 4; ++ii) {
    const Vector4& vec1 = m1[ii];
    const Vector4& vec2 = m2[ii];
    for (int jj = 0; jj < 4; ++jj) {
      if (!AlmostEqual32BitFloat(vec1[jj], vec2[jj], 8)) {
        return false;
      }
    }
  }
  return true;
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

// A non-cachable Param.
class ParamCounter : public ParamMatrix4 {
 public:
  explicit ParamCounter(ServiceLocator* service_locator)
      : ParamMatrix4(service_locator, true, true),
        count_(0) {
    SetNotCachable();
  }
 private:
  virtual void ComputeValue() {
    count_ += 1.0f;
    set_read_only_value(Matrix4(Vector4(count_, count_, count_, count_),
                                Vector4(count_, count_, count_, count_),
                                Vector4(count_, count_, count_, count_),
                                Vector4(count_, count_, count_, count_)));
  }

  float count_;
};

}  // end unnamed namespace

// Basic test fixture.  Simply creates a Client object
// before each test and deletes it after
class TransformBasic : public testing::Test {
 protected:
  TransformBasic()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();

  virtual void TearDown();

  // Creates a simple Transform hierarchy for testing world matrix updates
  void SetupSimpleTree();

  Pack* pack() { return pack_; }

  Transform* transform_;
  Transform* transform2_;

 private:
  ServiceDependency<ObjectManager> object_manager_;
  Pack* pack_;
};

void TransformBasic::SetUp() {
  pack_ = object_manager_->CreatePack();
  transform_ = pack()->Create<Transform>();
  transform2_ = pack()->Create<Transform>();
}

void TransformBasic::TearDown() {
  pack_->Destroy();
}

// Make sure the Transform is initialized properly
TEST_F(TransformBasic, Type) {
  // Check the type
  EXPECT_EQ(Transform::GetApparentClass(), transform_->GetClass());

  // Check that the local_matrix is identity
  CompareMatrix4s(Matrix4::identity(), transform_->local_matrix());

  // Check that the default params got created
  EXPECT_TRUE(
      transform_->GetParam<ParamBoolean>(Transform::kVisibleParamName) != NULL);
  EXPECT_TRUE(
      transform_->GetParam<ParamBoolean>(Transform::kCullParamName) != NULL);
  EXPECT_TRUE(
      transform_->GetParam<ParamBoundingBox>(
          Transform::kBoundingBoxParamName) != NULL);

  // Check that visibility defaults to true
  EXPECT_EQ(transform_->visible(), true);
}

// Test set_local_matrix() / local_matrix() {
TEST_F(TransformBasic, SetLocalMatrix) {
  Matrix4 t(Vector4(0, 1, 2, 3),
            Vector4(4, 5, 6, 7),
            Vector4(8, 9, 8, 7),
            Vector4(6, 5, 4, 3));
  transform_->set_local_matrix(t);

  CompareMatrix4s(t, transform_->local_matrix());
}

// Create an additional Transform and parent transform_ under it
void TransformBasic::SetupSimpleTree() {
  transform2_->set_name("t2");

  // Make the transform_ a child of t2
  transform_->SetParent(transform2_);

  // Set t2's local matrix
  Vector3 translate(10, 20, 30);
  Vector3 rotate(1, 2, 3);
  Vector3 scale(5, 6, 7);

  Matrix4 mat(Matrix4::identity());
  mat *= Matrix4::translation(translate);
  mat *= Matrix4::rotationZYX(rotate);
  mat *= Matrix4::scale(scale);

  transform2_->set_local_matrix(mat);

  // Set transform_'s local matrix
  Vector3 translate2(30, 40, 50);
  Vector3 rotate2(3, 4, 5);
  Vector3 scale2(4, 5, 12);

  mat = Matrix4::identity();
  mat *= Matrix4::translation(translate2);
  mat *= Matrix4::rotationZYX(rotate2);
  mat *= Matrix4::scale(scale2);

  transform_->set_local_matrix(mat);
}


// Tests GetUpdatedWorldMatrix()
TEST_F(TransformBasic, GetUpdatedWorldMatrix) {
  // Create a basic hierarchy
  SetupSimpleTree();

  Matrix4 t_transform = transform_->local_matrix();
  Matrix4 t2_transform = transform2_->local_matrix();

  // Expected world matrix for transform_
  Matrix4 expected_transform = t2_transform * t_transform;

  CompareMatrix4s(expected_transform, transform_->GetUpdatedWorldMatrix());

  // Now bind something to the world_matrix of t2 and make sure that the
  // value we get is the value from the bind and not from the product
  // of the local matrix with the parent matrix.
  ParamMatrix4* matrix_param =
      transform_->CreateParam<ParamMatrix4>("matrixParam");
  Matrix4 matrix = Matrix4::translation(Vector3(10, 0, 10)) *
                   Matrix4::rotationZYX(Vector3(2, 1, 2)) *
                   Matrix4::scale(Vector3(1, 2, 3));
  matrix_param->set_value(matrix);

  ParamMatrix4* t_world_matrix =
      transform_->GetParam<ParamMatrix4>(Transform::kWorldMatrixParamName);
  ASSERT_TRUE(t_world_matrix != NULL);
  ASSERT_TRUE(t_world_matrix->Bind(matrix_param));

  CompareMatrix4s(matrix, transform_->GetUpdatedWorldMatrix());
}

// Tests GetChildren()
TEST_F(TransformBasic, GetChildren) {
  Transform* t2 = pack()->Create<Transform>();
  Transform* t3 = pack()->Create<Transform>();

  EXPECT_EQ(0, transform_->GetChildren().size());

  t2->SetParent(transform_);
  EXPECT_EQ(1, transform_->GetChildren().size());

  t3->SetParent(transform_);
  EXPECT_EQ(2, transform_->GetChildren().size());

  TransformArray children = transform_->GetChildren();
  TransformArrayConstIterator pos = find(children.begin(), children.end(), t2);
  EXPECT_TRUE(pos != children.end());

  pos = find(children.begin(), children.end(), t3);
  EXPECT_TRUE(pos != children.end());
}

// Tests WorldMatrix on Transform
TEST_F(TransformBasic, WorldMatrix) {
  // Setup a basic hierarchy
  SetupSimpleTree();

  // Compute expected world matrix for transform_
  Matrix4 t_transform = transform_->local_matrix();
  Matrix4 t2_transform = transform2_->local_matrix();
  Matrix4 expected_world_matrix = t2_transform * t_transform;

  // Force an update of the world_matrix for transform_
  transform_->GetUpdatedWorldMatrix();

  // Get current worldMatrix
  Matrix4 world_matrix_val(transform_->world_matrix());

  CompareMatrix4s(Matrix4(expected_world_matrix), world_matrix_val);
}

// Tests GetTransformsInTree
TEST_F(TransformBasic, GetTransformsInTree) {
  // Setup a basic hierarchy
  SetupSimpleTree();

  Transform* t3 = pack()->Create<Transform>();
  Transform* t4 = pack()->Create<Transform>();
  t3->SetParent(transform_);
  t4->SetParent(t3);

  TransformArray transforms_in_tree(transform2_->GetTransformsInTree());

  // Check that all of them are in the tree.
  EXPECT_EQ(transforms_in_tree.size(), 4);
  EXPECT_TRUE(std::find(transforms_in_tree.begin(),
                        transforms_in_tree.end(),
                        transform_) != transforms_in_tree.end());
  EXPECT_TRUE(std::find(transforms_in_tree.begin(),
                        transforms_in_tree.end(),
                        transform2_) != transforms_in_tree.end());
  EXPECT_TRUE(std::find(transforms_in_tree.begin(),
                        transforms_in_tree.end(),
                        t3) != transforms_in_tree.end());
  EXPECT_TRUE(std::find(transforms_in_tree.begin(),
                        transforms_in_tree.end(),
                        t4) != transforms_in_tree.end());

  // Check if we remove one it it's still correct.
  t3->SetParent(NULL);

  transforms_in_tree = transform2_->GetTransformsInTree();
  EXPECT_EQ(transforms_in_tree.size(), 2);
  EXPECT_TRUE(std::find(transforms_in_tree.begin(),
                        transforms_in_tree.end(),
                        transform_) != transforms_in_tree.end());
  EXPECT_TRUE(std::find(transforms_in_tree.begin(),
                        transforms_in_tree.end(),
                        transform2_) != transforms_in_tree.end());
  EXPECT_FALSE(std::find(transforms_in_tree.begin(),
                         transforms_in_tree.end(),
                         t3) != transforms_in_tree.end());
  EXPECT_FALSE(std::find(transforms_in_tree.begin(),
                         transforms_in_tree.end(),
                         t4) != transforms_in_tree.end());
}

// Tests GetTransformsByNameInTree
TEST_F(TransformBasic, GetTransformsByNameInTree) {
  // Setup a basic hierarchy
  SetupSimpleTree();

  // Check that a transform is in there.
  EXPECT_EQ(transform2_->GetTransformsByNameInTree("t2").size(), 1);
  // Check that another transform is not in there.
  EXPECT_EQ(transform2_->GetTransformsByNameInTree("t3").size(), 0);
}

// Tests AddShape, RemoveShape, GetShapes.
TEST_F(TransformBasic, AddShapeRemoveShapeGetShapes) {
  Shape* shape1 = pack()->Create<Shape>();
  Shape* shape2 = pack()->Create<Shape>();
  ASSERT_TRUE(shape1 != NULL);
  ASSERT_TRUE(shape2 != NULL);

  transform_->AddShape(shape1);
  transform_->AddShape(shape2);

  // Check that they actually got added.
  {
    const ShapeRefArray& shapes = transform_->GetShapeRefs();
    EXPECT_EQ(shapes.size(), 2);
    EXPECT_TRUE(std::find(shapes.begin(),
                          shapes.end(),
                          Shape::Ref(shape1)) != shapes.end());
    EXPECT_TRUE(std::find(shapes.begin(),
                          shapes.end(),
                          Shape::Ref(shape2)) != shapes.end());
  }

  // Add a second copy of shape1.
  transform_->AddShape(shape1);

  {
    // check it got added.
    ShapeArray shapes = transform_->GetShapes();
    EXPECT_EQ(shapes.size(), 3);
    EXPECT_TRUE(std::find(shapes.begin(),
                          shapes.end(),
                          Shape::Ref(shape1)) != shapes.end());
    EXPECT_TRUE(std::find(shapes.begin(),
                          shapes.end(),
                          Shape::Ref(shape2)) != shapes.end());
  }

  // Check that they can be removed.
  EXPECT_TRUE(transform_->RemoveShape(shape1));
  EXPECT_TRUE(transform_->RemoveShape(shape1));
  EXPECT_FALSE(transform_->RemoveShape(shape1));
  EXPECT_TRUE(transform_->RemoveShape(shape2));
  EXPECT_FALSE(transform_->RemoveShape(shape1));
}

TEST_F(TransformBasic,
    ShouldReplaceShapeArrayWithThoseInArrayPassedToSetShapes) {
  Shape* shape1 = pack()->Create<Shape>();
  Shape* shape2 = pack()->Create<Shape>();
  transform_->AddShape(shape1);

  ShapeArray shape_array;
  shape_array.push_back(shape2);

  transform_->SetShapes(shape_array);
  shape_array = transform_->GetShapes();

  EXPECT_EQ(1, shape_array.size());
  EXPECT_EQ(shape2, shape_array[0]);
}

TEST_F(TransformBasic, CreateGroupDrawElements) {
  // Setup a basic hierarchy
  SetupSimpleTree();

  Shape* shape1 = pack()->Create<Shape>();
  Shape* shape2 = pack()->Create<Shape>();
  Primitive* primitive1 = pack()->Create<Primitive>();
  Primitive* primitive2 = pack()->Create<Primitive>();
  Material* material = pack()->Create<Material>();
  ASSERT_TRUE(shape1 != NULL);
  ASSERT_TRUE(shape2 != NULL);
  ASSERT_TRUE(material != NULL);
  ASSERT_TRUE(primitive1 != NULL);
  ASSERT_TRUE(primitive2 != NULL);

  transform_->AddShape(shape1);
  transform2_->AddShape(shape2);
  primitive1->SetOwner(shape1);
  primitive2->SetOwner(shape2);

  transform2_->CreateDrawElements(pack(), NULL);
  transform2_->CreateDrawElements(pack(), material);

  // Check that they got created correctly.
  EXPECT_EQ(primitive1->GetDrawElementRefs().size(), 2);
  EXPECT_EQ(primitive2->GetDrawElementRefs().size(), 2);
}

TEST_F(TransformBasic, GetConcreteInputsForParamGetConcreteOutputsForParam) {
  // Setup a basic hierarchy
  SetupSimpleTree();

  Transform* t3 = pack()->Create<Transform>();
  Transform* t4 = pack()->Create<Transform>();
  transform2_->SetParent(t3);
  t3->SetParent(t4);

  // t4->t3->transform2_->transform_

  Param* t1_local_matrix = transform_->GetUntypedParam(
      Transform::kLocalMatrixParamName);
  Param* t2_local_matrix = transform2_->GetUntypedParam(
      Transform::kLocalMatrixParamName);
  Param* t3_local_matrix = t3->GetUntypedParam(
      Transform::kLocalMatrixParamName);
  Param* t4_local_matrix = t4->GetUntypedParam(
      Transform::kLocalMatrixParamName);
  Param* t1_world_matrix = transform_->GetUntypedParam(
      Transform::kWorldMatrixParamName);
  Param* t2_world_matrix = transform2_->GetUntypedParam(
      Transform::kWorldMatrixParamName);
  Param* t3_world_matrix = t3->GetUntypedParam(
      Transform::kWorldMatrixParamName);
  Param* t4_world_matrix = t4->GetUntypedParam(
      Transform::kWorldMatrixParamName);

  ParamVector params;

  // Tests GetConcreteInputsForParam (because GetInputsForParam calles
  // GetConcreteInputsForParam)
  t1_world_matrix->GetInputs(&params);
  EXPECT_EQ(params.size(), 7);
  EXPECT_TRUE(ParamInParams(t1_local_matrix, params));
  EXPECT_TRUE(ParamInParams(t2_world_matrix, params));
  EXPECT_TRUE(ParamInParams(t2_local_matrix, params));
  EXPECT_TRUE(ParamInParams(t3_world_matrix, params));
  EXPECT_TRUE(ParamInParams(t3_local_matrix, params));
  EXPECT_TRUE(ParamInParams(t4_world_matrix, params));
  EXPECT_TRUE(ParamInParams(t4_local_matrix, params));

  t1_local_matrix->GetInputs(&params);
  EXPECT_EQ(params.size(), 0);

  // Tests GetConcreteOutputsForParam (because GetOutputsForParam calles
  // GetConcreteOutputsForParam)
  t4_local_matrix->GetOutputs(&params);
  EXPECT_EQ(params.size(), 4);
  EXPECT_TRUE(ParamInParams(t1_world_matrix, params));
  EXPECT_TRUE(ParamInParams(t2_world_matrix, params));
  EXPECT_TRUE(ParamInParams(t3_world_matrix, params));
  EXPECT_TRUE(ParamInParams(t4_world_matrix, params));

  t4_world_matrix->GetOutputs(&params);
  EXPECT_EQ(params.size(), 3);
  EXPECT_TRUE(ParamInParams(t1_world_matrix, params));
  EXPECT_TRUE(ParamInParams(t2_world_matrix, params));
  EXPECT_TRUE(ParamInParams(t3_world_matrix, params));
}

// Tests the param system handles implicit parameter relationships.
TEST_F(TransformBasic, ImplicitInputs) {
  // Setup a basic hierarchy
  SetupSimpleTree();

  Transform* t3 = pack()->Create<Transform>();
  Transform* t4 = pack()->Create<Transform>();
  transform2_->SetParent(t3);
  t3->SetParent(t4);

  Param::Ref source_param = Param::Ref(new ParamCounter(g_service_locator));
  ASSERT_FALSE(source_param.IsNull());

  // t4->t3->transform2_->transform_

  Param* t1_local_matrix = transform_->GetUntypedParam(
      Transform::kLocalMatrixParamName);
  Param* t2_local_matrix = transform2_->GetUntypedParam(
      Transform::kLocalMatrixParamName);
  Param* t3_local_matrix = t3->GetUntypedParam(
      Transform::kLocalMatrixParamName);
  Param* t4_local_matrix = t4->GetUntypedParam(
      Transform::kLocalMatrixParamName);
  ParamMatrix4* t1_world_matrix = transform_->GetParam<ParamMatrix4>(
      Transform::kWorldMatrixParamName);
  Param* t2_world_matrix = transform2_->GetUntypedParam(
      Transform::kWorldMatrixParamName);
  Param* t3_world_matrix = t3->GetUntypedParam(
      Transform::kWorldMatrixParamName);
  Param* t4_world_matrix = t4->GetUntypedParam(
      Transform::kWorldMatrixParamName);

  // Check that they start as cachable.
  EXPECT_TRUE(t4_world_matrix->cachable());
  EXPECT_TRUE(t3_world_matrix->cachable());
  EXPECT_TRUE(t2_world_matrix->cachable());
  EXPECT_TRUE(t1_world_matrix->cachable());

  // check that all implicitly related params get marked as non-cachable if
  // the source is not cachacble.
  EXPECT_TRUE(t4_world_matrix->Bind(source_param));
  EXPECT_FALSE(t4_world_matrix->cachable());
  EXPECT_FALSE(t3_world_matrix->cachable());
  EXPECT_FALSE(t2_world_matrix->cachable());
  EXPECT_FALSE(t1_world_matrix->cachable());

  // Check that each time we ask for the value it changes.
  Matrix4 value1(t1_world_matrix->value());
  Matrix4 value2(t1_world_matrix->value());
  EXPECT_FALSE(MatricesAreSame(value1, value2));

  // Check that if we disconnect in the middle some of them become cachable.
  transform2_->SetParent(NULL);
  EXPECT_FALSE(t4_world_matrix->cachable());
  EXPECT_FALSE(t3_world_matrix->cachable());
  EXPECT_TRUE(t2_world_matrix->cachable());
  EXPECT_TRUE(t1_world_matrix->cachable());

  // Check if we disconnect the bottom the rest become cachable.
  source_param->UnbindOutputs();
  EXPECT_TRUE(t4_world_matrix->cachable());
  EXPECT_TRUE(t3_world_matrix->cachable());
  EXPECT_TRUE(t2_world_matrix->cachable());
  EXPECT_TRUE(t1_world_matrix->cachable());

  // Check if we connect to a middle one the correct ones become cachable.
  transform2_->SetParent(t3);
  EXPECT_TRUE(t3_local_matrix->Bind(source_param));
  EXPECT_TRUE(t4_world_matrix->cachable());
  EXPECT_FALSE(t3_world_matrix->cachable());
  EXPECT_FALSE(t2_world_matrix->cachable());
  EXPECT_FALSE(t1_world_matrix->cachable());
}

}  // namespace o3d
