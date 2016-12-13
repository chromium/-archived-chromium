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


// This file contains the definitions for the Buffer, VertexBuffer and
// IndexBuffer.

#include "core/cross/precompile.h"
#include <vector>
#include "core/cross/field.h"
#include "core/cross/error.h"
#include "core/cross/buffer.h"
#include "core/cross/types.h"
#include "core/cross/renderer.h"
#include "import/cross/memory_stream.h"

namespace o3d {

O3D_DEFN_CLASS(Field, NamedObject);
O3D_DEFN_CLASS(FloatField, Field);
O3D_DEFN_CLASS(UInt32Field, Field);
O3D_DEFN_CLASS(UByteNField, Field);

namespace {

// Sets a field from a specific type of source converting through a
// convertFunction.
template <typename SourceType,
          typename DestinationType,
          DestinationType convert_function(SourceType value)>
void SetFrom(const SourceType* source,
             unsigned source_stride,
             Field* field,
             unsigned destination_start_index,
             unsigned num_elements) {
  if (!field->RangeValid(destination_start_index, num_elements)) {
    return;
  }

  Buffer* buffer = field->buffer();
  o3d::BufferLockHelper helper(buffer);
  void* buffer_data = helper.GetData(o3d::Buffer::WRITE_ONLY);
  if (!buffer_data) {
    O3D_ERROR(field->service_locator())
        << "could not lock buffer for field '" << field->name() << "'";
    return;
  }
  unsigned destination_stride = buffer->stride();
  unsigned num_components = field->num_components();
  DestinationType* destination =
      PointerFromVoidPointer<DestinationType*>(
          buffer_data,
          destination_start_index * destination_stride + field->offset());
  for (; num_elements; --num_elements) {
    for (unsigned cc = 0; cc < num_components; ++cc) {
      destination[cc] = convert_function(source[cc]);
    }
    source += source_stride;
    destination = AddPointerOffset(destination, destination_stride);
  }
}

template <typename SourceType,
          typename DestinationType,
          DestinationType convert_function(SourceType value)>
void SetFromWithSwizzle(const SourceType* source,
                        unsigned source_stride,
                        Field* field,
                        unsigned destination_start_index,
                        unsigned num_elements,
                        const int* swizzle_table) {
  if (!field->RangeValid(destination_start_index, num_elements)) {
    return;
  }

  Buffer* buffer = field->buffer();
  o3d::BufferLockHelper helper(buffer);
  void* buffer_data = helper.GetData(o3d::Buffer::WRITE_ONLY);
  if (!buffer_data) {
    O3D_ERROR(field->service_locator())
        << "could not lock buffer for field '" << field->name() << "'";
    return;
  }
  unsigned destination_stride = buffer->stride();
  unsigned num_components = field->num_components();
  DestinationType* destination =
      PointerFromVoidPointer<DestinationType*>(
          buffer_data,
          destination_start_index * destination_stride + field->offset());
  for (; num_elements; --num_elements) {
    for (unsigned cc = 0; cc < num_components; ++cc) {
      destination[swizzle_table[cc]] = convert_function(source[cc]);
    }
    source += source_stride;
    destination = AddPointerOffset(destination, destination_stride);
  }
}

// Gets a field copying into a specific type of destination converting through a
// convertFunction.
template <typename SourceType,
          typename DestinationType,
          DestinationType convert_function(SourceType value)>
void GetAs(const Field* field_c,
           unsigned source_start_index,
           DestinationType* destination,
           unsigned destination_stride,
           unsigned num_elements) {
  Field* field = const_cast<Field*>(field_c);
  if (!field->RangeValid(source_start_index, num_elements)) {
    return;
  }

  Buffer* buffer = field->buffer();
  o3d::BufferLockHelper helper(buffer);
  void* buffer_data = helper.GetData(o3d::Buffer::READ_ONLY);
  if (!buffer_data) {
    O3D_ERROR(field->service_locator())
        << "could not lock buffer for field '" << field->name() << "'";
    return;
  }
  unsigned source_stride = buffer->stride();
  unsigned num_components = field->num_components();
  SourceType* source = PointerFromVoidPointer<SourceType*>(
      buffer_data,
      source_start_index * source_stride + field->offset());
  for (; num_elements; --num_elements) {
    for (unsigned cc = 0; cc < num_components; ++cc) {
      destination[cc] = convert_function(source[cc]);
    }
    source = AddPointerOffset(source, source_stride);
    destination += destination_stride;
  }
}


// Gets a field copying into a specific type of destination converting through a
// convertFunction.
template <typename SourceType,
          typename DestinationType,
          DestinationType convert_function(SourceType value)>
void GetAsWithSwizzle(const Field* field_c,
                      unsigned source_start_index,
                      DestinationType* destination,
                      unsigned destination_stride,
                      unsigned num_elements,
                      const int* swizzle_table) {
  Field* field = const_cast<Field*>(field_c);
  if (!field->RangeValid(source_start_index, num_elements)) {
    return;
  }

  Buffer* buffer = field->buffer();
  o3d::BufferLockHelper helper(buffer);
  void* buffer_data = helper.GetData(o3d::Buffer::READ_ONLY);
  if (!buffer_data) {
    O3D_ERROR(field->service_locator())
        << "could not lock buffer for field '" << field->name() << "'";
    return;
  }
  unsigned source_stride = buffer->stride();
  unsigned num_components = field->num_components();
  SourceType* source = PointerFromVoidPointer<SourceType*>(
      buffer_data,
      source_start_index * source_stride + field->offset());
  for (; num_elements; --num_elements) {
    for (unsigned cc = 0; cc < num_components; ++cc) {
      destination[cc] = convert_function(source[swizzle_table[cc]]);
    }
    source = AddPointerOffset(source, source_stride);
    destination += destination_stride;
  }
}

inline float ConvertFloatToFloat(float value) {
  return value;
}

// Note that |value| is an int here since we want to avoid loading
// an incorrectly swapped value into a float register
inline float ConvertLittleEndianFloatToFloat(uint32 value) {
  union UInt32FloatUnion {
    uint32 i;
    float f;
  };
  UInt32FloatUnion u;
  u.i = MemoryReadStream::GetLittleEndianUInt32(
      reinterpret_cast<uint32*>(&value));
  return u.f;
}

inline uint32 ConvertLittleEndianUInt32ToUInt32(uint32 value) {
  return MemoryReadStream::GetLittleEndianUInt32(
      reinterpret_cast<uint32*>(&value));
}

inline float ConvertUInt32ToFloat(uint32 value) {
  return static_cast<float>(value);
}

inline float ConvertUByteNToFloat(uint8 value) {
  return static_cast<float>(value) / 255.0f;
}

inline uint32 ConvertFloatToUInt32(float value) {
  return static_cast<uint32>(std::max(0.0f, value));
}

inline uint32 ConvertUInt32ToUInt32(uint32 value) {
  return value;
}

inline uint32 ConvertUByteNToUInt32(uint8 value) {
  return static_cast<uint32>(value > 0);
}

inline uint8 ConvertFloatToUByteN(float value) {
  return static_cast<uint8>(floorf(
      std::min(1.0f, std::max(0.0f, value)) * 255.0f + 0.5f));
}

inline uint8 ConvertUInt32ToUByteN(uint32 value) {
  return static_cast<uint8>(std::min<unsigned>(255, value));
}

inline uint8 ConvertUByteNToUByteN(uint8 value) {
  return value;
}

}  // anonymous namespace.

Field::Field(ServiceLocator* service_locator,
             Buffer* buffer,
             unsigned num_components,
             unsigned offset)
    : NamedObject(service_locator),
      buffer_(buffer),
      num_components_(num_components),
      offset_(offset) {
  DCHECK(num_components > 0);
}

bool Field::RangeValid(unsigned int start_index, unsigned int num_elements) {
  if (!buffer_) {
    O3D_ERROR(service_locator())
        << "The buffer for field '" << name() << "' no longer exists";
    return false;
  }
  if (start_index + num_elements > buffer_->num_elements() ||
      start_index + num_elements < start_index) {
    O3D_ERROR(service_locator())
        << "Range is not valid for Buffer '"<< buffer_->name()
        << "' on Field '" << name() << "'";
    return false;
  }
  return true;
}

void Field::Copy(const Field& source) {
  if (!source.IsA(GetClass())) {
    O3D_ERROR(service_locator())
      << "source field of type " << source.GetClassName()
      << " is not compatible with field of type " << GetClassName();
    return;
  }
  if (!source.buffer()) {
    O3D_ERROR(service_locator()) << "source buffer is null";
    return;
  }
  if (!buffer()) {
    O3D_ERROR(service_locator()) << "destination buffer is null";
    return;
  }
  ConcreteCopy(source);
}

// FloatField -------------------

FloatField::FloatField(ServiceLocator* service_locator,
                       Buffer* buffer,
                       unsigned num_components,
                       unsigned offset)
    : Field(service_locator, buffer, num_components, offset) {
}

size_t FloatField::GetFieldComponentSize() const {
  return sizeof(float);  // NOLINT
}

void FloatField::SetFromFloats(const float* source,
                               unsigned source_stride,
                               unsigned destination_start_index,
                               unsigned num_elements) {
  SetFrom<const float, float, ConvertFloatToFloat>(
      source, source_stride, this, destination_start_index, num_elements);
}

void FloatField::SetFromUInt32s(const uint32* source,
                                unsigned source_stride,
                                unsigned destination_start_index,
                                unsigned num_elements) {
  SetFrom<const uint32, float, ConvertUInt32ToFloat>(
      source, source_stride, this, destination_start_index, num_elements);
}

void FloatField::SetFromUByteNs(const uint8* source,
                                unsigned source_stride,
                                unsigned destination_start_index,
                                unsigned num_elements) {
  SetFrom<const uint8, float, ConvertUByteNToFloat>(
      source, source_stride, this, destination_start_index, num_elements);
}

bool FloatField::SetFromMemoryStream(MemoryReadStream* stream) {
  if (!buffer()) {
    O3D_ERROR(service_locator())
        << "The buffer for field '" << name() << "' no longer exists";
    return false;
  }

  size_t num_elements = buffer()->num_elements();

  // sanity check that the stream has enough data
  if (stream->GetRemainingByteCount() < num_elements * size()) {
    return false;
  }

  // Note that we're interpreting the source as uint32 since that's
  // what ConvertLittleEndianFloatToFloat wants (byte swapping for
  // float32 and uint32 are the same).  Interpreting floating point
  // values before they're byte-swapped can cause problems...
  const uint32 *source = stream->GetDirectMemoryPointerAs<const uint32>();

  stream->Skip(num_elements * size());

  SetFrom<const uint32, float, ConvertLittleEndianFloatToFloat>(
      source, num_components(), this, 0, num_elements);

  return true;
}

void FloatField::GetAsFloats(unsigned source_start_index,
                             float* destination,
                             unsigned destination_stride,
                             unsigned num_elements) const {
  GetAs<const float, float, ConvertFloatToFloat>(
      this,
      source_start_index,
      destination,
      destination_stride,
      num_elements);
}

void FloatField::ConcreteCopy(const Field& source) {
  DCHECK(source.IsA(GetClass()));
  DCHECK(source.buffer());
  unsigned num_components = source.num_components();
  unsigned num_elements = source.buffer()->num_elements();
  scoped_array<float> temp(new float[num_components * num_elements]);
  source.GetAsFloats(0, temp.get(), num_components, num_elements);
  SetFromFloats(temp.get(), num_components, 0, num_elements);
}

Field::Ref FloatField::Create(ServiceLocator* service_locator,
                              Buffer* buffer,
                              unsigned num_components,
                              unsigned offset) {
  return Field::Ref(
      new FloatField(service_locator, buffer, num_components, offset));
}

// UInt32Field -------------------

UInt32Field::UInt32Field(ServiceLocator* service_locator,
                         Buffer* buffer,
                         unsigned num_components,
                         unsigned offset)
    : Field(service_locator, buffer, num_components, offset) {
}

size_t UInt32Field::GetFieldComponentSize() const {
  return sizeof(uint32);  // NOLINT
}

void UInt32Field::SetFromFloats(const float* source,
                                unsigned source_stride,
                                unsigned destination_start_index,
                                unsigned num_elements) {
  SetFrom<const float, uint32, ConvertFloatToUInt32>(
      source, source_stride, this, destination_start_index, num_elements);
}

void UInt32Field::SetFromUInt32s(const uint32* source,
                                 unsigned source_stride,
                                 unsigned destination_start_index,
                                 unsigned num_elements) {
  SetFrom<const uint32, uint32, ConvertUInt32ToUInt32>(
      source, source_stride, this, destination_start_index, num_elements);
}

void UInt32Field::SetFromUByteNs(const uint8* source,
                                 unsigned source_stride,
                                 unsigned destination_start_index,
                                 unsigned num_elements) {
  SetFrom<const uint8, uint32, ConvertUByteNToUInt32>(
      source, source_stride, this, destination_start_index, num_elements);
}

bool UInt32Field::SetFromMemoryStream(MemoryReadStream* stream) {
  if (!buffer()) {
    O3D_ERROR(service_locator())
        << "The buffer for field '" << name() << "' no longer exists";
    return false;
  }

  size_t num_elements = buffer()->num_elements();

  // sanity check that the stream has enough data
  if (stream->GetRemainingByteCount() < num_elements * size()) {
    return false;
  }

  const uint32 *source = stream->GetDirectMemoryPointerAs<const uint32>();

  stream->Skip(num_elements * size());

  SetFrom<const uint32, uint32, ConvertLittleEndianUInt32ToUInt32>(
      source, num_components(), this, 0, num_elements);

  return true;
}

void UInt32Field::GetAsFloats(unsigned source_start_index,
                              float* destination,
                              unsigned destination_stride,
                              unsigned num_elements) const {
  GetAs<const uint32, float, ConvertUInt32ToFloat>(
      this,
      source_start_index,
      destination,
      destination_stride,
      num_elements);
}

void UInt32Field::GetAsUInt32s(unsigned source_start_index,
                               uint32* destination,
                               unsigned destination_stride,
                               unsigned num_elements) const {
  GetAs<const uint32, uint32, ConvertUInt32ToUInt32>(
      this,
      source_start_index,
      destination,
      destination_stride,
      num_elements);
}

void UInt32Field::ConcreteCopy(const Field& source) {
  DCHECK(source.IsA(GetClass()));
  DCHECK(source.buffer());
  unsigned num_components = source.num_components();
  unsigned num_elements = source.buffer()->num_elements();
  scoped_array<uint32> temp(new uint32[num_components * num_elements]);
  down_cast<const UInt32Field*>(&source)->GetAsUInt32s(
      0, temp.get(), num_components, num_elements);
  SetFromUInt32s(temp.get(), num_components, 0, num_elements);
}

Field::Ref UInt32Field::Create(ServiceLocator* service_locator,
                               Buffer* buffer,
                               unsigned num_components,
                               unsigned offset) {
  return Field::Ref(
      new UInt32Field(service_locator, buffer, num_components, offset));
}

// UByteNField -------------------

UByteNField::UByteNField(ServiceLocator* service_locator,
                         Buffer* buffer,
                         unsigned num_components,
                         unsigned offset)
    : Field(service_locator, buffer, num_components, offset) {
  Renderer* renderer = service_locator->GetService<Renderer>();
  DCHECK(renderer);
  DCHECK(num_components % 4 == 0);

  swizzle_table_ = renderer->GetRGBAUByteNSwizzleTable();
}

size_t UByteNField::GetFieldComponentSize() const {
  return sizeof(uint8);  // NOLINT
}

void UByteNField::SetFromFloats(const float* source,
                                unsigned source_stride,
                                unsigned destination_start_index,
                                unsigned num_elements) {
  SetFromWithSwizzle<const float, uint8, ConvertFloatToUByteN>(
      source, source_stride, this, destination_start_index, num_elements,
      swizzle_table_);
}

void UByteNField::SetFromUInt32s(const uint32* source,
                                 unsigned source_stride,
                                 unsigned destination_start_index,
                                 unsigned num_elements) {
  SetFromWithSwizzle<const uint32, uint8, ConvertUInt32ToUByteN>(
      source, source_stride, this, destination_start_index, num_elements,
      swizzle_table_);
}

void UByteNField::SetFromUByteNs(const uint8* source,
                                 unsigned source_stride,
                                 unsigned destination_start_index,
                                 unsigned num_elements) {
  SetFromWithSwizzle<const uint8, uint8, ConvertUByteNToUByteN>(
      source, source_stride, this, destination_start_index, num_elements,
      swizzle_table_);
}

bool UByteNField::SetFromMemoryStream(MemoryReadStream* stream) {
  if (!buffer()) {
    O3D_ERROR(service_locator())
        << "The buffer for field '" << name() << "' no longer exists";
    return false;
  }

  size_t num_elements = buffer()->num_elements();

  // sanity check that the stream has enough data
  if (stream->GetRemainingByteCount() < num_elements * size()) {
    return false;
  }

  const uint8 *source = stream->GetDirectMemoryPointerAs<const uint8>();
  stream->Skip(num_elements * size());

  SetFromWithSwizzle<const uint8, uint8, ConvertUByteNToUByteN>(
      source, num_components(), this, 0, num_elements, swizzle_table_);

  return true;
}

void UByteNField::GetAsFloats(unsigned source_start_index,
                              float* destination,
                              unsigned destination_stride,
                              unsigned num_elements) const {
  GetAsWithSwizzle<const uint8, float, ConvertUByteNToFloat>(
      this,
      source_start_index,
      destination,
      destination_stride,
      num_elements,
      swizzle_table_);
}

void UByteNField::GetAsUByteNs(unsigned source_start_index,
                               uint8* destination,
                               unsigned destination_stride,
                               unsigned num_elements) const {
  GetAsWithSwizzle<const uint8, uint8, ConvertUByteNToUByteN>(
      this,
      source_start_index,
      destination,
      destination_stride,
      num_elements,
      swizzle_table_);
}

void UByteNField::ConcreteCopy(const Field& source) {
  DCHECK(source.IsA(GetClass()));
  DCHECK(source.buffer());
  unsigned num_components = source.num_components();
  unsigned num_elements = source.buffer()->num_elements();
  scoped_array<uint8> temp(new uint8[num_components * num_elements]);
  down_cast<const UByteNField*>(&source)->GetAsUByteNs(
      0, temp.get(), num_components, num_elements);
  SetFromUByteNs(temp.get(), num_components, 0, num_elements);
}


Field::Ref UByteNField::Create(ServiceLocator* service_locator,
                               Buffer* buffer,
                               unsigned num_components,
                               unsigned offset) {
  return Field::Ref(
      new UByteNField(service_locator, buffer, num_components, offset));
}

}  // namespace o3d
