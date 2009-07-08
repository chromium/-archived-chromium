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


// This file contains the definitions of Buffer, VertexBuffer and IndexBuffer.

#include "core/cross/precompile.h"
#include "core/cross/buffer.h"
#include "core/cross/renderer.h"
#include "core/cross/features.h"
#include "core/cross/error.h"
#include "import/cross/memory_stream.h"
#include "import/cross/raw_data.h"

namespace o3d {

O3D_DEFN_CLASS(Buffer, NamedObject);
O3D_DEFN_CLASS(VertexBufferBase, Buffer);
O3D_DEFN_CLASS(VertexBuffer, VertexBufferBase);
O3D_DEFN_CLASS(SourceBuffer, VertexBufferBase);
O3D_DEFN_CLASS(IndexBuffer, Buffer);

const char *Buffer::kSerializationID = "BUFF";

namespace {

// Copies a field.
// Parameters:
//   source: address of source data.
//   source_stride: amount to step for each element in the source.
//   field_size: size of field in bytes.
//   num_elements: number of elements to copy.
//   destination: address of destination data.
//   destination_stride: amount to step for each element in the destination.
void CopyField(const void* source,
               size_t source_stride,
               size_t field_size,
               size_t num_elements,
               void* destination,
               size_t destination_stride) {
  while (num_elements) {
    memcpy(destination, source, field_size);
    source = AddPointerOffset(source, source_stride);
    destination = AddPointerOffset(destination, destination_stride);
    --num_elements;
  }
}

typedef Field::Ref (*FieldCreatorFunc)(ServiceLocator* service_locator,
                                       Buffer* buffer,
                                       unsigned num_components,
                                       unsigned offset);

struct FieldCreator {
  const ObjectBase::Class* field_type;
  FieldCreatorFunc create_function;
  unsigned required_component_multiple;
};
static FieldCreator g_creators[] = {
  { FloatField::GetApparentClass(), FloatField::Create,
    FloatField::kRequiredComponentMultiple, },
  { UInt32Field::GetApparentClass(), UInt32Field::Create,
    UInt32Field::kRequiredComponentMultiple, },
  { UByteNField::GetApparentClass(), UByteNField::Create,
    UByteNField::kRequiredComponentMultiple, },
};

}  // anonymouse namespace.

// Buffer ---------------
Buffer::Buffer(ServiceLocator* service_locator)
    : NamedObject(service_locator),
      features_(service_locator->GetService<Features>()),
      access_mode_(NONE),
      field_change_count_(0),
      total_components_(0),
      stride_(0),
      num_elements_(0),
      lock_count_(0) {
}

Buffer::~Buffer() {
  for (unsigned ii = 0; ii < fields_.size(); ++ii) {
    if (!fields_[ii].IsNull()) {
      fields_[ii]->ClearBuffer();
    }
  }
}

bool Buffer::AllocateElements(unsigned num_elements) {
  if (access_mode_ != NONE) {
    O3D_ERROR(service_locator()) << "Attempt to allocate locked Buffer '"
                                 << name() << "'";
    return false;
  }

  if (stride_ == 0) {
    O3D_ERROR(service_locator())
        << "No fields have been set on Buffer '" << name() << "'";
    return false;
  }

  if (num_elements > MAX_SMALL_INDEX && !features_->large_geometry()) {
    O3D_ERROR(service_locator())
        << "You can not allocate more then " << MAX_SMALL_INDEX
        << " elements in a buffer unless "
        << "you request support for large geometry when you "
        << "initialize O3D.";
    return false;
  }

  if (num_elements > MAX_LARGE_INDEX) {
    O3D_ERROR(service_locator())
        << "The maximum number of elements in a buffer is " << MAX_LARGE_INDEX
        << ".";
    return false;
  }

  size_t size_in_bytes = num_elements * stride_;

  if (size_in_bytes == 0) {
    O3D_ERROR(service_locator())
        << "Attempt to allocate zero bytes for Buffer '" << name() << "'";
    return false;
  }

  if (!ConcreteAllocate(size_in_bytes)) {
    num_elements_ = 0;
    return false;
  }

  num_elements_ = num_elements;
  return true;
}

void Buffer::Free() {
  if (num_elements_ > 0) {
    ConcreteFree();
    num_elements_ = 0;
  }
}

bool Buffer::ReshuffleBuffer(unsigned int new_stride, Field* field_to_remove) {
  if (new_stride == 0) {
    ConcreteFree();
    return true;
  }
  if (num_elements_) {
    size_t size_in_bytes = num_elements_ * new_stride;
    std::vector<uint8> temp(size_in_bytes);

    // Copy old fields into new buffer.
    {
      BufferLockHelper helper(this);
      void* source = helper.GetData(Buffer::READ_ONLY);
      if (!source) {
        return false;
      }
      unsigned int offset = 0;
      for (unsigned ii = 0; ii < fields_.size(); ++ii) {
        Field* field = fields_[ii].Get();
        if (field != field_to_remove) {
          CopyField(PointerFromVoidPointer<void*>(source, field->offset()),
                    stride_,
                    field->size(),
                    num_elements_,
                    PointerFromVoidPointer<void*>(&temp[0], offset),
                    new_stride);
          field->set_offset(offset);
          offset += field->size();
        }
      }
    }
    // Copy the reorganized data into a new buffer.
    {
      ConcreteFree();
      if (!ConcreteAllocate(size_in_bytes)) {
        num_elements_ = 0;
        O3D_ERROR(service_locator())
            << "Couldn't allocate buffer of size: " << size_in_bytes
            << " for Buffer '" << name() << "'";
        return false;
      }
      // stride_ must be set before GetData is called so that the proper size
      // buffer is allocated. We also need to set it after this function is
      // is completed (see CreateField, RemoveField) for when we create a new
      // buffer with no fields yet.
      stride_ = new_stride;
      BufferLockHelper helper(this);
      void* destination = helper.GetData(Buffer::WRITE_ONLY);
      if (!destination) {
        return false;
      }
      memcpy(destination, &temp[0], size_in_bytes);
    }
  }
  return true;
}

Field* Buffer::CreateFieldByClassName(const String& field_type,
                                      unsigned num_components) {
  for (unsigned ii = 0; ii < arraysize(g_creators); ++ii) {
    if (!field_type.compare(g_creators[ii].field_type->name()) ||
        !field_type.compare(g_creators[ii].field_type->unqualified_name())) {
      return CreateField(g_creators[ii].field_type, num_components);
    }
  }

  O3D_ERROR(service_locator())
      << "unrecognized field type '" << field_type << "'";
  return NULL;
}

Field* Buffer::CreateField(const ObjectBase::Class* field_type,
                           unsigned num_components) {
  FieldCreator* creator = NULL;
  for (unsigned ii = 0; ii < arraysize(g_creators); ++ii) {
    if (g_creators[ii].field_type == field_type) {
      creator = &g_creators[ii];
      break;
    }
  }

  if (!creator) {
    O3D_ERROR(service_locator())
        << "unrecognized field type '"
        << (field_type ? field_type->name() :  "NULL") << "'";
    return NULL;
  }

  if (num_components == 0) {
    O3D_ERROR(service_locator())
        << "num components must be > 0 for Buffer '" << name() << "'";
    return NULL;
  }

  if (num_components % creator->required_component_multiple != 0) {
    O3D_ERROR(service_locator())
        << "num components must be a multiple of "
        << creator->required_component_multiple
        << " for fields of type " << field_type->unqualified_name();
    return NULL;
  }

  Field::Ref field = creator->create_function(service_locator(), this,
                                              num_components, stride_);
  unsigned int new_stride = stride_ + field->size();
  ReshuffleBuffer(new_stride, NULL);

  fields_.push_back(field);
  stride_ = new_stride;
  total_components_ += num_components;
  ++field_change_count_;

  return field;
}

void Buffer::RemoveField(Field* field) {
  for (unsigned ii = 0; ii < fields_.size(); ++ii) {
    if (fields_[ii] == field) {
      unsigned int new_stride = stride_ - field->size();
      ReshuffleBuffer(new_stride, field);
      total_components_ -= field->num_components();
      stride_ = new_stride;
      field->ClearBuffer();
      // This erase may remove the last reference to the field so field may
      // be invalid after this line.
      fields_.erase(fields_.begin() + ii);
      ++field_change_count_;
      return;
    }
  }
  O3D_ERROR(service_locator())
      << "Field '" << field->name()
      << "' does not exist on Buffer '" << name() << "'";
}

bool Buffer::Lock(AccessMode access_mode, void** buffer_data) {
  if (access_mode == NONE) {
    O3D_ERROR(service_locator())
        << "attempt to lock Buffer '" << name()
        << "' with access mode NONE";
    return false;
  }
  if (access_mode_ == NONE || access_mode == access_mode_) {
    if (lock_count_ == 0) {
      if (!ConcreteLock(access_mode, &locked_data_)) {
        return false;
      }
    }
    ++lock_count_;
    *buffer_data = locked_data_;
    return true;
  } else {
    O3D_ERROR(service_locator())
        << "attempt to lock already locked Buffer '" << name()
        << "' with different access mode";
    return false;
  }
}

bool Buffer::Unlock() {
  if (lock_count_ == 0) {
    O3D_ERROR(service_locator())
        << "attempt to unlock unlocked Buffer '" << name() << "'";
    return false;
  }
  --lock_count_;
  if (lock_count_ == 0) {
    return ConcreteUnlock();
  }
  return true;
}

bool Buffer::Set(o3d::RawData *raw_data) {
  DCHECK(raw_data);
  return Set(raw_data, 0, raw_data->GetLength());
}

bool Buffer::Set(o3d::RawData *raw_data,
                 size_t offset,
                 size_t length) {
  DCHECK(raw_data);

  if (!raw_data->IsOffsetLengthValid(offset, length)) {
    O3D_ERROR(service_locator()) << "illegal buffer data offset or size";
    return false;
  }

  // GetData() returns NULL if it, for example, cannot open the temporary data
  // file. In that case, it invokes the error callback. We just have to be
  // careful not to dereference it.
  const uint8 *data = raw_data->GetDataAs<uint8>(offset);
  if (!data) {
    return false;
  }

  MemoryReadStream stream(data, length);

  // Verify we at least have enough data for four-char kSerializationID plus
  // version and num_fields
  if (length < 4 + 2*sizeof(int32)) {
    O3D_ERROR(service_locator())
        << "data object does not contain buffer data";
    return false;
  }

  // To insure data integrity we expect four characters kSerializationID
  char id[5];
  stream.Read(id, 4);
  id[4] = 0;  // null-terminate

  if (strcmp(id, kSerializationID)) {
    O3D_ERROR(service_locator())
        << "data object does not contain buffer data";
    return false;
  }

  int32 version = stream.ReadLittleEndianInt32();
  if (version != 1) {
    O3D_ERROR(service_locator()) << "unknown buffer data version";
    return false;
  }

  // Delete existing fields.
  // TODO: Since this removes all fields, there is no need to
  // reshuffle (which can be expensive). We could provide a RemoveAllFields
  // and call it here.
  while (fields_.size() > 0) {
    RemoveField(fields_[0]);
  }

  // Create fields.
  int32 num_fields = stream.ReadLittleEndianInt32();
  for (int32 ff = 0; ff < num_fields; ++ff) {
    if (stream.GetRemainingByteCount() < 2*sizeof(uint8)) {
      O3D_ERROR(service_locator()) << "unexpected end of buffer data";
      return false;
    }

    uint8 field_id = stream.ReadByte();
    uint8 num_components = stream.ReadByte();

    const ObjectBase::Class *field_type;

    switch (field_id) {
      case Field::FIELDID_FLOAT32:
        field_type = FloatField::GetApparentClass();
        break;
      case Field::FIELDID_UINT32:
        field_type = UInt32Field::GetApparentClass();
        break;
      case Field::FIELDID_BYTE:
        field_type = UByteNField::GetApparentClass();
        break;

      case Field::FIELDID_UNKNOWN:
      default:
        O3D_ERROR(service_locator()) << "unknown field_type";
        return false;
    }

    Field* field = CreateField(field_type, num_components);
    if (!field) {
      O3D_ERROR(service_locator()) << "couldn't create field";
      return false;
    }
  }

  // Read the number of elements and allocate space
  if (stream.GetRemainingByteCount() < sizeof(int32)) {
    O3D_ERROR(service_locator()) << "unexpected end of buffer data";
    return false;
  }
  int32 num_elements = stream.ReadLittleEndianInt32();
  if (!AllocateElements(num_elements)) {
    O3D_ERROR(service_locator()) << "could not allocate buffer elements";
    return false;
  }

  {
    // Lock before reading in all the fields to avoid locking/unlocking
    // for each field which would be slower
    o3d::BufferLockHelper helper(this);
    void *buffer_data = helper.GetData(o3d::Buffer::WRITE_ONLY);

    // Read each field
    for (int32 ff = 0; ff < num_fields; ++ff) {
      Field *field = fields()[ff];
      if (!field->SetFromMemoryStream(&stream)) {
        O3D_ERROR(service_locator()) <<
            "unexpected end of buffer field data";
        return false;
      }
    }
  }

  // Final integrity check that we consumed exactly the correct amount of data
  if (stream.GetRemainingByteCount() != 0) {
    O3D_ERROR(service_locator()) << "extra buffer data remaining";
    return false;
  }

  return true;
}

VertexBufferBase::VertexBufferBase(ServiceLocator* service_locator)
    : Buffer(service_locator) {
}

VertexBuffer::VertexBuffer(ServiceLocator* service_locator)
    : VertexBufferBase(service_locator) {
}

ObjectBase::Ref VertexBuffer::Create(ServiceLocator* service_locator) {
  Renderer* renderer = service_locator->GetService<Renderer>();
  if (NULL == renderer) {
    O3D_ERROR(service_locator) << "No Render Device Available";
    return ObjectBase::Ref();
  }
  return ObjectBase::Ref(renderer->CreateVertexBuffer());
}

SourceBuffer::SourceBuffer(ServiceLocator* service_locator)
    : VertexBufferBase(service_locator),
      buffer_() {
}

SourceBuffer::~SourceBuffer() {
  ConcreteFree();
}

void SourceBuffer::ConcreteFree() {
  buffer_.reset();
}

bool SourceBuffer::ConcreteAllocate(size_t size_in_bytes) {
  ConcreteFree();

  buffer_.reset(new char[size_in_bytes]);

  return true;
}

bool SourceBuffer::ConcreteLock(AccessMode access_mode, void **buffer_data) {
  if (!buffer_.get()) {
    return false;
  }

  *buffer_data = reinterpret_cast<void*>(buffer_.get());
  return true;
}

bool SourceBuffer::ConcreteUnlock() {
  return buffer_.get() != NULL;
}

ObjectBase::Ref SourceBuffer::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new SourceBuffer(service_locator));
}

