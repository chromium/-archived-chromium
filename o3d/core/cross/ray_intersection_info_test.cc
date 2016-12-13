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


// This file implements unit tests for class RayIntersectionInfo.

#include "core/cross/client.h"
#include "tests/common/win/testing_common.h"
#include "core/cross/ray_intersection_info.h"

namespace o3d {

class RayIntersectionInfoTest : public testing::Test {
 protected:

  RayIntersectionInfoTest()
      : object_manager_(g_service_locator) {}

  virtual void SetUp();
  virtual void TearDown();

  Pack* pack() { return pack_; }

 private:
  ServiceDependency<ObjectManager> object_manager_;
  Pack *pack_;
};

void RayIntersectionInfoTest::SetUp() {
  pack_ = object_manager_->CreatePack();
}

void RayIntersectionInfoTest::TearDown() {
  pack_->Destroy();
}

TEST_F(RayIntersectionInfoTest, Basic) {
  RayIntersectionInfo ray_intersection_info;

  // Check that it defaults to NOT valid.
  EXPECT_FALSE(ray_intersection_info.valid());

  // Check that it defaults to NOT intersected.
  EXPECT_FALSE(ray_intersection_info.intersected());
}

TEST_F(RayIntersectionInfoTest, IntersectTriangle) {
  RayIntersectionInfo ray_intersection_info;

  Point3 point1(0.0f, 0.0f, 0.0f);
  Point3 point2(0.0f, 1.0f, 0.0f);
  Point3 point3(1.0f, 0.0f, 0.0f);

  // Check that a ray intersects a triangle.
  Point3 result;
  EXPECT_TRUE(RayIntersectionInfo::IntersectTriangle(
     Point3(0.25f, 0.25f, -1.0f),
     Point3(0.25f, 0.25f,  1.0f),
     point1,
     point2,
     point3,
     &result));
  EXPECT_EQ(result.getX(), 0.25f);
  EXPECT_EQ(result.getY(), 0.25f);
  EXPECT_EQ(result.getZ(), 0.0f);

  // Check that a ray does NOT intersects a triangle.
  EXPECT_FALSE(RayIntersectionInfo::IntersectTriangle(
     Point3(1.25f, 0.25f, -1.0f),
     Point3(1.25f, 0.25f,  1.0f),
     point1,
     point2,
     point3,
     &result));

  // Check opposite winding of points.

  // Check that a ray intersects a triangle.
  EXPECT_FALSE(RayIntersectionInfo::IntersectTriangle(
     Point3(0.25f, 0.25f, -1.0f),
     Point3(0.25f, 0.25f,  1.0f),
     point1,
     point3,
     point2,
     &result));
  EXPECT_EQ(result.getX(), 0.25f);
  EXPECT_EQ(result.getY(), 0.25f);
  EXPECT_EQ(result.getZ(), 0.0f);

  // Check that a ray does NOT intersects a triangle.
  EXPECT_FALSE(RayIntersectionInfo::IntersectTriangle(
     Point3(1.25f, 0.25f, -1.0f),
     Point3(1.25f, 0.25f,  1.0f),
     point1,
     point3,
     point2,
     &result));
}

}  // namespace o3d
