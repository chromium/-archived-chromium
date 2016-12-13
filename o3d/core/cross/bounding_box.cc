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


// This file contains the definition of BoundingBox which is a class that
// represents an Axis Aligned Bounding Box. It also contains the definition of
// ParamBoundingBox, a Param that cointains a BoundingBox as well as
// ParamBindFloat3sToBoundingBox which is a ParamBind that takes 2 Float3s and
// converts them to a BoundingBox.

#include "core/cross/precompile.h"
#include "core/cross/bounding_box.h"
#include "core/cross/ray_intersection_info.h"

namespace o3d {

O3D_DEFN_CLASS(ParamBoundingBox, Param);
// TODO: change this to a ParamOperation
// O3D_DEFN_CLASS(ParamBindFloat3sToBoundingBox, ParamBind);

// Adds a box to this box producing a bounding box that contains both.
void BoundingBox::Add(const BoundingBox& box, BoundingBox* result) const {
  DLOG_ASSERT(result != NULL);

  if (valid() && box.valid()) {
    *result = BoundingBox(
        Vectormath::Aos::minPerElem(min_extent(), box.min_extent()),
        Vectormath::Aos::maxPerElem(max_extent(), box.max_extent()));
  } else if (valid()) {
    *result = *this;
  } else if (box.valid()) {
    *result = box;
  } else {
    *result = BoundingBox();  // will be an invalid box.
  }
}

// Computing the bounding box of this box re-oriented by multiplying by a
// Matrix4.
void BoundingBox::Mul(const Matrix4& matrix, BoundingBox* result) const {
  DLOG_ASSERT(result != NULL);

  Point3 min_extent((matrix * min_extent_).getXYZ());
  Point3 max_extent(min_extent);

  Point3 temp;
  temp = Point3((matrix * max_extent_).getXYZ()),
  min_extent = Vectormath::Aos::minPerElem(temp, min_extent);
  max_extent = Vectormath::Aos::maxPerElem(temp, max_extent);
  temp = Point3((matrix * Point3(min_extent_.getX(),
                                 max_extent_.getY(),
                                 min_extent_.getZ())).getXYZ());
  min_extent = Vectormath::Aos::minPerElem(temp, min_extent);
  max_extent = Vectormath::Aos::maxPerElem(temp, max_extent);
  temp = Point3((matrix * Point3(min_extent_.getX(),
                                 min_extent_.getY(),
                                 max_extent_.getZ())).getXYZ());
  min_extent = Vectormath::Aos::minPerElem(temp, min_extent);
  max_extent = Vectormath::Aos::maxPerElem(temp, max_extent);
  temp = Point3((matrix * Point3(min_extent_.getX(),
                                 max_extent_.getY(),
                                 max_extent_.getZ())).getXYZ());
  min_extent = Vectormath::Aos::minPerElem(temp, min_extent);
  max_extent = Vectormath::Aos::maxPerElem(temp, max_extent);
  temp = Point3((matrix * Point3(max_extent_.getX(),
                                 min_extent_.getY(),
                                 min_extent_.getZ())).getXYZ());
  min_extent = Vectormath::Aos::minPerElem(temp, min_extent);
  max_extent = Vectormath::Aos::maxPerElem(temp, max_extent);
  temp = Point3((matrix * Point3(max_extent_.getX(),
                                 max_extent_.getY(),
                                 min_extent_.getZ())).getXYZ());
  min_extent = Vectormath::Aos::minPerElem(temp, min_extent);
  max_extent = Vectormath::Aos::maxPerElem(temp, max_extent);
  temp = Point3((matrix * Point3(max_extent_.getX(),
                                 min_extent_.getY(),
                                 max_extent_.getZ())).getXYZ());
  min_extent = Vectormath::Aos::minPerElem(temp, min_extent);
  max_extent = Vectormath::Aos::maxPerElem(temp, max_extent);

  *result = BoundingBox(min_extent, max_extent);
}

// Adapted from: Fast Ray-Box Intersection by Andrew Woo from "Graphics Gems",
// Academic Press, 1990
void BoundingBox::IntersectRay(const Point3& start,
                               const Point3& end,
                               RayIntersectionInfo* result) const {
  DLOG_ASSERT(result != NULL);

  result->Reset();
  if (valid()) {
    result->set_valid(true);
    result->set_intersected(true);  // Assume true.

    static const int kNumberOfDimensions = 3;
    static const int kRight = 0;
    static const int kLeft = 1;
    static const int kMiddle = 2;

    Vector3 dir(end - start);
    Point3 coord;
    bool inside = true;
    float quadrant[kNumberOfDimensions];
    int which_plane;
    float max_t[kNumberOfDimensions];
    float candidate_plane[kNumberOfDimensions];

    // Find candidate planes; this loop can be avoided if rays cast all from the
    // eye (assumes perpsective view).
    for (int i = 0; i < kNumberOfDimensions; ++i) {
      if (start[i] < min_extent_[i]) {
        quadrant[i] = kLeft;
        candidate_plane[i] = min_extent_[i];
        inside = false;
      } else if (start[i] >  max_extent_[i]) {
        quadrant[i] = kRight;
        candidate_plane[i] =  max_extent_[i];
        inside = false;
      } else  {
        quadrant[i] = kMiddle;
      }
    }

    // Ray origin inside bounding box.
    if (inside) {
      // TODO: Should this return true? I feel like maybe not. Or
      //   maybe ray_intersection_info should have a flag like "inside".
      result->set_position(start);
    } else {
      // Calculate T distances to candidate planes.
      for (int i = 0; i < kNumberOfDimensions; ++i) {
        if (quadrant[i] != kMiddle && dir[i] != 0.0f) {
          max_t[i] = (candidate_plane[i] - start[i]) / dir[i];
        } else {
          max_t[i] = -1.0f;
        }
      }

      // Get largest of the max_t's for final choice of intersection.
      which_plane = 0;
      for (int i = 1; i < kNumberOfDimensions; ++i) {
        if (max_t[which_plane] < max_t[i]) {
          which_plane = i;
        }
      }

      // Check final candidate actually inside box.
      if (max_t[which_plane] < 0.0f) {
        result->set_intersected(false);
      } else {
        for (int i = 0; i < kNumberOfDimensions; ++i) {
          if (which_plane != i) {
            coord[i] = start[i] + max_t[which_plane] * dir[i];
            if (coord[i] < min_extent_[i] || coord[i] > max_extent_[i]) {
              result->set_intersected(false);
              break;
            }
          } else {
            coord[i] = candidate_plane[i];
          }
        }

        // ray hits box.
        result->set_position(coord);
      }
    }
  }
}

// Returns true if the bounding box is inside the frustum matrix.
// It checks all 8 corners of the bounding box against the 6 frustum planes
// and determines whether there's at least one plane for which all 6 points lie
// on the outside side of it.  In that case it reports that the bounding box
// is outside the frustum.  Note that this is a conservative check in that
// it in certain cases it will report that a box is in the frustum even if it
// really isn't.  However if it reports that the box is outside then it's
// guaranteed to be outside.
bool BoundingBox::InFrustum(const Matrix4& matrix) const {
  Point3 box_max = max_extent();
  Point3 box_min = min_extent();

  // Compute the 8 corners of the box in local space.
  Vector4 box_points[8] = {
    Vector4(box_min.getX(), box_max.getY(), box_min.getZ(), 1),
    Vector4(box_min.getX(), box_min.getY(), box_max.getZ(), 1),
    Vector4(box_min.getX(), box_max.getY(), box_max.getZ(), 1),
    Vector4(box_max.getX(), box_min.getY(), box_min.getZ(), 1),
    Vector4(box_max.getX(), box_max.getY(), box_min.getZ(), 1),
    Vector4(box_max.getX(), box_min.getY(), box_max.getZ(), 1),
    Vector4(box_max),
    Vector4(box_min)};

  // Convert the bounding box corners to screen space and test them against
  // the six planes of the frustum.  The results of the tests are stored in six
  // consecutive bits in a bit field.  A one denotes that the point is on the
  // outside of a particular frustum plane.  The order of the bits is as
  // follows:
  // bit 0: x < -w
  // bit 1: x > w
  // bit 2: y < -w
  // bit 3: y > w
  // bit 4: z < 0
  // bit 5: z > w
  // The generated bitfields are ANDed together.  If at the end any of the
  // resulting bits has a value of one then it means that all 8 points lie
  // outside of one of the frustum planes and hence the entire bounding box
  // is outside the frustum.
  unsigned char bb_test = 0xff;
  for (int i = 0; i < 8; i++) {
    // Compute the coordinates of the corner in screen space.
    Vector4 point = matrix * box_points[i];

    // Test against all 6 of the frustum planes.
    unsigned char test1 = point.getX() < -point.getW();
    unsigned char test2 = ((point.getX() > point.getW()) << 1);
    unsigned char test3 = ((point.getY() < -point.getW()) << 2);
    unsigned char test4 = ((point.getY() > point.getW()) << 3);
    unsigned char test5 = ((point.getZ() < 0) << 4);
    unsigned char test6 = ((point.getZ() > point.getW()) << 5);

    // Put all the results in a single bitfield.
    unsigned char test_bits = test1 | test2 | test3 | test4 | test5 | test6;

    // Check if any of the tests match with the results from the other points.
    bb_test &= test_bits;

    // Early out.  If the points we processed so far there's not a single
    // frustum plane that they are all outside of, then there's no need to
    // continue.
    if (bb_test == 0)
      return true;
  }

  return (bb_test == 0);
}

ObjectBase::Ref ParamBoundingBox::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamBoundingBox(service_locator, false, false));
}