Field* IndexBuffer::index_field() const {
  // TODO: It does not make sense for an IndexBuffer to have more
  // than one field. For example, we do not support any way of rendering a
  // primitive using only one of the fields of an IndexBuffer. The API allows
  // you to create any number of fields for Buffers in general. That may not be
  // the best design. This code verifies that there are not multiple fields
  // on the IndexBuffer. It also checks for the case where there are no fields
  // to prevent a crash. This function can be called from JavaScript. It
  // should never crash. It is okay for an IndexBuffer to temporarily contain
  // no fields. This is is used by Buffer::Set(). I added a DCHECK for debug
  // builds.
  DCHECK(fields().size() == 1);
  return fields().size() == 1 ? fields()[0].Get() : NULL;
}

IndexBuffer::IndexBuffer(ServiceLocator* service_locator)
    : Buffer(service_locator) {
  CreateField(UInt32Field::GetApparentClass(), 1);
}

ObjectBase::Ref IndexBuffer::Create(ServiceLocator* service_locator) {
  Renderer* renderer = service_locator->GetService<Renderer>();
  if (NULL == renderer) {
    O3D_ERROR(service_locator) << "No Render Device Available";
    return ObjectBase::Ref();
  }
  return ObjectBase::Ref(renderer->CreateIndexBuffer());
}

BufferLockHelper::BufferLockHelper(Buffer* buffer)
    : buffer_(buffer),
      data_(NULL),
      locked_(false) {
}

BufferLockHelper::~BufferLockHelper() {
  if (locked_) {
    buffer_->Unlock();
  }
}

void* BufferLockHelper::GetData(Buffer::AccessMode access_mode) {
  if (!locked_) {
    locked_ = buffer_->Lock(access_mode, &data_);
    if (!locked_) {
      O3D_ERROR(buffer_->service_locator())
          << "Unable to lock buffer '" << buffer_->name() << "'";
    }
  }
  return data_;
}

}  // namespace o3d
