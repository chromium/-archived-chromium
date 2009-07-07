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


// This file contains the definition of StreamBank.

#include "core/cross/precompile.h"
#include "core/cross/stream_bank.h"
#include "core/cross/renderer.h"
#include "core/cross/error.h"
#include "core/cross/vertex_source.h"

namespace o3d {

O3D_DEFN_CLASS(StreamBank, NamedObject);
O3D_DEFN_CLASS(ParamStreamBank, RefParamBase);

ObjectBase::Ref StreamBank::Create(ServiceLocator* service_locator) {
  Renderer* renderer = service_locator->GetService<Renderer>();
  if (NULL == renderer) {
    O3D_ERROR(service_locator) << "No Render Device Available";
    return ObjectBase::Ref();
  }

  return ObjectBase::Ref(renderer->CreateStreamBank());
}

StreamBank::StreamBank(ServiceLocator* service_locator)
    : NamedObject(service_locator),
      number_binds_(0),
      change_count_(1),
      weak_pointer_manager_(this) {
}

StreamBank::~StreamBank() {
}

unsigned StreamBank::GetMaxVertices() const {
  unsigned int max_vertices = UINT_MAX;
  StreamParamVector::const_iterator stream_iter;
  for (stream_iter = vertex_stream_params_.begin();
       stream_iter != vertex_stream_params_.end();
       ++stream_iter) {
    const Stream& stream = (*stream_iter)->stream();
    max_vertices = std::min(max_vertices, stream.GetMaxVertices());
  }
  return max_vertices;
}

// Adds a new vertex Stream to the StreamBank.  If a Stream with the same
// semantic is already bound to the StreamBank then it removes it before adding
// the new one. Otherwise, it creates a new stream with the information supplied
// in the parameters and adds it to the array of streams referenced by the
// StreamBank.
bool StreamBank::SetVertexStream(Stream::Semantic stream_semantic,
                                 int semantic_index,
                                 Field* field,
                                 unsigned int start_index) {
  Buffer* buffer = field->buffer();
  if (!buffer) {
    O3D_ERROR(service_locator()) << "No buffer on field";
    return false;
  }

  // Check that this buffer is renderable. StreamBanks are used to submit
  // data to GPU so we can only allow GPU accessible buffers through here.
  if (!buffer->IsA(VertexBuffer::GetApparentClass())) {
    O3D_ERROR(service_locator()) << "Buffer is not a VertexBuffer";
    return false;
  }

  ++change_count_;

  Stream::Ref stream(new Stream(service_locator(),
                                field,
                                start_index,
                                stream_semantic,
                                semantic_index));

  // If a stream with the same semantic has already been set then remove it.
  RemoveVertexStream(stream_semantic, semantic_index);

  ParamVertexBufferStream::Ref stream_param(
      new SlaveParamVertexBufferStream(service_locator(), this, stream));
  vertex_stream_params_.push_back(stream_param);

  OnUpdateStreams();

  return true;
}

// Looks for a vertex stream with the given semantic in the array of vertex
// streams stored in the StreamBank.  Right now it does a simple linear pass
// through all the streams.
const Stream* StreamBank::GetVertexStream(Stream::Semantic stream_semantic,
                                          int semantic_index) const {
  ParamVertexBufferStream* param = GetVertexStreamParam(stream_semantic,
                                                        semantic_index);
  return param ? &param->stream() : NULL;
}

ParamVertexBufferStream* StreamBank::GetVertexStreamParam(
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

bool StreamBank::RemoveVertexStream(Stream::Semantic stream_semantic,
                                    int semantic_index) {
  StreamParamVector::iterator iter, end = vertex_stream_params_.end();
  for (iter = vertex_stream_params_.begin(); iter != end; ++iter) {
    const Stream& stream = (*iter)->stream();
    if (stream.semantic() == stream_semantic &&
        stream.semantic_index() == semantic_index) {
      ++change_count_;
      vertex_stream_params_.erase(iter);
      OnUpdateStreams();
      return true;
    }
  }
  return false;
}

bool StreamBank::BindStream(VertexSource* source,
                            Stream::Semantic semantic,
                            int semantic_index) {
  if (source) {
    ParamVertexBufferStream* source_param = source->GetVertexStreamParam(
        semantic, semantic_index);
    ParamVertexBufferStream* dest_param = GetVertexStreamParam(
        semantic, semantic_index);
    if (source_param && dest_param &&
        source_param->stream().field().IsA(
            dest_param->stream().field().GetClass()) &&
        source_param->stream().field().num_components() ==
        dest_param->stream().field().num_components()) {
      return dest_param->Bind(source_param);
    }
  }

  return false;
}

bool StreamBank::UnbindStream(Stream::Semantic semantic, int semantic_index) {
  ParamVertexBufferStream* dest_param = GetVertexStreamParam(
      semantic, semantic_index);
  if (dest_param && dest_param->input_connection() != NULL) {
    dest_param->UnbindInput();
    return true;
  }
  return false;
}

void StreamBank::UpdateStreams() {
  if (number_binds_) {
    // TODO: Although a second call to UpdateStream on these streams
    // should do nothing, is there any way to void the loop the second time
    // through? Short of checking that each stream param is valid which is a
    // loop, I'm not sure what we could do but because this loop is only called
    // for things that are bound it's unlikely it will be here often.
    for (unsigned ii = 0; ii < vertex_stream_params_.size(); ++ii) {
      vertex_stream_params_[ii]->UpdateStream();  // Triggers updating.
    }
  }
}

ObjectBase::Ref ParamStreamBank::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamStreamBank(service_locator, false, false));
}

}  // namespace o3d
