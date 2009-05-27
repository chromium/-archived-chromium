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


// This file contains the definition of Primitive.

#include "core/cross/precompile.h"
#include "core/cross/primitive.h"
#include "core/cross/renderer.h"
#include "core/cross/error.h"
#include "core/cross/vertex_source.h"

namespace o3d {

O3D_DEFN_CLASS(Primitive, Element);

const char* Primitive::kStreamBankParamName =
    O3D_STRING_CONSTANT("streamBank");

bool Primitive::GetIndexCount(PrimitiveType primitive_type,
                              unsigned int primitive_count,
                              unsigned int* index_count) {
  switch (primitive_type) {
    case Primitive::POINTLIST:
      *index_count = primitive_count;
      return true;
    case Primitive::LINELIST:
      *index_count = primitive_count * 2;
      return true;
    case Primitive::LINESTRIP:
      *index_count = primitive_count + 1;
      return true;
    case Primitive::TRIANGLELIST:
      *index_count = primitive_count * 3;
      return true;
    case Primitive::TRIANGLESTRIP:
      *index_count = primitive_count + 2;
      return true;
    case Primitive::TRIANGLEFAN:
      *index_count = primitive_count + 2;
      return true;
    default:
      return false;
  }
}

ObjectBase::Ref Primitive::Create(ServiceLocator* service_locator) {
  Renderer* renderer = service_locator->GetService<Renderer>();
  if (NULL == renderer) {
    O3D_ERROR(service_locator) << "No Render Device Available";
    return ObjectBase::Ref();
  }

  return ObjectBase::Ref(renderer->CreatePrimitive());
}

Primitive::Primitive(ServiceLocator* service_locator)
    : Element(service_locator),
      primitive_type_(Primitive::TRIANGLELIST),
      number_vertices_(0),
      number_primitives_(0),
      start_index_(0) {
  RegisterParamRef(kStreamBankParamName, &stream_bank_ref_);
}

Primitive::~Primitive() {
}

namespace {

// A class to make it easy to access floats from a buffer given a stream.
template<typename T>
class FieldReadAccessor {
 public:
  FieldReadAccessor()
      : initialized_(false),
        buffer_(NULL),
        locked_(false),
        data_(NULL),
        offset_(0),
        stride_(0),
        real_start_index_(0),
        translated_end_index_(0) { }
  explicit FieldReadAccessor(const Stream& stream)
      : initialized_(true),
        buffer_(stream.field().buffer()),
        locked_(false),
        data_(NULL),
        offset_(stream.field().offset()),
        stride_(stream.field().buffer()->stride()),
        real_start_index_(stream.start_index()),
        translated_end_index_(stream.GetMaxVertices() - stream.start_index()) {
    if (real_start_index_ > translated_end_index_) {
      initialized_ = false;
      return;
    }
    if (buffer_) {
      locked_ = buffer_->Lock(Buffer::READ_ONLY, &data_);
    }
  }
  virtual ~FieldReadAccessor() {
    if (initialized_ && locked_) {
      buffer_->Unlock();
    }
  }
  virtual T& operator[](unsigned int translated_index) {
    if (translated_index >= translated_end_index_) {
      O3D_ERROR(buffer_->service_locator())
          << "Index " << real_start_index_ + translated_index
          << " into buffer '" << buffer_->name() << "' is out of range.";
      translated_index = 0;
    }
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(data_) + offset_ +
        stride_ * (translated_index + real_start_index_));
  }
  void Initialize(const Field& field, unsigned int start_index,
      unsigned int length) {
    buffer_ = field.buffer();
    locked_ = false;
    data_ = NULL;
    offset_ = field.offset();
    stride_ = buffer_->stride();
    real_start_index_ = start_index;
    unsigned int max_index = std::min(length, buffer_->num_elements());
    translated_end_index_ = max_index - start_index;
    // Check for unsigned wrap.
    if (translated_end_index_ > max_index) {
      translated_end_index_ = 0;
    }
    if (start_index > translated_end_index_) {
      return;  // It will not be valid in this case.
    }
    if (buffer_) {
      locked_ = buffer_->Lock(Buffer::READ_ONLY, &data_);
    }
    initialized_ = true;
  }
  bool Valid() {
    return initialized_ && locked_;
  }

  unsigned max_index() const {
    return translated_end_index_ - real_start_index_;
  }

