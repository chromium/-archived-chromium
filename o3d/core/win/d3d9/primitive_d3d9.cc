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


// This file contains the definition of PrimitiveD3D9.

#include "core/cross/precompile.h"

#include "core/win/d3d9/primitive_d3d9.h"

#include "core/cross/buffer.h"
#include "core/cross/param_cache.h"
#include "core/cross/renderer.h"
#include "core/cross/error.h"
#include "core/win/d3d9/buffer_d3d9.h"
#include "core/win/d3d9/d3d_entry_points.h"
#include "core/win/d3d9/draw_element_d3d9.h"
#include "core/win/d3d9/effect_d3d9.h"
#include "core/win/d3d9/param_cache_d3d9.h"
#include "core/win/d3d9/stream_bank_d3d9.h"
#include "core/win/d3d9/utils_d3d9.h"

// Someone defines min, conflicting with std::min
#ifdef min
#undef min
#endif

namespace o3d {

PrimitiveD3D9::PrimitiveD3D9(ServiceLocator* service_locator,
                             IDirect3DDevice9* d3d_device)
    : Primitive(service_locator),
      d3d_device_(d3d_device) {
  DCHECK(d3d_device);
}

PrimitiveD3D9::~PrimitiveD3D9() {
}

// Binds the vertex and index streams required to draw the shape.  If the
// vertex or fragment programs have changed since the last time this method
// was called (or it's the first time it's getting called) then it forces
// an update of the mapping between the Material's Params and the shader
// parameters and also fills in for any missing streams.
void PrimitiveD3D9::Render(Renderer* renderer,
                           DrawElement* draw_element,
                           Material* material,
                           ParamObject* override,
                           ParamCache* param_cache) {
  DLOG_ASSERT(param_cache != NULL);
  ParamCacheD3D9* param_cache_d3d9 = down_cast<ParamCacheD3D9*>(param_cache);

  if (!material) {
    O3D_ERROR(service_locator())
        << "No material on Primitive '" << name() << "'";
    return;
  }

  EffectD3D9* effect_d3d9 = down_cast<EffectD3D9*>(material->effect());
  if (!effect_d3d9) {
    O3D_ERROR(service_locator())
        << "No effect on material '" << material->name() << "'";
    return;
  }

  StreamBankD3D9* stream_bank_d3d9 = down_cast<StreamBankD3D9*>(stream_bank());
  if (!stream_bank_d3d9) {
    O3D_ERROR(service_locator())
        << "No stream bank on Primitive '" << name() << "'";
    return;
  }

  DrawElementD3D9* draw_element_d3d9 = down_cast<DrawElementD3D9*>(
      draw_element);

  IDirect3DVertexShader9* vshader = effect_d3d9->d3d_vertex_shader();
  IDirect3DPixelShader9* fshader = effect_d3d9->d3d_fragment_shader();

  if (!param_cache_d3d9->ValidateAndCacheParams(effect_d3d9,
                                                draw_element_d3d9,
                                                this,
                                                stream_bank_d3d9,
                                                material,
                                                override)) {
    Stream::Semantic missing_semantic;
    int missing_semnatic_index;
    if (!stream_bank_d3d9->CheckForMissingVertexStreams(
            effect_d3d9,
            &missing_semantic,
            &missing_semnatic_index)) {
      param_cache_d3d9->ClearParamCache();
      O3D_ERROR(service_locator())
          << "Required Stream "
          << Stream::GetSemanticDescription(missing_semantic) << ":"
          << missing_semnatic_index << " missing on Primitive '" << name()
          << "' using Material '" << material->name()
          << "' with Effect '" << effect_d3d9->name() << "'";
      return;
    }
  }

  if (indexed()) {
    // Set the index stream
    IndexBufferD3D9* index_buffer_d3d9 =
        down_cast<IndexBufferD3D9*>(index_buffer());
    unsigned int max_indices = index_buffer_d3d9->num_elements();
    unsigned int index_count;

    if (!Primitive::GetIndexCount(primitive_type_,
                                  number_primitives_,
                                  &index_count)) {
      O3D_ERROR(service_locator())
          << "Unknown Primitive Type in GetIndexCount: "
          << primitive_type_ << ". Skipping primitive "
          << name();
      return;
    }
    if (index_count > max_indices) {
      O3D_ERROR(service_locator())
          << "Trying to draw with " << index_count
          << " indices when only " << max_indices
          << " are available in the buffer. Skipping primitive " << name();
      return;
    }
    // TODO: Also check that indices in the index buffer are less than
    // max_vertices_. Needs support from the index buffer (scan indices on
    // Unlock).
    HR(d3d_device_->SetIndices(index_buffer_d3d9->d3d_buffer()));
  }

  // Make sure our streams are up to date (skinned, etc..)
  stream_bank_d3d9->UpdateStreams();

  // Get all the vertex streams associated with the shape
  unsigned int max_vertices;
  if (!stream_bank_d3d9->BindStreamsForRendering(&max_vertices)) {
    return;
  }

  // TODO: move these checks at 'set' time instead of draw time.

  // Check the max number of vertices. Do this after InsertMissingVertexStreams
  // happenned because it may modify the list of streams, thus max_vertices_.
  if (number_vertices_ > max_vertices) {
    O3D_ERROR(service_locator())
        << "Trying to draw with " << number_vertices_
        << " vertices when there are only " << max_vertices
        << " available in the buffers. Skipping primitive " << name();
    return;
  }

  // Setup the shaders in the effect
  if (effect_d3d9) {
    effect_d3d9->PrepareForDraw(param_cache_d3d9->cached_effect_params());
  }

  // Draw the appropriate primitive
  D3DPRIMITIVETYPE d3d_primitive_type;
  switch (primitive_type()) {
    case Primitive::POINTLIST:
      if (indexed()) {
        O3D_ERROR(service_locator())
            << "POINTLIST unsupported for indexed primitives for primitive "
            << name();
        return;
      } else {
        d3d_primitive_type = D3DPT_POINTLIST;
        break;
      }
    case Primitive::LINELIST:
      d3d_primitive_type = D3DPT_LINELIST;
      break;
    case Primitive::LINESTRIP:
      d3d_primitive_type = D3DPT_LINESTRIP;
      break;
    case Primitive::TRIANGLELIST:
      d3d_primitive_type = D3DPT_TRIANGLELIST;
      break;
    case Primitive::TRIANGLESTRIP:
      d3d_primitive_type = D3DPT_TRIANGLESTRIP;
      break;
    case Primitive::TRIANGLEFAN:
      d3d_primitive_type = D3DPT_TRIANGLEFAN;
      break;
    default:
      O3D_ERROR(service_locator())
          << "Unknown Primitive Type in Primitive: "
          << primitive_type() << " for primitive "
          << name();
      return;
  }
  renderer->AddPrimitivesRendered(number_primitives_);

  if (indexed()) {
    d3d_device_->DrawIndexedPrimitive(d3d_primitive_type,
                                      0,
                                      0,
                                      number_vertices_,
                                      start_index_,
                                      number_primitives_);
  } else {
    d3d_device_->DrawPrimitive(
        d3d_primitive_type, start_index_, number_primitives_);
  }

  if (effect_d3d9) {
    effect_d3d9->PostDraw(this, param_cache_d3d9->cached_effect_params());
  }
}

}  // namespace o3d