#if 0  // TODO: change this to a ParamOperation
const char* ParamBindFloat3sToBoundingBox::kInput1Name = "Input1";
const char* ParamBindFloat3sToBoundingBox::kInput2Name = "Input2";
const char* ParamBindFloat3sToBoundingBox::kOutputName = "Output";

ParamBindFloat3sToBoundingBox::ParamBindFloat3sToBoundingBox(const String& name)
    : ParamBind(service_locator) {
  RegisterInputPointer(kInput1Name, &source_bind_connection_1_);
  RegisterInputPointer(kInput2Name, &source_bind_connection_2_);
  RegisterOutputPointer(kOutputName, &destination_bind_connection_);
}

// Computes the product of the two source Params and stores the result in the
// destination Param.
void ParamBindFloat3sToBoundingBox::ComputeDestinationValues() {
  // Update the stored values of the two input Params
  UpdateSourceValues();

  Float3 value_1(source_bind_connection_1_->GetParam()->value());
  Float3 value_2(source_bind_connection_2_->GetParam()->value());
  ParamBoundingBox *dest_bounding_box =
      destination_bind_connection_->GetParam();

  dest_bounding_box->set_dynamic_value(
      BoundingBox(Point3(value_1[0], value_1[1], value_1[2]),
                  Point3(value_2[0], value_2[1], value_2[2])));
}

ObjectBase::Ref ParamBindFloat3sToBoundingBox::Create(
    ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamBindFloat3sToBoundingBox(service_locator));
}
#endif

}  // namespace o3d