 protected:
  bool initialized_;
  Buffer* buffer_;
  bool locked_;
  void* data_;
  unsigned int offset_;
  unsigned int stride_;
  unsigned int real_start_index_;
  unsigned int translated_end_index_;

  DISALLOW_COPY_AND_ASSIGN(FieldReadAccessor);
};

class FieldReadAccessorUnsignedInt : public FieldReadAccessor<unsigned int> {
 public:
  FieldReadAccessorUnsignedInt()
      : FieldReadAccessor<unsigned int>(),
        just_count_(false) { }
  void InitializeJustCount(unsigned int start_index, unsigned int length) {
    initialized_ = true;
    real_start_index_ = start_index;
    translated_end_index_ = length;
    locked_ = false;
    just_count_ = true;
  }
  virtual unsigned int& operator[](unsigned int translated_index) {
    if (translated_index >= translated_end_index_) {
      O3D_ERROR(buffer_->service_locator())
          << "Index " << real_start_index_ + translated_index
          << " into buffer '" << buffer_->name() << "' is out of range.";
      translated_index = 0;
    }
    if (just_count_) {
      counter_ = real_start_index_ + translated_index;
      return counter_;
    } else {
      return FieldReadAccessor<unsigned int>::operator[](translated_index);
    }
  }
 protected:
  unsigned int counter_;
  bool just_count_;
};

// Attempts to initialize a FieldReadAccessor for the stream of vertices of
// the primitive.  Returns success.
bool GetVerticesAccessor(const Primitive* primitive,
                         int position_stream_index,
                         FieldReadAccessor<Point3>* accessor) {
  if (!(accessor && primitive))
    return false;

  const StreamBank* stream_bank = primitive->stream_bank();
  if (!stream_bank) {
    O3D_ERROR(primitive->service_locator())
        << "No stream bank on Primitive '" << primitive->name() << "'";
    return false;
  }

  const Stream* vertex_stream = stream_bank->GetVertexStream(
      Stream::POSITION,
      position_stream_index);

  if (!vertex_stream) {
    O3D_ERROR(primitive->service_locator())
        << "No POSITION stream index "
        << position_stream_index;
    return false;
  }

  const Field& field = vertex_stream->field();

  if (!field.buffer()) {
    O3D_ERROR(primitive->service_locator()) << "Vertex Buffer not set";
    return false;
  }

  if (!field.IsA(FloatField::GetApparentClass())) {
    O3D_ERROR(primitive->service_locator()) << "POSITION stream index "
                                            << position_stream_index
                                            << " is not a FLOAT stream";
    return false;
  }

  if (field.num_components() != 3) {
    O3D_ERROR(primitive->service_locator())
        << "POSITION stream index " << position_stream_index
        << " does not have 3 components";
    return false;
  }

  accessor->Initialize(field, vertex_stream->start_index(),
                       vertex_stream->GetMaxVertices());

  if (!accessor->Valid()) {
    O3D_ERROR(primitive->service_locator())
        << "Could not lock vertex buffer";
    return false;
  }

  return true;
}

// Attempts to initialize a FieldReadAccessor for the stream of indices of
// the primitive.  Returns success.
bool GetIndicesAccessor(const Primitive* primitive,
                        FieldReadAccessor<unsigned int>* accessor,
                        unsigned start_index,
                        unsigned index_count) {
  if (!(accessor && primitive))
    return false;

  const IndexBuffer* buffer = primitive->index_buffer();
  DCHECK(buffer);
  if (!buffer->index_field())
    return false;

  accessor->Initialize(*buffer->index_field(), start_index, index_count);

  if (!accessor->Valid()) {
    O3D_ERROR(primitive->service_locator())
        << "Could not lock index buffer";
    return false;
  }

  return true;
}

}  // anonymous namespace

