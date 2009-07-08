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


// This file contains the declaration for the Buffer, VertexBuffer and
// IndexBuffer classes.

#ifndef O3D_CORE_CROSS_BUFFER_H_
#define O3D_CORE_CROSS_BUFFER_H_

#include <vector>
#include "core/cross/field.h"
#include "core/cross/named_object.h"
#include "core/cross/types.h"

namespace o3d {

class RawData;
class Features;

// class Buffer -----------------------------
//
// DESIGN GOALS:the Buffer object is a low level container for a flat list of
// floating point or integer values. These are used to define geometry,
// parameter buffers and to hold animation data.
//
// The default implementation of the Buffer doesn't do much.  Each render system
// (e.g. D3D, OGL, etc) should derive its own version of a Buffer that handles
// the underlying data resources appropriately

// The Buffer object is a low level container for a flat list of
// floating point or integer values. These are currently used to define
// geometry.  Buffer is an abstract class and only declares the interface for
// buffer operations.  Data storage needs to be allocated by derived classes.
class Buffer : public NamedObject {
 public:
  typedef SmartPointer<Buffer> Ref;

  // Defines how you want to access a buffer when locking.
  enum AccessMode {
    NONE = 0,
    READ_ONLY = 1,
    WRITE_ONLY = 2,
    READ_WRITE = 3,
  };

  // Yes, 65534 is the correct number. Specifically the Intel 945 only allows
  // 65534 elements.
  static const unsigned MAX_SMALL_INDEX = 65534;

  // Yes, Sadly many modern cards only support 1048575 vertices.
  static const unsigned MAX_LARGE_INDEX = 1048575;

  // A four-character identifier used in the binary serialization format
  // (not exposed to Javascript)
  static const char *kSerializationID;

  explicit Buffer(ServiceLocator* service_locator);
  ~Buffer();

  // Allocates memory for the data to be stored in the buffer
  bool AllocateElements(unsigned int num_elements);

  // Frees any data currently allocated for this buffer.
  void Free();

  // Creates a field on this buffer
  // Parameters:
  //   field_type: type of field.
  //   num_components: number of components.
  // Returns:
  //   Pointer to created field.
  Field* CreateField(const ObjectBase::Class* field_type,
                     unsigned num_components);

  // Creates a field on this buffer
  //
  // this is for Javascript
  //
  // Paramters:
  //   field_type_name: name of type of field.
  //   num_components: number of components.
  // Returns:
  //   Pointer to created field.
  Field* CreateFieldByClassName(const String& field_type_name,
                                unsigned num_components);

  // Removes a field
  // Parameters:
  //   field: Field to remove.
  void RemoveField(Field* field);

  // Returns the stride of the buffer.
  unsigned int stride() const {
    return stride_;
  }

  // Returns the number of components per element. In other words the sum
  // of all the components in all fields.
  unsigned int total_components() const {
    return total_components_;
  }

  // Returns the field change count. Anytime a field is added, removed or
  // changed this value is incremented. Streams track it so they know
  // to rebuild vertex declarations based on the buffer changing format.
  unsigned int field_change_count() const {
    return field_change_count_;
  }

  // Obtains a pointer to the memory location where the data is stored.
  // This method should get called before data stored in the buffer can be
  // modified. From C++ use LockAs
  // Parameters:
  //   access_mode: How you want to access the data.
  //   buffer_data: pointer to void pointer to receive pointer to data.
  // Returns:
  //   true if the operation succeeds.
  bool Lock(AccessMode access_mode, void** buffer_data);

  // Notifies that updates to the buffer data are completed.  Once Unlock
  // is called data should not be modified any more.
  bool Unlock();

  // Gets the number of elements.
  unsigned int num_elements() const {
    return num_elements_;
  }

  // Gets the array of fields.
  const FieldRefArray& fields() const {
    return fields_;
  }

  // Returns the size of the buffer in bytes.
  size_t GetSizeInBytes() const {
    return num_elements_ * stride_;
  }

  // A typed version of Lock
  template <typename T>
  bool LockAs(AccessMode access_mode, T** buffer_data) {
    return Lock(access_mode, reinterpret_cast<void**>(buffer_data));
  }

  // De-serializes the data contained in |raw_data|
  // The entire contents of |raw_data| from start to finish will
  // be used
  bool Set(o3d::RawData *raw_data);

  // De-serializes the data contained in |raw_data|
  // starting at byte offset |offset| and using |length|
  // bytes
  bool Set(o3d::RawData *raw_data,
           size_t offset,
           size_t length);

 protected:
  // The concrete version of AllocateElements.
  virtual bool ConcreteAllocate(size_t size_in_bytes) = 0;

  // The concrete version of Free
  virtual void ConcreteFree() = 0;

  // The concrete version of Lock. Platform specific versions of buffers
  // need to override this.
  //
  // Parameters:
  //   access_mode: How you want to access the data. buffer_data: pointer to
  //   void pointer to receive pointer to data.
  // Returns:
  //   true if the operation succeeds.
  virtual bool ConcreteLock(AccessMode access_mode, void** buffer_data) = 0;

  // The concrete version of Unlock. Platform specific versions of buffers need
  // to override this.
  virtual bool ConcreteUnlock() = 0;

