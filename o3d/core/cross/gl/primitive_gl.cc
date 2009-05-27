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


// This file contains the definition of PrimitiveGL.

#include <algorithm>

#include "core/cross/precompile.h"
#include "core/cross/stream.h"
#include "core/cross/error.h"
#include "core/cross/gl/buffer_gl.h"
#include "core/cross/gl/effect_gl.h"
#include "core/cross/gl/primitive_gl.h"
#include "core/cross/gl/renderer_gl.h"
#include "core/cross/gl/draw_element_gl.h"
#include "core/cross/gl/stream_bank_gl.h"
#include "core/cross/gl/utils_gl-inl.h"
#include "Cg/cgGL.h"

// Someone defines min, conflicting with std::min
#ifdef min
#undef min
#endif

namespace o3d {

// Number of times to log a repeated event before giving up.
const int kNumLoggedEvents = 5;

// PrimitiveGL functions -------------------------------------------------------

PrimitiveGL::PrimitiveGL(ServiceLocator* service_locator)
    : Primitive(service_locator) {
  DLOG(INFO) << "PrimitiveGL Construct";
}

PrimitiveGL::~PrimitiveGL() {
  DLOG(INFO) << "PrimitiveGL Destruct";
}

// Binds the vertex and index streams required to draw the shape.  If the
// vertex or fragment programs have changed since the last time this method
// was called (or it's the first time it's getting called) then it forces
// an update of the mapping between the Shape Param's and the shader parameters
// and also fills in for any missing streams.
void PrimitiveGL::Render(Renderer* renderer,
                         DrawElement* draw_element,
                         Material* material,
                         ParamObject* override,
                         ParamCache* param_cache) {
  DLOG_ASSERT(draw_element);
  DLOG_ASSERT(param_cache);
  DLOG_FIRST_N(INFO, kNumLoggedEvents) << "PrimitiveGL Draw \""
                                       << draw_element->name() << "\"";
  // If there's no material attached to this Shape.
  if (!material) {
    O3D_ERROR(service_locator()) << "No Material attached to Shape \""
                                 << draw_element->name() << "\"";
    return;
  }
  DrawElementGL* draw_element_gl = down_cast<DrawElementGL*>(draw_element);
  EffectGL* effect_gl = down_cast<EffectGL*>(material->effect());
  // If there's no effect attached to this Material.
  if (!effect_gl) {
    O3D_ERROR(service_locator()) << "No Effect attached to Material '"
                                 << material->name() << "' in Shape '"
                                 << draw_element_gl->name() << "'";
    return;
  }
  StreamBankGL* stream_bank_gl = down_cast<StreamBankGL*>(stream_bank());
  if (!stream_bank_gl) {
    O3D_ERROR(service_locator())
        << "No StreambBank attached to Primitive '"
        << material->name() << "' in Shape '"
        << draw_element_gl->name() << "'";
    return;
  }

  ParamCacheGL* param_cache_gl = down_cast<ParamCacheGL*>(param_cache);
  ParamCacheGL::VaryingParameterMap& varying_map =
      param_cache_gl->varying_map();

  // If this PrimitiveGL has an effect we haven't seen before (or it's the first
  // time through), initalize the parameter lists before drawing with it.
  if (effect_gl->cg_vertex_program() && effect_gl->cg_fragment_program()) {
    // Set up the current CGeffect.
    if (!param_cache_gl->ValidateAndCacheParams(effect_gl,
                                                draw_element_gl,
                                                this,
                                                stream_bank_gl,
                                                material,
                                                override)) {
      Stream::Semantic missing_semantic;
      int missing_semnatic_index;
      if (!stream_bank_gl->CheckForMissingVertexStreams(
              varying_map,
              &missing_semantic,
              &missing_semnatic_index)) {
        param_cache_gl->ClearParamCache();
        O3D_ERROR(service_locator())
            << "Required Stream "
            << Stream::GetSemanticDescription(missing_semantic) << ":"
            << missing_semnatic_index << " missing on Primitive '" << name()
            << "' using Material '" << material->name()
            << "' with Effect '" << effect_gl->name() << "'";
        return;
      }
    }
  } else {
    O3D_ERROR(service_locator())
        << "No CG effect provided in Effect \""
        << effect_gl->name() << "\" used by Material \""
        << material->name() << "\" in Shape \""
        << draw_element_gl->name() << "\". Drawing nothing.";
    return;
  }

  // Make sure our streams are up to date (skinned, etc..)
  stream_bank_gl->UpdateStreams();

  unsigned int max_vertices;
  if (!stream_bank_gl->BindStreamsForRendering(varying_map, &max_vertices)) {
    return;
  }

  // TODO: move these checks at 'set' time instead of draw time.

  bool draw = true;
  if (number_vertices_ > max_vertices) {
    O3D_ERROR(service_locator())
        << "Trying to draw with " << number_vertices_
        << " vertices when there are only " << max_vertices
        << " available in the buffers. Skipping primitive.";
    draw = false;
  }

  unsigned int index_count;

  if (!Primitive::GetIndexCount(primitive_type_,
                                number_primitives_,
                                &index_count)) {
    O3D_ERROR(service_locator())
        << "Unknown Primitive Type in GetIndexCount: "
        << primitive_type_ << ". Skipping primitive "
        << name();
    draw = false;
  }

  if (indexed()) {
    // Re-bind the index buffer for this shape
    IndexBufferGL *ibuffer = down_cast<IndexBufferGL*>(index_buffer());

    unsigned int max_indices = ibuffer->num_elements();

    if (index_count > max_indices) {
      O3D_ERROR(service_locator())
          << "Trying to draw with " << index_count
          << " indices when only " << max_indices
          << " are available in the buffer. Skipping shape.";
      draw = false;
    }

    // TODO: Also check that indices in the index buffer are less than
    // max_vertices_. Needs support from the index buffer (scan indices on
    // Unlock).

    glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER, ibuffer->gl_buffer());
  }

