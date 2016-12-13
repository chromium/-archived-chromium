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


// This file contains the definition for class RayIntersectionInfo.

#include "core/cross/precompile.h"
#include "core/cross/ray_intersection_info.h"

namespace o3d {

static const float kEpsilon = 0.000001f;

// TODO: Someone who is better at math, please optimize this.
bool RayIntersectionInfo::IntersectTriangle(const Point3& start,
                                            const Point3& end,
                                            const Point3& vert0,
                                            const Point3& vert1,
                                            const Point3& vert2,
                                            Point3* intersectionPoint) {
  Vector3 ab(vert1 - vert0);
  Vector3 ac(vert2 - vert0);
  Vector3 qp(start - end);

  Vector3 n(cross(ab, ac));

  float d = dot(qp, n);
  if (d <= 0.0f) {
    return false;
  }

  Vector3 ap(start - vert0);
  float t = dot(ap, n);
  if (t < 0.0f) {
    return false;
  }
  if (t > d) {
    return false;
  }

  Vector3 e(cross(qp, ap));
  float v = dot(ac, e);
  if (v < 0.0f || v > d) {
    return false;
  }

  float w = -dot(ab, e);
  if (w < 0.0f || v + w > d) {
    return false;
  }

  float ood = 1.0f / d;
  t *= ood;
  v *= ood;
  w *= ood;
  float u = 1.0f - v -w;

  *intersectionPoint = Point3(
      Vector3(vert0) * u + Vector3(vert1) * v + Vector3(vert2) * w);

  return true;
}

}  // namespace o3d