 private:
  // Takes the data currently allocated and copies it to new data of a different
  // stride.
  // Parameters:
  //   new_stride: stride of new buffer.
  //   field_to_remove: address of field. If NULL no field is removed.
  bool ReshuffleBuffer(unsigned int new_stride, Field* field_to_remove);

  Features* features_;

  // Fields.
  FieldRefArray fields_;

  // The number of times fields have been added or removed. Streams
  // can track this value so they can know if they need to update.
  unsigned int field_change_count_;

  // The total number of components in all fields.
  unsigned int total_components_;

  // The stride of the buffer.
  unsigned int stride_;

  // The current number of elements in the buffer.
  unsigned int num_elements_;

  // The mode the buffer is currently being accessed so we can fail if a
  // different mode is requested
  AccessMode access_mode_;

  // The number of times this buffer has been locked.
  int lock_count_;

  // Pointer to data when it's locked.
  void* locked_data_;

  O3D_DECL_CLASS(Buffer, NamedObject);
};

// VertexBufferBase is just here so VertexBuffer and SourceBuffer can share IDL
// glue.
class VertexBufferBase : public Buffer {
 public:
  typedef SmartPointer<VertexBufferBase> Ref;

 protected:
  explicit VertexBufferBase(ServiceLocator* service_locator);

 private:
  O3D_DECL_CLASS(VertexBufferBase, Buffer);
  DISALLOW_COPY_AND_ASSIGN(VertexBufferBase);
};

// VertexBuffer is Buffer object used for storing vertex data for geometry
// (e.g. vertex positions, normals, colors, etc).  It is an abstract class
// declaring the interface only.  Each rendering platform should derive its own
// implementation of the interface.
//
// NOTE: You can not READ data from a VertexBuffer.
class VertexBuffer : public VertexBufferBase {
 public:
  typedef SmartPointer<VertexBuffer> Ref;

 protected:
  explicit VertexBuffer(ServiceLocator* service_locator);

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(VertexBuffer, VertexBufferBase);
  DISALLOW_COPY_AND_ASSIGN(VertexBuffer);
};

// SourceBuffer is a buffer object stored in system memory. It is used as
// the source for skinning, morph targets, etc.
class SourceBuffer : public VertexBufferBase {
 public:
  typedef SmartPointer<SourceBuffer> Ref;

  ~SourceBuffer();

 protected:
  // Overridden from Buffer.
  virtual bool ConcreteAllocate(size_t size_in_bytes);

  // Overridden from Buffer.
  virtual bool ConcreteLock(AccessMode access_mode, void **buffer_data);

  // Overridden from Buffer.
  virtual bool ConcreteUnlock();

  explicit SourceBuffer(ServiceLocator* service_locator);

 protected:
  // Frees the buffer if it exists.
  void ConcreteFree();

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  scoped_array<char> buffer_;  // The actual data for this buffer.

  O3D_DECL_CLASS(SourceBuffer, VertexBufferBase);
  DISALLOW_COPY_AND_ASSIGN(SourceBuffer);
};

// IndexBuffer is a buffer object used for storing geometry index data (e.g.
// triangle indices).  It is an abstract class declaring the interface only.
// Each rendering platform should derive its own implementation of the
// interface.
//
// NOTE: You can not READ data from an IndexBuffer.
class IndexBuffer : public Buffer {
 public:
  typedef SmartPointer<IndexBuffer> Ref;

  Field* index_field() const;

  // De-serializes the data contained in |raw_data|
  // The entire contents of |raw_data| from start to finish will
  // be used
  bool Set(o3d::RawData *raw_data) {
    return Buffer::Set(raw_data);
  }

  // De-serializes the data contained in |raw_data|
  // starting at byte offset |offset| and using |length|
  // bytes
  bool Set(o3d::RawData *raw_data,
           size_t offset,
           size_t length) {
    return Buffer::Set(raw_data, offset, length);
  }

 protected:
  explicit IndexBuffer(ServiceLocator* service_locator);

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(IndexBuffer, Buffer);
  DISALLOW_COPY_AND_ASSIGN(IndexBuffer);
};

// BufferLockHelper can be used to lock a buffer in a safe way in that it will
// unlock the buffer on destruction so you can use it like this
//
// {
//   BufferLockHelper helper(my_buffer);
//   void* data = helper.GetData();
//   if (data) {
//     .. do something with data here ..
//   }
// }
//
// Because there is no need to call Unlock it is much easier to deal with error
// conditions.
class BufferLockHelper {
 public:
  explicit BufferLockHelper(Buffer* buffer);

  ~BufferLockHelper();

  // Gets a pointer to the data of the buffer, locking the buffer if necessary.
  // Returns:
  //   Pointer to data in buffer or NULL if there was an error.
  void* GetData(Buffer::AccessMode access_mode);

  // Typed version of GetData
  template <typename T>
  T* GetDataAs(Buffer::AccessMode access_mode) {
    return reinterpret_cast<T*>(GetData(access_mode));
  }

 private:
  Buffer* buffer_;
  void* data_;
  bool locked_;

  DISALLOW_COPY_AND_ASSIGN(BufferLockHelper);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_BUFFER_H_
