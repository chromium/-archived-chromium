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


// This file contains the definition of FakeVertexSource. It is only used in
// unit testing and should not be compiled in with the plugin.

#include "core/cross/precompile.h"
#include "core/cross/fake_vertex_source.h"
#include "core/cross/buffer.h"

namespace o3d {

O3D_DEFN_CLASS(FakeVertexSource, VertexSource);

bool FakeVertexSource::SetVertexStream(Stream::Semantic semantic,
                                       int semantic_index,
                                       Field* field,
                                       unsigned int start_index) {
  Buffer* buffer = field->buffer();
  if (!buffer || !buffer->IsA(SourceBuffer::GetApparentClass())) {
    return false;
  }

  Stream::Ref stream(new Stream(service_locator(),
                                field,
                                start_index,
                                semantic,
                                semantic_index));

  // If a stream with the same semantic has already been set then remove it.
  RemoveVertexStream(semantic, semantic_index);

  ParamVertexBufferStream::Ref stream_param(new SlaveParamVertexBufferStream(
     service_locator(), this, stream));
  vertex_stream_params_.push_back(stream_param);

  return true;
}

bool FakeVertexSource::RemoveVertexStream(Stream::Semantic stream_semantic,
                                          int semantic_index) {
  StreamParamVector::iterator iter, end = vertex_stream_params_.end();
  for (iter = vertex_stream_params_.begin(); iter != end; ++iter) {
    const Stream& stream = (*iter)->stream();
    if (stream.semantic() == stream_semantic &&
        stream.semantic_index() == semantic_index) {
      vertex_stream_params_.erase(iter);
      return true;
    }
  }
  return false;
}

const Stream* FakeVertexSource::GetVertexStream(
    Stream::Semantic stream_semantic,
    int semantic_index) const {
  ParamVertexBufferStream* param = GetVertexStreamParam(stream_semantic,
                                                        semantic_index);
  return param ? &param->stream() : NULL;
}

ParamVertexBufferStream* FakeVertexSource::GetVertexStreamParam(
    Stream::Semantic semantic,
    int semantic_index) const {
  StreamParamVector::const_iterator iter, end = vertex_stream_params_.end();
  for (iter = vertex_stream_params_.begin(); iter != end; ++iter) {
    const Stream& stream = (*iter)->stream();
    if (stream.semantic() == semantic &&
        stream.semantic_index() == semantic_index) {
      return *iter;
    }
  }
  return NULL;
}

void FakeVertexSource::UpdateStreams() {
  for (unsigned ii = 0; ii < vertex_stream_params_.size(); ++ii) {
    vertex_stream_params_[ii]->UpdateStream();
  }
}

void FakeVertexSource::UpdateOutputs() {
  ++update_outputs_call_count_;

  // Now copy our streams to their outputs.
  for (unsigned ii = 0; ii < vertex_stream_params_.size(); ++ii) {
    ParamVertexBufferStream* source_param = vertex_stream_params_[ii];

    // Make sure our upstream streams are ready
    ParamVertexBufferStream* input = down_cast<ParamVertexBufferStream*>(
        source_param->input_connection());
    if (input) {
      input->UpdateStream();  // Will automatically mark us as valid.
    } else {
      // Mark us as valid so we don't evaluate a second time.
      source_param->ValidateStream();
    }

    const Stream& source_stream = source_param->stream();
    const Field& source_field = source_stream.field();
    unsigned num_components = source_field.num_components();
    Buffer* source_buffer = source_field.buffer();
    if (source_buffer) {
      const ParamVector& outputs = source_param->output_connections();
      BufferLockHelper source_helper(source_buffer);
      void* source_data = source_helper.GetData(Buffer::READ_ONLY);
      if (source_data) {
        unsigned source_num_vertices = source_stream.GetMaxVertices();
        unsigned source_stride = source_buffer->stride();
        for (unsigned jj = 0; jj < outputs.size(); ++jj) {
          ParamVertexBufferStream* destination_param =
              down_cast<ParamVertexBufferStream*>(outputs[jj]);
          // TODO: Make calling this automatic so that each concrete
          //     VertexSource does not have to manually call it.
          destination_param->ValidateStream();
          const Stream& destination_stream = destination_param->stream();
          const Field& destination_field = destination_stream.field();
          Buffer* destination_buffer = destination_field.buffer();
          if (destination_buffer) {
            BufferLockHelper destination_helper(destination_buffer);
            void* destination_data =
                destination_helper.GetData(Buffer::WRITE_ONLY);
            if (destination_data) {
              unsigned num_vertices = std::min(
                  destination_stream.GetMaxVertices(),
                  source_num_vertices);
              if (num_vertices < UINT_MAX) {
                unsigned destination_stride = destination_buffer->stride();
                float* source = PointerFromVoidPointer<float*>(
                    source_data, source_field.offset());
                float* destination = PointerFromVoidPointer<float*>(
                    destination_data, destination_field.offset());
                while (num_vertices) {
                  for (unsigned jj = 0; jj < num_components; ++jj) {
                    destination[jj] =
                        source[jj] * (destination_stream.semantic_index() + 2);
                  }
                  destination = AddPointerOffset(destination,
                                                 destination_stride);
                  source = AddPointerOffset(source, source_stride);
                  --num_vertices;
                }
              }
            }
          }
        }
      }
    }
  }
}

}  // namespace o3d
