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


// This file contains the declaration for the Field, FloatField, UInt32Field and
// UByteNField classes.

#ifndef O3D_CORE_CROSS_FIELD_H_
#define O3D_CORE_CROSS_FIELD_H_

#include <vector>
#include "core/cross/named_object.h"
#include "core/cross/types.h"

namespace o3d {

// Adds an arbitrary byte offset to a typed pointer.
template <typename T>
T AddPointerOffset(T pointer, unsigned offset) {
  return reinterpret_cast<T>(
      const_cast<uint8*>(reinterpret_cast<const uint8*>(pointer) + offset));
}

// Creates a typed pointer from a void pointer and an offset.
template <typename T>
T PointerFromVoidPointer(void* pointer, unsigned offset) {
  return reinterpret_cast<T>(
      const_cast<uint8*>(reinterpret_cast<const uint8*>(pointer) + offset));
}

class Buffer;
class MemoryReadStream;

// A Field is an abstract base class that manages a set of components in a
// buffer of a specific type. Fields are managed by Buffers and can not be
// directly created. When a Buffer is destroyed or if a Field is removed from a
// Buffer the Field's buffer pointer will be set to NULL.
class Field : public NamedObject {
 public:
  typedef SmartPointer<Field> Ref;

  // These IDs are used for serialization and are not exposed to
  // Javascript.
  enum FieldID {
    FIELDID_UNKNOWN = 0,
    FIELDID_FLOAT32 = 1,
    FIELDID_UINT32  = 2,
    FIELDID_BYTE    = 3
  };

  Field(ServiceLocator* service_locator,
        Buffer* buffer,
        unsigned num_components,
        unsigned offset);

  // The number of components in this field.
  unsigned num_components() const {
    return num_components_;
  }

  // The offset for this field.
  unsigned offset() const {
    return offset_;
  }

  // The size of this field in bytes.
  size_t size() const {
    return num_components_ * GetFieldComponentSize();
  }

  // The buffer this field belongs to. Can be NULL if the buffer has been
  // deleted.
  Buffer* buffer() const {
    return buffer_;
  }

  // Returns the size of a field type in bytes.
  virtual size_t GetFieldComponentSize() const = 0;

  // Sets this field from source floats
  //
  // This function copies elements from the source array to the field.
  // It assumes that there are a multiple of N components in the source where N
  // is the number of components in the field. In other words, if the field has
  // 3 components then passing a num_elements of 2 would copy 2 elements, each 3
  // components.
  //
  // Parameters:
  //   source: pointer to first element in array.
  //   source_stride: stride between elements in source where an element
  //       equals the number of components this field uses. This is in source
  //       units not in bytes.
  //   destination_start_index: element in the destination to start.
  //   num_elements: The number of elements to copy.
  virtual void SetFromFloats(const float* source,
                             unsigned source_stride,
                             unsigned destination_start_index,
                             unsigned num_elements) = 0;

  // This function is the same as SetFromFloats except takes UInt32s as input.
  virtual void SetFromUInt32s(const uint32* source,
                              unsigned source_stride,
                              unsigned destination_start_index,
                              unsigned num_elements) = 0;

  // This function is the same as SetFromFloats except takes UByteNs as input.
  virtual void SetFromUByteNs(const uint8* source,
                              unsigned source_stride,
                              unsigned destination_start_index,
                              unsigned num_elements) = 0;

  // Sets all the elements for this field given the memory stream
  virtual bool SetFromMemoryStream(MemoryReadStream* stream) = 0;

  // Gets this field as floats.
  //
  // This function copies elements from the the field to the destination array.
  // It assumes that there are a multiple of N components in the destination
  // where N is the number of components in the field. In other words, if the
  // field has 3 components then passing a num_elements of 2 would copy 2
  // elements, each 3 components.
  //
  // Parameters:
  //   source_start_index: element in the source to start.
  //   destination: pointer to first element in destination array.
  //   destination_stride: stride between elements in the destination in
  //       destination units.
  //   num_elements: The number of elements to copy.
  virtual void GetAsFloats(unsigned source_start_index,
                           float* destination,
                           unsigned destination_stride,
                           unsigned num_elements) const = 0;

  // Checks if the start_index and num_elements would reference something
  // outside the buffer associated with this field.
  bool RangeValid(unsigned int start_index, unsigned int num_elements);

  // Copies a field. The field must be of the same type.
  // Paremeters:
  //   source: field to copy from.
  void Copy(const Field& source);

 protected:
  // The concrete version of Copy. Copy calls this function to do the actual
  // copying after it has verified the types are compatible and the buffers
  // exist. ConcreteCopy does NOT have to check for those errors.
  // Paremeters:
  //   source: field to copy from.
  virtual void ConcreteCopy(const Field& source) = 0;

 private:
  friend class Buffer;
  void set_offset(unsigned offset) {
    offset_ = offset;
  }

  void ClearBuffer() {
    buffer_ = NULL;
  }

  Buffer* buffer_;
  unsigned num_components_;
  unsigned offset_;

  O3D_DECL_CLASS(Field, NamedObject);
  DISALLOW_COPY_AND_ASSIGN(Field);
};

typedef std::vector<Field*> FieldArray;
typedef std::vector<Field::Ref> FieldRefArray;

// A field the hold floats.
class FloatField : public Field {
 public:
  // When requesting a field of this type the number of componets must be a
  // multiple of this.
  static const unsigned kRequiredComponentMultiple = 1;

  // Overridden from Field.
  virtual size_t GetFieldComponentSize() const;

  static Field::Ref Create(ServiceLocator* service_locator,
                           Buffer* buffer,
                           unsigned num_components,
                           unsigned offset);

  // Overridden from Field.
  virtual void SetFromFloats(const float* source,
                             unsigned source_stride,
                             unsigned destination_start_index,
                             unsigned num_elements);