bool Primitive::WalkPolygons(
    int position_stream_index,
    PolygonFunctor* polygon_functor) const {
  DLOG_ASSERT(polygon_functor);

  FieldReadAccessor<Point3> vertices;
  FieldReadAccessorUnsignedInt indices;

  if (!GetVerticesAccessor(this, position_stream_index, &vertices))
    return false;

  unsigned int index_count;
  if (indexed()) {
    if (!Primitive::GetIndexCount(primitive_type(),
                                  number_primitives(),
                                  &index_count)) {
      O3D_ERROR(service_locator())
          << "Unknown Primitive Type in GetIndexCount: " << primitive_type_;
      return false;
    }

    if (!GetIndicesAccessor(this, &indices, start_index_, index_count)) {
      return false;
    }

    index_count = std::min(index_count, indices.max_index());
  } else {
    index_count = number_vertices();
    indices.InitializeJustCount(start_index_, index_count);
  }

  // If there are no vertices then exit early.
  if (vertices.max_index() == 0) {
    if (indices.max_index() > 0) {
      O3D_ERROR(service_locator())
          << "Indices on primitive '" << name() << "' reference a vertexbuffer"
          << "with 0 elements.";
    }
    return indices.max_index() == 0;
  }

  switch (primitive_type()) {
    case Primitive::TRIANGLELIST: {
      int prim = 0;
      for (unsigned int prim_base = 0;
           prim_base + 2 < index_count;
           prim_base += 3) {
        polygon_functor->ProcessTriangle(
            prim,
            vertices[indices[prim_base + 0]],
            vertices[indices[prim_base + 1]],
            vertices[indices[prim_base + 2]]);
        ++prim;
      }
      break;
    }
    case Primitive::TRIANGLESTRIP: {
      if (index_count > 2) {
        int prim = 0;
        unsigned local_indices[3];
        local_indices[0] = indices[0];
        local_indices[1] = indices[1];
          for (unsigned int prim_base = 2;
             prim_base < index_count;
             ++prim_base) {
          local_indices[2] = indices[prim_base];
          // flip ordering passed to ProcessTriangle since triangle strips flip
          // ordering.
          if ((prim & 1) == 0) {
            polygon_functor->ProcessTriangle(
                prim,
                vertices[local_indices[0]],
                vertices[local_indices[1]],
                vertices[local_indices[2]]);
          } else {
            polygon_functor->ProcessTriangle(
                prim,
                vertices[local_indices[0]],
                vertices[local_indices[2]],
                vertices[local_indices[1]]);
          }
          local_indices[0] = local_indices[1];
          local_indices[1] = local_indices[2];
          ++prim;
        }
      }
      break;
    }
    case Primitive::TRIANGLEFAN: {
      if (index_count > 2) {
        int prim = 0;
        unsigned local_indices[3];
        local_indices[0] = indices[0];
        local_indices[1] = indices[1];
          for (unsigned int prim_base = 2;
             prim_base < index_count;
             ++prim_base) {
          local_indices[2] = indices[prim_base];
          polygon_functor->ProcessTriangle(
              prim,
              vertices[local_indices[0]],
              vertices[local_indices[1]],
              vertices[local_indices[2]]);
          local_indices[1] = local_indices[2];
          ++prim;
        }
      }
      break;
    }
    case Primitive::LINELIST: {
      int prim = 0;
      for (unsigned int prim_base = 0;
           prim_base + 1 < index_count;
           prim_base += 2) {
        polygon_functor->ProcessLine(
            prim,
            vertices[indices[prim_base + 0]],
            vertices[indices[prim_base + 1]]);
        ++prim;
      }
      break;
    }
    case Primitive::LINESTRIP: {
      if (index_count > 1) {
        int prim = 0;
        unsigned local_indices[2];
        local_indices[0] = indices[0];
        for (unsigned int prim_base = 1;
             prim_base < index_count;
             ++prim_base) {
          local_indices[1] = indices[prim_base];
          polygon_functor->ProcessLine(
              prim,
              vertices[local_indices[0]],
              vertices[local_indices[1]]);
          local_indices[0] = local_indices[1];
          ++prim;
        }
      }
      break;
    }
    case Primitive::POINTLIST: {
      int prim = 0;
      for (unsigned int prim_base = 0; prim_base < index_count; ++prim_base) {
        polygon_functor->ProcessPoint(prim, vertices[indices[prim_base]]);
        ++prim;
      }
      break;
    }
  }

  return true;
}

namespace {

class IntersectRayHelper : public Primitive::PolygonFunctor {
 public:
  IntersectRayHelper(State::Cull cull,
                     const Point3& start,
                     const Point3& end,
                     RayIntersectionInfo* result)
      : cull_(cull),
        start_(start),
        end_(end),
        result_(result),
        closest_distance_squared_(0) {
  }

