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


// This file contains the declaration of class Serializer.

#ifndef O3D_SERIALIZER_CROSS_SERIALIZER_H_
#define O3D_SERIALIZER_CROSS_SERIALIZER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "core/cross/bounding_box.h"
#include "core/cross/float_n.h"
#include "core/cross/object_base.h"
#include "core/cross/stream.h"
#include "core/cross/types.h"
#include "core/cross/visitor_base.h"
#include "core/cross/service_dependency.h"
#include "import/cross/iarchive_generator.h"
#include "utils/cross/structured_writer.h"

namespace o3d {

class IClassManager;
class Pack;
class StructuredWriter;
class Transform;

// Serialize value to a StructuredWriter.
void Serialize(StructuredWriter* writer, ObjectBase* value);

// Serialize value to a StructuredWriter.
void Serialize(StructuredWriter* writer, float value);

// Serialize value to a StructuredWriter.
void Serialize(StructuredWriter* writer, const Float2& value);

// Serialize value to a StructuredWriter.
void Serialize(StructuredWriter* writer, const Float3& value);

// Serialize value to a StructuredWriter.
void Serialize(StructuredWriter* writer, const Float4& value);

// Serialize value to a StructuredWriter.
void Serialize(StructuredWriter* writer, int value);

// Serialize value to a StructuredWriter.
void Serialize(StructuredWriter* writer, unsigned int value);

// Serialize value to a StructuredWriter.
void Serialize(StructuredWriter* writer, bool value);

// Serialize value to a StructuredWriter.
void Serialize(StructuredWriter* writer, const String& value);

// Serialize value to a StructuredWriter.
void Serialize(StructuredWriter* writer, const Matrix4& value);

// Serialize value to a StructuredWriter.
void Serialize(StructuredWriter* writer, const BoundingBox& value);

// Serialize value to a StructuredWriter.
void Serialize(StructuredWriter* writer, const Stream& value);

// A range of bytes within a binary file.
struct BinaryRange {
  BinaryRange() : begin_offset_(0), end_offset_(0) {}
  BinaryRange(size_t begin_offset, size_t end_offset)
      : begin_offset_(begin_offset),
        end_offset_(end_offset) {
  }
  size_t begin_offset_;
  size_t end_offset_;
};

// A class that tracks the contents of binary files in an archive and the ranges
// corresponding to each object.
class BinaryArchiveManager {
  typedef std::vector<uint8> FileContent;
  typedef std::map<std::string, FileContent > FileMap;
  typedef std::map<ObjectBase*, BinaryRange> ObjectBinaryRangeMap;

 public:
  BinaryArchiveManager() {
  }

  // Write the binary content for an object. Multiple consecutive calls
  // can be made for a single object. Calls for different objects may not
  // be interleaved. All binary content for a particular object must be
  // written to a single file. This function does not write to the archive.
  // That is deferred until WriteArchive is called.
  void WriteObjectBinary(ObjectBase* object,
                         const std::string& file_name,
                         const uint8* begin,
                         size_t numBytes);

  // Gets the byte range of the file corresponding to a particular
  // object.
  BinaryRange GetObjectRange(ObjectBase* object);

  // Writes all the collected binary data to the archive.
  void WriteArchive(IArchiveGenerator* archive_generator);

 private:
  FileMap file_map_;
  ObjectBinaryRangeMap object_binary_range_map_;
  DISALLOW_COPY_AND_ASSIGN(BinaryArchiveManager);
};

// Serializes whole packs, individual objects, individual sections
// of objects or individual Params to a StructuedWriter.
class Serializer {
 public:
  // Enumeration of all sections that may optionally be included in
  // an object.
  enum Section {
    PROPERTIES_SECTION,
    CUSTOM_SECTION,
    NUM_SECTIONS
  };

  // Any object that starts with this prefix will not be seralized
  // but a reference to it will be put at the top of the json object.
  static const char* ROOT_PREFIX;

  // Construct a new Serializer that writes future output to the
  // given StructuredWriter and IArchiveGenerator.
  explicit Serializer(ServiceLocator* service_locator,
                      StructuredWriter* writer,
                      IArchiveGenerator* archive_generator);
  ~Serializer();

  // Serialize a Pack and all the objects contained by the pack
  // to the StructuredWriter.
  void SerializePack(Pack* pack);

  // Serialize all the binary files in a pack.
  void SerializePackBinary(Pack* pack);

  // Serialize a single object to the StructuredWriter.
  void SerializeObject(ObjectBase* object);

  // Serialize one of the sections of an object to the StructuredWriter.
  void SerializeSection(ObjectBase* object, Section section);

  // Serialize a single Param of an object to the StructuredWriter.
  void SerializeParam(Param* param);

 private:
  ServiceDependency<IClassManager> class_manager_;
  StructuredWriter* writer_;
  IArchiveGenerator* archive_generator_;

  struct SectionConfig {
    const char* name_;
    IVisitor* visitor_;
  };
  SectionConfig sections_[NUM_SECTIONS];

  IVisitor* param_visitor_;
  IVisitor* binary_visitor_;

  BinaryArchiveManager binary_archive_manager_;

  DISALLOW_COPY_AND_ASSIGN(Serializer);
};
}  // namespace o3d

#endif  // O3D_SERIALIZER_CROSS_SERIALIZER_H_