  // Set up the shaders in this drawcall from the Effect.
  effect_gl->PrepareForDraw(param_cache_gl);

  // Do the drawcall.
  GLenum gl_primitive_type = GL_NONE;
  switch (primitive_type_) {
    case Primitive::POINTLIST : {
      if (indexed()) {
        O3D_ERROR(service_locator())
            << "POINTLIST unsupported for indexed primitives for primitive "
            << name();
        draw = false;
      } else {
        gl_primitive_type = GL_POINTS;
      }
      break;
    }
    case Primitive::LINELIST : {
      DLOG_FIRST_N(INFO, kNumLoggedEvents)
          << "Draw " << number_primitives_ << " GL_LINES";
      gl_primitive_type = GL_LINES;
      break;
    }
    case Primitive::LINESTRIP : {
      DLOG_FIRST_N(INFO, kNumLoggedEvents)
          << "Draw " << number_primitives_ << " GL_LINE_STRIP";
      gl_primitive_type = GL_LINE_STRIP;
      break;
    }
    case Primitive::TRIANGLELIST : {
      DLOG_FIRST_N(INFO, kNumLoggedEvents)
          << "Draw " << number_primitives_ << " GL_TRIANGLES";
      gl_primitive_type = GL_TRIANGLES;
      break;
    }
    case Primitive::TRIANGLESTRIP : {
      DLOG_FIRST_N(INFO, kNumLoggedEvents)
          << "Draw " << number_primitives_ << " GL_TRIANGLE_STRIP";
      gl_primitive_type = GL_TRIANGLE_STRIP;
      break;
    }
    case Primitive::TRIANGLEFAN : {
      DLOG_FIRST_N(INFO, kNumLoggedEvents)
          << "Draw " << number_primitives_ << " GL_TRIANGLE_FAN";
      gl_primitive_type = GL_TRIANGLE_FAN;
      break;
    }
    default : {
      DLOG(ERROR) << "Unknown Primitive Type in Primitive: "
                  << primitive_type_;
      draw = false;
    }
  }
  if (draw) {
    DCHECK_NE(gl_primitive_type, GL_NONE);
    renderer->AddPrimitivesRendered(number_primitives_);
    if (indexed())
      glDrawElements(gl_primitive_type,
                     index_count,
                     GL_UNSIGNED_INT,
                     BUFFER_OFFSET(start_index() * sizeof(uint32)));  // NOLINT
    else
      glDrawArrays(gl_primitive_type, start_index(), index_count);
  }

  // Clean up the shaders.
  effect_gl->PostDraw(param_cache_gl);

  // Disable the vertex attribute states set earlier.
  for (ParamCacheGL::VaryingParameterMap::iterator i = varying_map.begin();
       i != varying_map.end();
       ++i) {
    cgGLDisableClientState(i->first);
  }
  CHECK_GL_ERROR();
}

}  // namespace o3d