  // Overridden from Field.
  virtual void SetFromUInt32s(const uint32* source,
                              unsigned source_stride,
                              unsigned destination_start_index,
                              unsigned num_elements);

  // Overridden from Field.
  virtual void SetFromUByteNs(const uint8* source,
                              unsigned source_stride,
                              unsigned destination_start_index,
                              unsigned num_elements);

  // Overridden from Field.
  virtual bool SetFromMemoryStream(MemoryReadStream* stream);

  // Overridden from Field.
  virtual void GetAsFloats(unsigned source_start_index,
                           float* destination,
                           unsigned destination_stride,
                           unsigned num_elements) const;

 protected:
  // Overridden from Field.
  virtual void ConcreteCopy(const Field& source);

 private:
  FloatField(ServiceLocator* service_locator,
             Buffer* buffer,
             unsigned num_components,
             unsigned offset);

  O3D_DECL_CLASS(FloatField, Field);
  DISALLOW_COPY_AND_ASSIGN(FloatField);
};

// A field the hold uint32s.
class UInt32Field : public Field {
 public:
  // When requesting a field of this type the number of componets must be a
  // multiple of this.
  static const unsigned kRequiredComponentMultiple = 1;

  // Overridden from Field.
  virtual size_t GetFieldComponentSize() const;

  static Field::Ref Create(ServiceLocator* service_locator,
                           Buffer* buffer,
                           unsigned num_components,
                           unsigned offset);

  // Overridden from Field.
  virtual void SetFromFloats(const float* source,
                             unsigned source_stride,
                             unsigned destination_start_index,
                             unsigned num_elements);

  // Overridden from Field.
  virtual void SetFromUInt32s(const uint32* source,
                              unsigned source_stride,
                              unsigned destination_start_index,
                              unsigned num_elements);

  // Overridden from Field.
  virtual void SetFromUByteNs(const uint8* source,
                              unsigned source_stride,
                              unsigned destination_start_index,
                              unsigned num_elements);

  // Overridden from Field.
  virtual bool SetFromMemoryStream(MemoryReadStream* stream);

  // Overridden from Field.
  virtual void GetAsFloats(unsigned source_start_index,
                           float* destination,
                           unsigned destination_stride,
                           unsigned num_elements) const;

  // Gets this field as uint32s.
  //
  // This function copies elements from the the field to the destination array.
  // It assumes that there are a multiple of N components in the destination
  // where N is the number of components in the field. In other words, if the
  // field has 3 components then passing a num_elements of 2 would copy 2
  // elements, each 3 components.
  //
  // Parameters:
  //   source_start_index: element in the source to start.
  //   destination: pointer to first element in destination array.
  //   destination_stride: stride between elements in the destination in
  //       destination units.
  //   num_elements: The number of elements to copy.
  void GetAsUInt32s(unsigned source_start_index,
                    uint32* destination,
                    unsigned destination_stride,
                    unsigned num_elements) const;

 protected:
  // Overridden from Field.
  virtual void ConcreteCopy(const Field& source);

 private:
  UInt32Field(ServiceLocator* service_locator,
              Buffer* buffer,
              unsigned num_components,
              unsigned offset);

  O3D_DECL_CLASS(UInt32Field, Field);
  DISALLOW_COPY_AND_ASSIGN(UInt32Field);
};

// A field the hold UByteNs where a UByteN is an uint8 that represents a value
// from 0.0 to 1.0.
class UByteNField : public Field {
 public:
  // When requesting a field of this type the number of componets must be a
  // multiple of this.
  static const unsigned kRequiredComponentMultiple = 4;

  // Overridden from Field.
  virtual size_t GetFieldComponentSize() const;

  static Field::Ref Create(ServiceLocator* service_locator,
                           Buffer* buffer,
                           unsigned num_components,
                           unsigned offset);

  // Overridden from Field.
  virtual void SetFromFloats(const float* source,
                             unsigned source_stride,
                             unsigned destination_start_index,
                             unsigned num_elements);

  // Overridden from Field.
  virtual void SetFromUInt32s(const uint32* source,
                              unsigned source_stride,
                              unsigned destination_start_index,
                              unsigned num_elements);

  // Overridden from Field.
  virtual void SetFromUByteNs(const uint8* source,
                              unsigned source_stride,
                              unsigned destination_start_index,
                              unsigned num_elements);

  // Overridden from Field.
  virtual bool SetFromMemoryStream(MemoryReadStream* stream);

  // Overridden from Field.
  virtual void GetAsFloats(unsigned source_start_index,
                           float* destination,
                           unsigned destination_stride,
                           unsigned num_elements) const;

  // Gets this field as ubyten data.
  //
  // This function copies elements from the the field to the destination array.
  // It assumes that there are a multiple of N components in the destination
  // where N is the number of components in the field. In other words, if the
  // field has 3 components then passing a num_elements of 2 would copy 2
  // elements, each 3 components.
  //
  // Parameters:
  //   source_start_index: element in the source to start.
  //   destination: pointer to first element in destination array.
  //   destination_stride: stride between elements in the destination in
  //       destination units.
  //   num_elements: The number of elements to copy.
  void GetAsUByteNs(unsigned source_start_index,
                    uint8* destination,
                    unsigned destination_stride,
                    unsigned num_elements) const;

 protected:
  // Overridden from Field.
  virtual void ConcreteCopy(const Field& source);

 private:
  UByteNField(ServiceLocator* service_locator,
              Buffer* buffer,
              unsigned num_components,
              unsigned offset);

  const int* swizzle_table_;

  O3D_DECL_CLASS(UByteNField, Field);
  DISALLOW_COPY_AND_ASSIGN(UByteNField);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_FIELD_H_
