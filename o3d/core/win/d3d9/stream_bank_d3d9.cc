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


// This file contains the definition of StreamBankD3D9.

#include "core/cross/precompile.h"

#include "core/win/d3d9/stream_bank_d3d9.h"

#include "core/cross/buffer.h"
#include "core/cross/param_cache.h"
#include "core/cross/renderer.h"
#include "core/cross/error.h"
#include "core/win/d3d9/buffer_d3d9.h"
#include "core/win/d3d9/d3d_entry_points.h"
#include "core/win/d3d9/draw_element_d3d9.h"
#include "core/win/d3d9/effect_d3d9.h"
#include "core/win/d3d9/param_cache_d3d9.h"
#include "core/win/d3d9/utils_d3d9.h"

// Someone defines min, conflicting with std::min
#ifdef min
#undef min
#endif

namespace o3d {

StreamBankD3D9::StreamBankD3D9(ServiceLocator* service_locator,
                               IDirect3DDevice9* d3d_device)
    : StreamBank(service_locator),
      d3d_device_(d3d_device),
      vertex_declaration_(NULL) {
  DCHECK(d3d_device);
}

StreamBankD3D9::~StreamBankD3D9() {
  FreeVertexDeclaration();
}

void StreamBankD3D9::FreeVertexDeclaration() {
  if (vertex_declaration_) {
    vertex_declaration_->Release();
    vertex_declaration_ = NULL;
  }
}

// Releases any old vertex declaration so a new one will be created when
// rendering that encompasses the change in streams.
void StreamBankD3D9::OnUpdateStreams() {
  FreeVertexDeclaration();
}

// Looks for any streams that are required by the vertex shader for which there
// is no equivalent stream bound to the o3d streambank.  If there are such
// streams it returns failure.
bool StreamBankD3D9::CheckForMissingVertexStreams(
    EffectD3D9* effect,
    Stream::Semantic* missing_semantic,
    int* missing_semantic_index) {
  DCHECK(missing_semantic);
  DCHECK(missing_semantic_index);
  EffectStreamInfoArray streamInfos;
  effect->GetStreamInfo(&streamInfos);
  for (EffectStreamInfoArray::size_type i = 0;
       i < streamInfos.size(); ++i) {
    Stream::Semantic semantic = streamInfos[i].semantic();
    int semantic_index = streamInfos[i].semantic_index();
    StreamParamVector::const_iterator iter;
    for (iter = vertex_stream_params_.begin();
         iter != vertex_stream_params_.end();
         ++iter) {
      const Stream& stream = (*iter)->stream();
      if (stream.semantic() == semantic &&
          stream.semantic_index() == semantic_index) {
        break;
      }
    }
    if (iter == vertex_stream_params_.end()) {
      // no matching stream was found.
      *missing_semantic = semantic;
      *missing_semantic_index = semantic_index;
      return false;
    }
  }
  return true;
}

bool StreamBankD3D9::BindStreamsForRendering(unsigned int* max_vertices) {
  DCHECK(max_vertices);
  *max_vertices = UINT_MAX;
  {
    StreamParamVector::const_iterator stream_iter;
    unsigned int count = 0;
    for (stream_iter = vertex_stream_params_.begin();
         stream_iter != vertex_stream_params_.end();
         ++stream_iter, count++) {
      const Stream& vertex_stream = (*stream_iter)->stream();
      const Field& field = vertex_stream.field();
      VertexBufferD3D9 *vertex_buffer =
          down_cast<VertexBufferD3D9*>(field.buffer());
      if (!vertex_buffer) {
        O3D_ERROR(service_locator())
            << "stream has no buffer in StreamBank '" << name() << "'";
        return false;
      }
      // TODO: Support stride of 0. The plan is to make it if num_elements
      // is 1 then do stride = 0. The problem is this is hard to do in GL.
      // so for now we're not doing it.
      HR(d3d_device_->SetStreamSource(count,
                                      vertex_buffer->d3d_buffer(),
                                      0,
                                      vertex_buffer->stride()));
      // If the buffer has changed structure then we need to re-create the
      // vertex declaration.
      if (vertex_stream.last_field_change_count() !=
          vertex_buffer->field_change_count()) {
        FreeVertexDeclaration();
        vertex_stream.set_last_field_change_count(
            vertex_buffer->field_change_count());
      }
      *max_vertices = std::min(*max_vertices, vertex_stream.GetMaxVertices());
    }
  }

  // Create the vertex declaration if it does not exist.
  if (!vertex_declaration_) {
    D3DVERTEXELEMENT9 *vertex_elements
        = new D3DVERTEXELEMENT9[vertex_stream_params_.size()+1];

    StreamParamVector::const_iterator stream_iter;
    unsigned int count = 0;
    for (stream_iter = vertex_stream_params_.begin();
         stream_iter != vertex_stream_params_.end();
         ++stream_iter, count++) {
      const Stream& stream = (*stream_iter)->stream();
      vertex_elements[count].Stream = count;
      vertex_elements[count].Offset = stream.field().offset();
      vertex_elements[count].Type = DX9DataType(stream.field());
      vertex_elements[count].Method = D3DDECLMETHOD_DEFAULT;
      vertex_elements[count].Usage = DX9UsageType(stream.semantic());
      vertex_elements[count].UsageIndex = stream.semantic_index();
    }
    D3DVERTEXELEMENT9 end_element = D3DDECL_END();
    vertex_elements[count] = end_element;

    HR(d3d_device_->CreateVertexDeclaration(vertex_elements,
                                            &vertex_declaration_));
    delete [] vertex_elements;
  }

  HR(d3d_device_->SetVertexDeclaration(vertex_declaration_));

  return true;
}

}  // namespace o3d
