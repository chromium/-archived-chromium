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


// This file contains the declaration of BoundingBox which is a class that
// represents an Axis Aligned Bounding Box. It also contains the declaration of
// ParamBoundingBox, a Param that cointains a BoundingBox as well as
// ParamBindFloat3sToBoundingBox which is a ParamBind that takes 2 Float3s and
// converts them to a BoundingBox.

#ifndef O3D_CORE_CROSS_BOUNDING_BOX_H_
#define O3D_CORE_CROSS_BOUNDING_BOX_H_

#include "core/cross/types.h"
#include "core/cross/param.h"

namespace o3d {

class RayIntersectionInfo;

// Defines a class that represents an Axis Aligned Bounding Box.
class BoundingBox {
 public:
  // Constructs an uninitialized BoundingBox marking it as non valid.
  BoundingBox()  : min_extent_(0, 0, 0), max_extent_(0, 0, 0), valid_(false) { }

  // Constructs a BoundingBox.
  // Parameters:
  //   min_extent: minimum corner of the box.
  //   max_extent: maximum corner of the box.
  BoundingBox(const Point3& min_extent, const Point3& max_extent)
      : valid_(true),
        min_extent_(Vectormath::Aos::minPerElem(min_extent, max_extent)),
        max_extent_(Vectormath::Aos::maxPerElem(min_extent, max_extent)) {
  }

  // True if this bounding box has been initialized.
  // Returns:
  //   true if this bounding box has been initialized.
  bool valid() const {
    return valid_;
  }

  // The min extent of the box.
  // Returns:
  //   the min extent of the box. If the box is not valid the return value
  //   is undefined.
  const Point3& min_extent() const {
    return min_extent_;
  }

  // The max extent of the box.
  // Returns:
  //   the max extent of the box. If the box is not valid the return value
  //   is undefined.
  const Point3& max_extent() const {
    return max_extent_;
  }

  // Computing the bounding box of this box re-oriented by multiplying by a
  // Matrix4.
  // Paramaters:
  //   matrix: Matrix4 to multiple by.
  //   result: pointer to bounding box to store the result.
  void Mul(const Matrix4& matrix, BoundingBox* result) const;

  // Adds a bounding box to this box producing a bounding box that contains
  // both. If one box is invalid the result is the other box. If both boxes are
  // invalid the result will be an invalid box.
  // Parameters:
  //   box: bounding box to add to this box.
  //   result: pointer to bounding box to store the result.
  void Add(const BoundingBox& box, BoundingBox* result) const;

  // Checks if a ray defined in same coordinate system as this box intersects
  // this bounding box.
  // Parameters:
  //   start: position of start of ray in local space.
  //   end: position of end of ray in local space.
  //   result: RayIntersectionInfo structure to fill out with results. If
  //       result->valid() is false then something was wrong like using this
  //       function with an uninitialized bounding box. If result->intersected()
  //       is true then the ray intersected the box and result->position() is
  //       the exact point of intersection.
  void IntersectRay(const Point3& start,
                    const Point3& end,
                    RayIntersectionInfo* result) const;

  // Returns true if the bounding box is inside the frustum.
  // Parameter:
  //   matrix: Matrix to transform the box from its local space to view frustum
  //       space.
  bool InFrustum(const Matrix4& matrix) const;

 private:
  bool valid_;  // true if this bounding box has been initialized.
  Point3 min_extent_;
  Point3 max_extent_;
};

// A Param that contains a bounding box.
class ParamBoundingBox : public TypedParam<BoundingBox> {
 public:
  typedef SmartPointer<ParamBoundingBox> Ref;

 protected:
  ParamBoundingBox(ServiceLocator* service_locator,
                   bool dynamic,
                   bool read_only)
      : TypedParam<BoundingBox>(service_locator, dynamic, read_only) {
  }

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(ParamBoundingBox, Param);
  DISALLOW_COPY_AND_ASSIGN(ParamBoundingBox);
};

#if 0  // TODO: change this to a ParamOperation
// A bind operation of 2 Float3s into a bounding box.
class ParamBindFloat3sToBoundingBox : public ParamBind {
 public:
  typedef SmartPointer<ParamBindFloat3sToBoundingBox> Ref;

  static const char* kInput1Name;
  static const char* kInput2Name;
  static const char* kOutputName;

  // Creates a BoundingBox from 2 Float3s.
  virtual void ComputeDestinationValues();

 private:
  explicit ParamBindFloat3sToBoundingBox(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // The source Params.
  TypedBindConnection<ParamFloat3>* source_bind_connection_1_;
  TypedBindConnection<ParamFloat3>* source_bind_connection_2_;

  // The dest Param.
  TypedBindConnection<ParamBoundingBox>* destination_bind_connection_;

  O3D_DECL_CLASS(ParamBindFloat3sToBoundingBox, ParamBind)
  DISALLOW_COPY_AND_ASSIGN(ParamBindFloat3sToBoundingBox);
};
#endif

}  // namespace o3d

#endif  // O3D_CORE_CROSS_BOUNDING_BOX_H_
