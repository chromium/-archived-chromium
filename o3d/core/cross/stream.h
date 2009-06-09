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


// This file contains declarations for the Stream class.

#ifndef O3D_CORE_CROSS_STREAM_H_
#define O3D_CORE_CROSS_STREAM_H_

#include <vector>
#include "core/cross/field.h"
#include "core/cross/param.h"

namespace o3d {

class ServiceLocator;

// A stream object defines how Field data is interpreted by a Shape. It contains
// a reference to a Field the semantic associated with it.
class Stream : public ObjectBase {
 public:
  typedef SmartPointer<Stream> Ref;

  // Semantics used when binding buffers to the shape.  They determine how
  // the Stream links up to the shader inputs.
  // TODO: replace the Stream::Semantic types with proper semantic
  // strings and maps that allow aliasing of semantics.
  enum Semantic {
    UNKNOWN_SEMANTIC = 0,
    POSITION,    // Position
    NORMAL,      // Normal
    TANGENT,     // Tangent
    BINORMAL,    // Binormal
    COLOR,       // Color
    TEXCOORD,    // Texture Coordinate
  };

  Stream(ServiceLocator* service_locator,
         Field *field,
         unsigned int start_index,
         Semantic semantic,
         int semantic_index);

  // Returns a pointer to the associated Field object.
  Field& field() const {
    // This is guaranteed to be not NULL.
    return *field_.Get();
  }

  // Returns the semantic specified for the Stream.
  Semantic semantic() const { return semantic_; }

  // Returns the semantic index specified for the Stream
  // (eg., TEXCOORD1 = 1, BINORMAL7 = 7, etc).
  int semantic_index() const { return semantic_index_; }

  // Returns the start index for the specified Stream.
  unsigned int start_index() const { return start_index_; }

  // Returns the maximum vertices available given the stream's settings
  // and its buffer.
  unsigned GetMaxVertices() const;

  // Gets the last field change count.
  unsigned int last_field_change_count() const {
    return last_field_change_count_;
  }

  // Sets the last field change count.
  void set_last_field_change_count(unsigned int count) const {
    last_field_change_count_ = count;
  }

  static const char* GetSemanticDescription(Semantic);

 private:
  Field::Ref field_;

  // which field in the buffer this stream is using
  unsigned int field_id_;

  // Used to track if the field has changed offsets.
  mutable unsigned int last_field_change_count_;

  unsigned int start_index_;
  Semantic semantic_;
  int semantic_index_;

  O3D_DECL_CLASS(Stream, ObjectBase);
  DISALLOW_COPY_AND_ASSIGN(Stream);
};

// Defines a type for an array Stream objects.
typedef std::vector<Stream::Ref> StreamRefVector;

// A Param that can modify the contents of a vertex buffer.
// NOTE: This param behaves like no other param and therefore seems a little
// fishy. It feels like ParamChar (if there was such a thing) where you can do
// things like:
//
// param->set_value(value);
// value = param->value();
// param->bind(otherParamVertexBufferStream);
//
// But, the difference is when you call param->value(), if the param is bound to
// another stream we don't copy the stream or reference to the stream like other
// params. Using SlaveParamVertexBufferStream which is derived from this class
// a call to value() will call UpdateValue() which will call a virtual
// ComputeValue() which will update the vertex buffers in the destination stream
// using whatever means the owner of the Slave param specifies.
class ParamVertexBufferStream : public TypedParamBase<char> {
 public:
  typedef SmartPointer<ParamVertexBufferStream> Ref;

  ParamVertexBufferStream(ServiceLocator* service_locator,
                          Stream* stream,
                          bool dynamic,
                          bool read_only)
      : TypedParamBase<char>(service_locator, dynamic, read_only),
        stream_(stream) {
    DCHECK(stream);
  }

  virtual void CopyDataFromParam(Param *source_param);

  const Stream& stream() {
    // We guarantee there will always be a stream.
    return *stream_.Get();
  }

  // Marks the stream as valid so that it will not be updated again until
  // invalid.
  void ValidateStream() {
    Validate();
  }

  // Updates the stream if its bound to anything.
  void UpdateStream() {
    UpdateValue();
  }

 private:
  Stream::Ref stream_;

  O3D_DECL_CLASS(ParamVertexBufferStream, Param);
  DISALLOW_COPY_AND_ASSIGN(ParamVertexBufferStream);
};

typedef std::vector<ParamVertexBufferStream::Ref> StreamParamVector;

}  // namespace o3d

#endif  // O3D_CORE_CROSS_STREAM_H_
