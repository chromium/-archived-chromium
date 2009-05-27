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


// This file contains the declaration of RayIntersectionInfo

#ifndef O3D_CORE_CROSS_RAY_INTERSECTION_INFO_H_
#define O3D_CORE_CROSS_RAY_INTERSECTION_INFO_H_

#include "core/cross/types.h"

namespace o3d {

//  A RayIntersectionInfo is used to return the results of ray intersection
//  tests.
class RayIntersectionInfo {
 public:
  RayIntersectionInfo()
      : valid_(false),
        intersected_(false),
        primitive_index_(-1) {
  }

  // Puts this info in the default, unset state.
  void Reset() {
    set_valid(false);
    set_intersected(false);
    set_primitive_index(-1);
  }

  // True if this ray intersection info is valid. For example if you call
  // element->IntersectRay on an element that has no vertex buffers the result
  // will be invalid.
  bool valid() const {
    return valid_;
  }

  void set_valid(bool valid) {
    valid_ = valid;
  }

  // True if this ray intersection intersected something.
  bool intersected() const {
    return intersected_;
  }

  void set_intersected(bool intersected) {
    intersected_ = intersected;
  }

  // The position the ray intersected something.
  const Point3& position() const {
    return position_;
  }

  void set_position(const Point3& position) {
    position_ = position;
  }

  // The index of the primitive that was intersected.
  int primitive_index() const {
    return primitive_index_;
  }

  void set_primitive_index(int index) {
    primitive_index_ = index;
  }

  // Intersects a ray with a trianagle.
  // Parameters:
  //   start: position of start of ray in local space.
  //   end: position of end of ray in local space.
  //   vert0: first vertex of triangle.
  //   vert1: second vertex of triangle.
  //   vert2: third vertex of triangle.
  //   intersectionPoint: pointer to Point3 to receive the position of
  //       intersection.
  // Returns:
  //    true if triangle was intersected by ray.
  static bool IntersectTriangle(const Point3& start,
                                const Point3& end,
                                const Point3& vert0,
                                const Point3& vert1,
                                const Point3& vert2,
                                Point3* intersectionPoint);

 private:
  bool valid_;
  bool intersected_;
  Point3 position_;
  int primitive_index_;
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_RAY_INTERSECTION_INFO_H_




