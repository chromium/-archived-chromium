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


// This file contains the declaration of VertexSource.

#ifndef O3D_CORE_CROSS_VERTEX_SOURCE_H_
#define O3D_CORE_CROSS_VERTEX_SOURCE_H_

#include "core/cross/param_object.h"
#include "core/cross/stream.h"

namespace o3d {

// A VertexSource is an abstract base class for classes that allow binding
// Streams such that the VertexSource updates the Buffers of the Streams that
// have been bound to it. Example of concrete VertexSource objects would be
// SkinEval, BlendShapeEval, TerrainEval
class VertexSource : public ParamObject {
 public:
  explicit VertexSource(ServiceLocator* service_locator)
      : ParamObject(service_locator) {
  }

  // Used by BindStream. Each derived class must provide this function.
  // Returns the ParamVertexBufferStream that manages the given stream. as an
  // output param for this VertexSource.
  virtual ParamVertexBufferStream* GetVertexStreamParam(
      Stream::Semantic semantic,
      int semantic_index) const = 0;

  // Bind the source stream to the corresponding stream in this VertexSource.
  // Parameters:
  //   source: Source to get vertices from.
  //   semantic: The semantic of the vertices to get
  //   semantic_index: The semantic index of the vertices to get.
  // Returns:
  //   True if success. False if failure. If the requested semantic or semantic
  //   index do not exist on the source or this source the bind will fail.
  bool BindStream(VertexSource* source,
                  Stream::Semantic semantic,
                  int semantic_index);

  // Unbinds the requested stream.
  // Parameters:
  //   semantic: The semantic of the vertices to unbind
  //   semantic_index: The semantic index of the vertices to unbind.
  // Returns:
  //   True if unbound. False those vertices do not exist or were not bound.
  bool UnbindStream(Stream::Semantic semantic, int semantic_index);

 private:
  O3D_DECL_CLASS(VertexSource, ParamObject);
  DISALLOW_COPY_AND_ASSIGN(VertexSource);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_VERTEX_SOURCE_H_




