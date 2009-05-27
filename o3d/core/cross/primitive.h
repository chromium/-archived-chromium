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


// This file cotnains the declaration of Primitive.

#ifndef O3D_CORE_CROSS_PRIMITIVE_H_
#define O3D_CORE_CROSS_PRIMITIVE_H_

#include <vector>
#include <map>
#include "core/cross/element.h"
#include "core/cross/stream_bank.h"
#include "core/cross/buffer.h"

namespace o3d {

class RenderContext;
class Pack;
class Material;
class ParamCache;

// The Primitive contains the geometry streams, vertex and index streams and a
// reference to a material. It represents a piece of a Shape that uses a single
// material. For example a cube with 6 faces where each face uses a different
// material would be represented as 1 Shape with 6 Primitives.
class Primitive : public Element {
 public:
  typedef SmartPointer<Primitive> Ref;

  // Param names for Primitive.
  static const char* kStreamBankParamName;

  virtual ~Primitive();

  // Types of geometric primitives used by Primitive.
  enum PrimitiveType {
    POINTLIST = 1,      /* Point list */
    LINELIST = 2,       /* Line list */
    LINESTRIP = 3,      /* Line Strip */
    TRIANGLELIST = 4,   /* Triangle List */
    TRIANGLESTRIP = 5,  /* Triangle Strip */
    TRIANGLEFAN = 6,    /* Triangle fan */
  };

  // Gets the StreamBank this Primitive is using.
  StreamBank* stream_bank() const {
    return stream_bank_ref_->value();
  }

  // Sets the StreamBank this Primitive is using.
  void set_stream_bank(StreamBank* stream_bank) {
    stream_bank_ref_->set_value(stream_bank);
  }

  // Computes the number of indices a given set of primitives will use,
  // depending on the primitive type. Returns false if
  static bool GetIndexCount(PrimitiveType primitive_type,
                            unsigned int primitive_count,
                            unsigned int* index_count);

  // Binds an index buffer to the primitive.
  void set_index_buffer(IndexBuffer* buffer) {
    index_buffer_ = IndexBuffer::Ref(buffer);
  }

  // Returns the index buffer bound to the primitive or NULL if non has been
  // bound.
  IndexBuffer* index_buffer() const {
    return index_buffer_;
  }

  // Sets the type of geometric primitives used by the shape.
  void set_primitive_type(PrimitiveType type) {
    primitive_type_ = type;
  }

  // Returns the types of geometric primitives used by the shape.
  PrimitiveType primitive_type() const {
    return primitive_type_;
  }

  // Sets the number of vertices used by the draw calls.
  void set_number_vertices(unsigned int number_vertices) {
    number_vertices_ = number_vertices;
  }

  // Returns the number of vertices used by the draw calls.
  unsigned int number_vertices() const {
    return number_vertices_;
  }

  // Sets the number of primitives used by the draw calls.
  void set_number_primitives(unsigned int number_primitives) {
    number_primitives_ = number_primitives;
  }

  // Returns the number of primitives used by the draw calls.
  unsigned int number_primitives() const {
    return number_primitives_;
  }

  // Sets the index of the first vertex to render.
  void set_start_index(unsigned int start_index) {
    start_index_ = start_index;
  }

  // Gets the index of the first vertex to render.
  unsigned int start_index() const {
    return start_index_;
  }

  // Returns whether the geometry should be assumed to be indexed.
  // If there are no indices given, we assume non-indexed geometry.
  bool indexed() const {
    return !index_buffer_.IsNull();
  }

  // Render this Primitive.
  virtual void Render(Renderer* renderer,
                      DrawElement* draw_element,
                      Material* material,
                      ParamObject* param_object,
                      ParamCache* param_cache) = 0;

  // Overridden from Element (see element.h)
  // Computes the intersection of a ray in the same coordinate system as
  // the specified POSITION stream.
  virtual void IntersectRay(int position_stream_index,
                            State::Cull cull,
                            const Point3& start,
                            const Point3& end,
                            RayIntersectionInfo* result) const;

  // Overridden from Element (see element.h)
  // Computes the bounding box in same coordinate system as the specified
  // POSITION stream.
  virtual void GetBoundingBox(int position_stream_index,
                              BoundingBox* result) const;


  // A class for visiting each triangle in this primitive.
  class PolygonFunctor {
   public:
    virtual ~PolygonFunctor() { }
    virtual void ProcessTriangle(unsigned primitive_index,
                                 const Point3& p0,
                                 const Point3& p1,
                                 const Point3& p2) = 0;
    virtual void ProcessLine(unsigned primitive_index,
                             const Point3& p0,
                             const Point3& p1) = 0;
    virtual void ProcessPoint(unsigned primitive_index,
                              const Point3& p) = 0;
  };

  // Walks all the polygons in this primitive, calling the PolygonFunctor's
  // ProcessTriangle function for each one.
  bool WalkPolygons(int position_stream_index,
                    PolygonFunctor* geometry_functor) const;

 protected:
  explicit Primitive(ServiceLocator* service_locator);

  PrimitiveType primitive_type_;
  unsigned int number_vertices_;
  unsigned int number_primitives_;
  unsigned int start_index_;

  ParamStreamBank::Ref stream_bank_ref_;
  IndexBuffer::Ref index_buffer_;

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(Primitive, Element);
  DISALLOW_COPY_AND_ASSIGN(Primitive);
};

typedef std::vector<Primitive*> PrimitiveArray;
typedef std::vector<Primitive::Ref> PrimitiveRefArray;

}  // namespace o3d

#endif  // O3D_CORE_CROSS_PRIMITIVE_H_