  virtual void ProcessTriangle(unsigned primitive_index,
                               const Point3& p0,
                               const Point3& p1,
                               const Point3& p2) {
    Point3 intersection_point;
    bool intersected = false;
    if (cull_ == State::CULL_NONE || cull_ == State::CULL_CCW) {
      intersected = RayIntersectionInfo::IntersectTriangle(
          start_,
          end_,
          p0,
          p1,
          p2,
          &intersection_point);
    }
    if (!intersected &&
        (cull_ == State::CULL_NONE || cull_ == State::CULL_CW)) {
      intersected = RayIntersectionInfo::IntersectTriangle(
          start_,
          end_,
          p0,
          p2,
          p1,
          &intersection_point);
    }
    if (intersected) {
      // intersection
      bool update = false;
      float distance_squared;
      if (!result_->intersected()) {
        update = true;
        result_->set_intersected(true);
        distance_squared = lengthSqr(intersection_point - start_);
      } else {
        distance_squared = lengthSqr(intersection_point - start_);
        if (distance_squared < closest_distance_squared_) {
          update = true;
        }
      }
      if (update) {
        closest_distance_squared_ = distance_squared;
        result_->set_position(intersection_point);
        result_->set_primitive_index(primitive_index);
      }
    }
  }

  virtual void ProcessLine(unsigned primitive_index,
                           const Point3& p0,
                           const Point3& p1) {
    // For now, lines are not intersected
  }
  virtual void ProcessPoint(unsigned primitive_index,
                            const Point3& p) {
    // For now, points are not intersected
  }
 private:
  State::Cull cull_;
  const Point3& start_;
  const Point3& end_;
  RayIntersectionInfo* result_;
  float closest_distance_squared_;
};

class BoundingBoxHelper : public Primitive::PolygonFunctor {
 public:
  BoundingBoxHelper()
      : first_(true) {
  }
  virtual void ProcessTriangle(unsigned primitive_index,
                               const Point3& p0,
                               const Point3& p1,
                               const Point3& p2) {
    if (first_) {
      first_ = false;
      min_extent_ = p0;
      max_extent_ = p0;
    }
    min_extent_ = minPerElem(min_extent_, p0);
    max_extent_ = maxPerElem(max_extent_, p0);
    min_extent_ = minPerElem(min_extent_, p1);
    max_extent_ = maxPerElem(max_extent_, p1);
    min_extent_ = minPerElem(min_extent_, p2);
    max_extent_ = maxPerElem(max_extent_, p2);
  }
  virtual void ProcessLine(unsigned primitive_index,
                           const Point3& p0,
                           const Point3& p1) {
    if (first_) {
      first_ = false;
      min_extent_ = p0;
      max_extent_ = p0;
    }
    min_extent_ = minPerElem(min_extent_, p0);
    max_extent_ = maxPerElem(max_extent_, p0);
    min_extent_ = minPerElem(min_extent_, p1);
    max_extent_ = maxPerElem(max_extent_, p1);
  }
  virtual void ProcessPoint(unsigned primitive_index,
                            const Point3& p) {
    if (first_) {
      first_ = false;
      min_extent_ = p;
      max_extent_ = p;
    }
    min_extent_ = minPerElem(min_extent_, p);
    max_extent_ = maxPerElem(max_extent_, p);
  }
  void GetBoundingBox(BoundingBox* result) {
    DLOG_ASSERT(result);
    *result = BoundingBox(min_extent_, max_extent_);
  }
 private:
  bool first_;
  Point3 min_extent_;
  Point3 max_extent_;
};

}  // anonymouse namespace.

void Primitive::IntersectRay(int position_stream_index,
                             State::Cull cull,
                             const Point3& start,
                             const Point3& end,
                             RayIntersectionInfo* result) const {
  if (!result) {
    DLOG_ASSERT(false);
    return;
  }
  result->Reset();
  IntersectRayHelper helper(cull, start, end, result);
  if (WalkPolygons(position_stream_index, &helper)) {
    result->set_valid(true);
  }
}

void Primitive::GetBoundingBox(int position_stream_index,
                               BoundingBox* result) const {
  if (!result) {
    DLOG_ASSERT(false);
    return;
  }

  BoundingBoxHelper helper;
  if (WalkPolygons(position_stream_index, &helper)) {
    helper.GetBoundingBox(result);
  } else {
    *result = BoundingBox();
  }
}
}  // namespace o3d
