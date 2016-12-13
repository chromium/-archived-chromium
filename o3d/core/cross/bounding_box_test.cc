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


// This file implements unit tests for class BoundingBox.

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/bounding_box.h"
#include "core/cross/math_utilities.h"
#include "core/cross/ray_intersection_info.h"
#include "core/cross/transform.h"
#include "core/cross/renderer.h"

namespace o3d {

class BoundingBoxTest : public testing::Test {
};

// Tests the creation of a BoundingBox.
TEST_F(BoundingBoxTest, Basic) {
  // Check that it defaults to invalid.
  EXPECT_FALSE(BoundingBox().valid());

  BoundingBox bounding_box(Point3(-1.0f, 2.0f, -3.0f),
                           Point3(1.0f, -2.0f, 3.0f));
  // Check it's valid.
  EXPECT_TRUE(bounding_box.valid());

  // Check it got set.
  EXPECT_EQ(bounding_box.min_extent().getX(), -1.0f);
  EXPECT_EQ(bounding_box.min_extent().getY(), -2.0f);
  EXPECT_EQ(bounding_box.min_extent().getZ(), -3.0f);
  EXPECT_EQ(bounding_box.max_extent().getX(), 1.0f);
  EXPECT_EQ(bounding_box.max_extent().getY(), 2.0f);
  EXPECT_EQ(bounding_box.max_extent().getZ(), 3.0f);
}

// Tests BoundingBox::Add
TEST_F(BoundingBoxTest, Add) {
  BoundingBox box(Point3(-1.0f, -2.0f, -3.0f),
                  Point3(1.0f, 2.0f, 3.0f));
  box.Add(BoundingBox(Point3(-4.0f, -1.0f,  9.0f),
                      Point3(5.0f, 1.0f, 11.0f)),
          &box);

  // Check the results are the sum of both boxes.
  EXPECT_TRUE(box.valid());
  EXPECT_EQ(box.min_extent().getX(), -4.0f);
  EXPECT_EQ(box.min_extent().getY(), -2.0f);
  EXPECT_EQ(box.min_extent().getZ(), -3.0f);
  EXPECT_EQ(box.max_extent().getX(), 5.0f);
  EXPECT_EQ(box.max_extent().getY(), 2.0f);
  EXPECT_EQ(box.max_extent().getZ(), 11.0f);

  // Check only the original box is valid.
  box.Add(BoundingBox(),
          &box);

  // Check the results are just the first box.
  EXPECT_TRUE(box.valid());
  EXPECT_EQ(box.min_extent().getX(), -4.0f);
  EXPECT_EQ(box.min_extent().getY(), -2.0f);
  EXPECT_EQ(box.min_extent().getZ(), -3.0f);
  EXPECT_EQ(box.max_extent().getX(), 5.0f);
  EXPECT_EQ(box.max_extent().getY(), 2.0f);
  EXPECT_EQ(box.max_extent().getZ(), 11.0f);

  // Check only the otherbox is valid.
  box = BoundingBox();
  box.Add(BoundingBox(Point3(-4.0f, -1.0f,  9.0f),
                      Point3(5.0f, 1.0f, 11.0f)),
          &box);

  // Check the results are just the second box.
  EXPECT_TRUE(box.valid());
  EXPECT_EQ(box.min_extent().getX(), -4.0f);
  EXPECT_EQ(box.min_extent().getY(), -1.0f);
  EXPECT_EQ(box.min_extent().getZ(), 9.0f);
  EXPECT_EQ(box.max_extent().getX(), 5.0f);
  EXPECT_EQ(box.max_extent().getY(), 1.0f);
  EXPECT_EQ(box.max_extent().getZ(), 11.0f);

  // Check neither box valid
  box = BoundingBox();
  box.Add(BoundingBox(), &box);

  // Check the results are not valid.
  EXPECT_FALSE(box.valid());
}

static const float kEpsilon = 0.0001f;

// Tests BoundingBox::Mul
TEST_F(BoundingBoxTest, Mul) {
  BoundingBox bounding_box(Point3(-10.0f, 1.0f, -3.0f),
                           Point3(0.0f,   2.0f, 3.0f));

  // Rotate around Z 180 degrees and check the values are as expected.
  BoundingBox box;
  bounding_box.Mul(Matrix4::rotationZ(3.14159f), &box);

  EXPECT_TRUE(fabsf(box.min_extent().getX() -  0.0f) < kEpsilon);
  EXPECT_TRUE(fabsf(box.min_extent().getY() - -2.0f) < kEpsilon);
  EXPECT_TRUE(fabsf(box.min_extent().getZ() - -3.0f) < kEpsilon);
  EXPECT_TRUE(fabsf(box.max_extent().getX() - 10.0f) < kEpsilon);
  EXPECT_TRUE(fabsf(box.max_extent().getY() - -1.0f) < kEpsilon);
  EXPECT_TRUE(fabsf(box.max_extent().getZ() -  3.0f) < kEpsilon);
}

// Tests BoundingBox::IntersectRay
TEST_F(BoundingBoxTest, IntersectRay) {
  BoundingBox bounding_box(Point3(-1.0f, -2.0f, -3.0f),
                           Point3(1.0f, 2.0f, 3.0f));

  // Check a ray that collides.
  RayIntersectionInfo info;
  bounding_box.IntersectRay(Point3(-2.0f, -4.0f, -6.0f),
                            Point3(2.0f, 4.0f, 6.0f),
                            &info);

  EXPECT_TRUE(info.valid());
  EXPECT_TRUE(info.intersected());
  EXPECT_TRUE(fabsf(info.position().getX() - -1.0f) < kEpsilon);
  EXPECT_TRUE(fabsf(info.position().getY() - -2.0f) < kEpsilon);
  EXPECT_TRUE(fabsf(info.position().getZ() - -3.0f) < kEpsilon);

  // Check a ray that misses.
  bounding_box.IntersectRay(Point3(2.0f, 4.0f, 6.0f),
                            Point3(12.0f, 14.0f, 16.0f),
                            &info);

  EXPECT_TRUE(info.valid());
  EXPECT_FALSE(info.intersected());

  // Check that an invalid box returns an invalid ray.
  BoundingBox invalid_box;
  invalid_box.IntersectRay(Point3(2.0f, 4.0f, 6.0f),
                           Point3(12.0f, 14.0f, 16.0f),
                           &info);
  EXPECT_FALSE(info.valid());
}

// Tests BoundingBox::InFrustum
TEST_F(BoundingBoxTest, InFrustum) {
  Matrix4 view(Matrix4::lookAt(Point3(0.0f, 0.0f, 0.0f),
                               Point3(0.0f, 0.0f, 1.0f),
                               Vector3(0.0f, 1.0f, 0.0f)));
  Matrix4 projection(Vectormath::Aos::CreatePerspectiveMatrix(
      45.0f * 180.0f / 3.14159f,
      4.0f / 3.0f,
      1.0,
      10.0));
  Matrix4 frustum(projection * view);

  // Check a box completely inside the frustum.
  EXPECT_TRUE(BoundingBox(Point3(1.0f, 1.0f, 1.0f),
                          Point3(2.0f, 2.0f, 2.0f)).InFrustum(frustum));
  // Check a box completely outside the frustum.
  EXPECT_FALSE(BoundingBox(Point3(11.0f, 11.0f, 11.0f),
                           Point3(12.0f, 12.0f, 12.0f)).InFrustum(frustum));
  // Check a box crossing the edge of the frustum.
  EXPECT_TRUE(BoundingBox(Point3(1.0f, 1.0f, 0.0f),
                          Point3(2.0f, 2.0f, 2.0f)).InFrustum(frustum));
}

#if 0  // TODO: change this to a ParamOperation
// Tests ParamBindFloat3sToBoundingBox
TEST_F(BoundingBoxTest, ParamBindFloat3sToBoundingBox) {
  Client client(g_service_locator);
  client.Init();

  Pack* pack = client.CreatePack();

  Transform* transform = pack->Create<Transform>();
  ASSERT_TRUE(transform != NULL);

  ParamBoundingBox* box_param = transform->CreateParam<ParamBoundingBox>("foo");
  ASSERT_TRUE(box_param != NULL);

  ParamBindFloat3sToBoundingBox* bind =
      pack->Create<ParamBindFloat3sToBoundingBox>();
  ASSERT_TRUE(bind != NULL);

  ParamFloat3* param1 = transform->CreateParam<ParamFloat3>("f1");
  ParamFloat3* param2 = transform->CreateParam<ParamFloat3>("f2");
  ASSERT_TRUE(param1 != NULL);
  ASSERT_TRUE(param2 != NULL);

  // Check that we can bind to the inputs and outputs.
  EXPECT_TRUE(
      bind->BindInput(ParamBindFloat3sToBoundingBox::kInput1Name, param1));
  EXPECT_TRUE(
      bind->BindInput(ParamBindFloat3sToBoundingBox::kInput2Name, param2));
  EXPECT_TRUE(
      bind->BindOutput(ParamBindFloat3sToBoundingBox::kOutputName, box_param));

  // Check that if we set the values, the make it through the bind and update
  // our bounding box.
  param1->set_value(Float3(-1.0f, 2.0f, -3.0f));
  param2->set_value(Float3(1.0f, -2.0f, 3.0f));
  BoundingBox box(box_param->value());

  EXPECT_EQ(box.min_extent()[0], -1.0f);
  EXPECT_EQ(box.min_extent()[1], -2.0f);
  EXPECT_EQ(box.min_extent()[2], -3.0f);
  EXPECT_EQ(box.max_extent()[0], 1.0f);
  EXPECT_EQ(box.max_extent()[1], 2.0f);
  EXPECT_EQ(box.max_extent()[2], 3.0f);

  pack->Destroy();
}
#endif

}  // namespace o3d
