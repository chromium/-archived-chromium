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


// This file contains the declaration of FakeVertexSource. It is only used in
// unit testing and is not compiled in with the plugin.

#ifndef O3D_CORE_CROSS_FAKE_VERTEX_SOURCE_H_
#define O3D_CORE_CROSS_FAKE_VERTEX_SOURCE_H_

#include <vector>
#include "core/cross/param_object.h"
#include "core/cross/vertex_source.h"

namespace o3d {

// This class is here to test that a primitive has its vertices updated from a
// VertexSource through binding. It copies its source vertices, multiplying each
// one by 2 + the semantic index of the destination stream.
class FakeVertexSource : public VertexSource {
 public:
  explicit FakeVertexSource(ServiceLocator* service_locator)
      : VertexSource(service_locator),
        update_outputs_call_count_() {
  }

  // Binds a SourceBuffer and defines how the data in the buffer should be
  // accessed and interpreted.
  bool SetVertexStream(Stream::Semantic semantic,
                       int semantic_index,
                       Field* field,
                       unsigned int start_index);

  // Removes a vertex stream from this primitive.
  // Returns true if the specified stream existed.
  bool RemoveVertexStream(Stream::Semantic stream_semantic,
                          int semantic_index);

  // Searches the vertex streams bound to the shape for one with the given
  // stream semantic.  If a stream is not found then it returns NULL.
  const Stream* GetVertexStream(Stream::Semantic stream_semantic,
                                int semantic_index) const;

  // Updates all the VertexBuffers bound to streams on this VertexSource.
  void UpdateStreams();

  // Updates the VertexBuffers bound to streams on this VertexSource.
  void UpdateOutputs();

  // For testing.
  unsigned update_outputs_call_count() {
    return update_outputs_call_count_;
  }

  // Overriden from
  virtual ParamVertexBufferStream* GetVertexStreamParam(
      Stream::Semantic semantic,
      int semantic_index) const;

 private:
  class SlaveParamVertexBufferStream : public ParamVertexBufferStream {
   public:
    SlaveParamVertexBufferStream(ServiceLocator* service_locator,
                                 FakeVertexSource* master,
                                 Stream* stream)
        : ParamVertexBufferStream(service_locator, stream, true, false),
          master_(master) {
    }

    virtual void ComputeValue() {
      master_->UpdateOutputs();
    }

   private:
    FakeVertexSource* master_;
    DISALLOW_COPY_AND_ASSIGN(SlaveParamVertexBufferStream);
  };

  typedef std::vector<SlaveParamVertexBufferStream::Ref> StreamParamVector;

  unsigned update_outputs_call_count_;
  StreamParamVector vertex_stream_params_;

  O3D_DECL_CLASS(FakeVertexSource, VertexSource);
  DISALLOW_COPY_AND_ASSIGN(FakeVertexSource);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_FAKE_VERTEX_SOURCE_H_




