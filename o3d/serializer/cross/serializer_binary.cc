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


// This file contains the serializer code for binary objects:
// Buffer, Curve, Skin...

#include <vector>

#include "core/cross/buffer.h"
#include "core/cross/curve.h"
#include "core/cross/error.h"
#include "core/cross/skin.h"
#include "import/cross/memory_buffer.h"
#include "import/cross/memory_stream.h"

const size_t kSerializationIDSize = 4;
const size_t kVersionSize = sizeof(int32);

namespace o3d {

// |output| will be filled with the serialized data
void SerializeBuffer(const Buffer &buffer, MemoryBuffer<uint8> *output) {
  // Determine the total size for the serialization data
  const unsigned num_elements = buffer.num_elements();
  const size_t num_fields = buffer.fields().size();
  const size_t kNumFieldsSize = sizeof(int32);
  const size_t kSingleFieldInfoSize = 2 * sizeof(uint8);  // id / num_components
  const size_t all_field_infos_size = num_fields * kSingleFieldInfoSize;
  const size_t kNumElementsSize = sizeof(int32);

  // Add up all the parts comprising the header
  const size_t header_size = kSerializationIDSize +
                             kVersionSize +
                             kNumFieldsSize +
                             all_field_infos_size +
                             kNumElementsSize;

  const size_t data_size = buffer.GetSizeInBytes();
  const size_t total_size = header_size + data_size;
  output->Resize(total_size);

  MemoryWriteStream stream(*output, total_size);

  // write out serialization ID for buffer
  stream.Write(Buffer::kSerializationID, 4);

  // write out version
  stream.WriteLittleEndianInt32(1);

  // write out number of fields
  stream.WriteLittleEndianInt32(static_cast<int32>(num_fields));

  // Write out the specification for the fields
  for (size_t i = 0; i < num_fields; ++i) {
    const Field &field = *buffer.fields()[i];

    // Determine the FIELDID code we need to write out
    // Start out "unknown" until we determine the class
    uint8 field_id = Field::FIELDID_UNKNOWN;

    if (field.IsA(FloatField::GetApparentClass())) {
      field_id = Field::FIELDID_FLOAT32;
    } else if (field.IsA(UInt32Field::GetApparentClass())) {
      field_id = Field::FIELDID_UINT32;
    } else if (field.IsA(UByteNField::GetApparentClass())) {
      field_id = Field::FIELDID_BYTE;
    } else {
      O3D_ERROR(buffer.service_locator()) << "illegal buffer field";
      return;
    }

    stream.WriteByte(field_id);
    stream.WriteByte(field.num_components());
  }

  // Write out the number of elements
  stream.WriteLittleEndianUInt32(num_elements);

  // Make note of stream position at end of header
  size_t data_start_position = stream.GetStreamPosition();

  // Write out the data for each field
  // Write out the specification for the fields
  for (size_t i = 0; i < num_fields; ++i) {
    const Field &field = *buffer.fields()[i];

    MemoryBuffer<uint8> field_data(field.size() * num_elements);
    uint8 *destination = field_data;

    // Figure out what type of field it is, and get the data
    // appropriately
    size_t nitems = num_elements * field.num_components();

    if (field.IsA(FloatField::GetApparentClass())) {
      float *float_destination = reinterpret_cast<float*>(destination);
      field.GetAsFloats(0,
                        float_destination,
                        field.num_components(),
                        num_elements);
      // Write out as little endian float32
      for (size_t i = 0; i < nitems; ++i) {
        stream.WriteLittleEndianFloat32(float_destination[i]);
      }
    } else if (field.IsA(UInt32Field::GetApparentClass())) {
      const UInt32Field &int_field = static_cast<const UInt32Field&>(field);
      uint32 *int_destination = reinterpret_cast<uint32*>(destination);
      int_field.GetAsUInt32s(0,
                             int_destination,
                             field.num_components(),
                             num_elements);
      // Write out as little endian int32
      for (size_t i = 0; i < nitems; ++i) {
        stream.WriteLittleEndianInt32(int_destination[i]);
      }
    } else if (field.IsA(UByteNField::GetApparentClass())) {
      const UByteNField &byte_field = static_cast<const UByteNField&>(field);
      uint8 *byte_destination = reinterpret_cast<uint8*>(destination);
      byte_field.GetAsUByteNs(0,
                              byte_destination,
                              field.num_components(),
                              num_elements);
      // Write out the bytes
      stream.Write(byte_destination, nitems);
    }
  }

  if (stream.GetStreamPosition() != total_size) {
    O3D_ERROR(buffer.service_locator()) << "error in serializing buffer";
    return;
  }
}

// |output| will be filled with the serialized data
void SerializeCurve(const Curve &curve, MemoryBuffer<uint8> *output) {
  const size_t num_keys = const_cast<Curve&>(curve).keys().size();

  // Bezier entry size is biggest, so compute kKeyEntryMaxSize based on it
  const size_t kFloat2Size = 2 * sizeof(float);
  const size_t kKeyEntryMaxSize =
      sizeof(uint8) + 2 * sizeof(float) + 2 * kFloat2Size;

  const size_t key_data_max_size = num_keys * kKeyEntryMaxSize;
  const size_t max_total_size =
      kSerializationIDSize + kVersionSize + key_data_max_size;

  // Allocate a buffer which is large enough to hold the serialized data
  // It may be larger than the actual size required.  It will be resized
  // to the exact size at the end
  output->Resize(max_total_size);

  MemoryWriteStream stream(*output, max_total_size);

  // write out serialization ID for curve
  stream.Write(Curve::kSerializationID, 4);

  // write out version
  stream.WriteLittleEndianInt32(1);


  for (size_t i = 0; i < num_keys; ++i) {
    const CurveKey &key = *curve.GetKey(i);

    // determine the KeyType based on the key's class
    if (key.IsA(StepCurveKey::GetApparentClass())) {
      stream.WriteByte(CurveKey::TYPE_STEP);
      stream.WriteLittleEndianFloat32(key.input());
      stream.WriteLittleEndianFloat32(key.output());
    } else if (key.IsA(LinearCurveKey::GetApparentClass())) {
      stream.WriteByte(CurveKey::TYPE_LINEAR);
      stream.WriteLittleEndianFloat32(key.input());
      stream.WriteLittleEndianFloat32(key.output());
    } else if (key.IsA(BezierCurveKey::GetApparentClass())) {
      const BezierCurveKey &bezier_key =
          static_cast<const BezierCurveKey&>(key);
      stream.WriteByte(CurveKey::TYPE_BEZIER);
      stream.WriteLittleEndianFloat32(bezier_key.input());
      stream.WriteLittleEndianFloat32(bezier_key.output());
      stream.WriteLittleEndianFloat32(bezier_key.in_tangent().getX());
      stream.WriteLittleEndianFloat32(bezier_key.in_tangent().getY());
      stream.WriteLittleEndianFloat32(bezier_key.out_tangent().getX());
      stream.WriteLittleEndianFloat32(bezier_key.out_tangent().getY());
    } else {
      O3D_ERROR(curve.service_locator()) << "error in serializing curve";
      return;
    }
  }

  // Make note of total amount of data written and set the buffer to this
  // exact size
  size_t total_size = stream.GetStreamPosition();
  output->Resize(total_size);
}

// |output| will be filled with the serialized data
void SerializeSkin(const Skin &skin, MemoryBuffer<uint8> *output) {
  const Skin::InfluencesArray &influences_array = skin.influences();
  const size_t influences_array_size = influences_array.size();

  // Count up total number of individual influences
  size_t total_influence_count = 0;
  for (size_t i = 0; i < influences_array_size; ++i) {
    total_influence_count += influences_array[i].size();
  }

  const size_t kInfluenceSize = sizeof(uint32) + sizeof(float);
  const size_t total_size = kSerializationIDSize +
                            kVersionSize +
                            influences_array_size * sizeof(uint32) +
                            total_influence_count * kInfluenceSize;


  // Allocate a buffer to hold the serialized data
  output->Resize(total_size);
  MemoryWriteStream stream(*output, total_size);

  // write out serialization ID for skin
  stream.Write(Skin::kSerializationID, 4);

  // write out version
  stream.WriteLittleEndianInt32(1);

  for (size_t i = 0; i < influences_array_size; ++i) {
    const Skin::Influences &influences = influences_array[i];

    // Write the influence count for this Influences object
    size_t influence_count = influences.size();
    stream.WriteLittleEndianInt32(static_cast<int32>(influence_count));

    for (size_t j = 0; j < influence_count; ++j) {
      const Skin::Influence &influence = influences[j];
      stream.WriteLittleEndianInt32(influence.matrix_index);
      stream.WriteLittleEndianFloat32(influence.weight);
    }
  }
}

}  // namespace o3d
