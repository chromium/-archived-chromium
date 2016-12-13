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


// This file contains the definition of StreamBankGL.

#include <algorithm>

#include "core/cross/precompile.h"
#include "core/cross/stream.h"
#include "core/cross/error.h"
#include "core/cross/gl/buffer_gl.h"
#include "core/cross/gl/effect_gl.h"
#include "core/cross/gl/stream_bank_gl.h"
#include "core/cross/gl/renderer_gl.h"
#include "core/cross/gl/draw_element_gl.h"
#include "core/cross/gl/utils_gl-inl.h"
#include "Cg/cgGL.h"

// Someone defines min, conflicting with std::min
#ifdef min
#undef min
#endif

namespace o3d {

namespace {

// Converts from a Field datatype to a suitable GL type
GLenum GLDataType(const Field& field) {
  if (field.IsA(FloatField::GetApparentClass())) {
    return GL_FLOAT;
  } else if (field.IsA(UByteNField::GetApparentClass())) {
    switch (field.num_components()) {
      case 4:
        return GL_UNSIGNED_BYTE;
    }
  }
  DLOG(ERROR) << "Unknown Stream DataType";
  return GL_INVALID_ENUM;
}

}  // anonymous namespace

// Number of times to log a repeated event before giving up.
const int kNumLoggedEvents = 5;

// StreamBankGL functions ------------------------------------------------------

StreamBankGL::StreamBankGL(ServiceLocator* service_locator)
    : StreamBank(service_locator) {
  DLOG(INFO) << "StreamBankGL Construct";
}

StreamBankGL::~StreamBankGL() {
  DLOG(INFO) << "StreamBankGL Destruct";
}

bool StreamBankGL::CheckForMissingVertexStreams(
    ParamCacheGL::VaryingParameterMap& varying_map,
    Stream::Semantic* missing_semantic,
    int* missing_semantic_index) {
  DCHECK(missing_semantic);
  DCHECK(missing_semantic_index);
  DLOG(INFO) << "StreamBankGL InsertMissingVertexStreams";
  // Match CG_VARYING parameters to Buffers with the matching semantics.
  ParamCacheGL::VaryingParameterMap::iterator i;
  for (i = varying_map.begin(); i != varying_map.end(); ++i) {
    CGparameter cg_param = i->first;
    const char* semantic_string = cgGetParameterSemantic(cg_param);
    int attr = SemanticNameToGLVertexAttribute(semantic_string);
    int index = 0;
    Stream::Semantic semantic = GLVertexAttributeToStream(attr, &index);
    int stream_index = FindVertexStream(semantic, index);
    if (stream_index >= 0) {
      // record the matched stream into the varying parameter map for later
      // use by StreamBankGL::Draw().
      i->second = stream_index;
      DLOG(INFO)
          << "StreamBankGL Matched CG_PARAMETER \""
          << cgGetParameterName(cg_param) << " : "
          << semantic_string << "\" to stream "
          << stream_index << " \""
          << vertex_stream_params_.at(
              stream_index)->stream().field().buffer()->name()
          << "\"";
    } else {
      // no matching stream was found.
      *missing_semantic = semantic;
      *missing_semantic_index = index;
      return false;
    }
  }
  CHECK_GL_ERROR();
  return true;
}

bool StreamBankGL::BindStreamsForRendering(
    const ParamCacheGL::VaryingParameterMap& varying_map,
    unsigned int* max_vertices) {
  *max_vertices = UINT_MAX;
  // Loop over varying params setting up the streams.
  ParamCacheGL::VaryingParameterMap::const_iterator i;
  for (i = varying_map.begin(); i != varying_map.end(); ++i) {
    const Stream& stream = vertex_stream_params_.at(i->second)->stream();
    const Field& field = stream.field();
    GLenum type = GLDataType(field);
    if (type == GL_INVALID_ENUM) {
      // TODO: support other kinds of buffers.
      O3D_ERROR(service_locator())
          << "unsupported field of type '" << field.GetClassName()
          << "' on StreamBank '" << name() << "'";
      return false;
    }
    VertexBufferGL *vbuffer = down_cast<VertexBufferGL*>(field.buffer());
    if (!vbuffer) {
      O3D_ERROR(service_locator())
          << "stream has no buffer in StreamBank '" << name() << "'";
      return false;
    }
    // TODO support all data types and packings here. Currently it
    // only supports GL_FLOAT buffers, but buffers of GL_HALF and GL_INT are
    // also possible as streamed parameter inputs.
    GLint element_count = field.num_components();
    if (element_count > 4) {
      element_count = 0;
      DLOG_FIRST_N(ERROR, kNumLoggedEvents)
          << "Unable to find stream for CGparameter: "
          << cgGetParameterName(i->first);
    }

    // In the num_elements = 1 case we want to do the D3D stride = 0 thing.
    // but see below.
    if (vbuffer->num_elements() == 1) {
      // TODO: passing a stride of 0 has a different meaning in GL
      // (compute a stride as if it was packed) than in DX (re-use the vertex
      // over and over again). The equivalent of the DX behavior is by
      // disabling the vertex array, and setting a constant value. Currently,
      // this just avoids de-referencing outside of the vertex buffer, but it
      // doesn't set the proper value: we'd need to map the buffer, get the
      // value, and unmap it (slow !!). A better solution is to disallow 0
      // stride at the API level, and instead maybe provide a way to pss a
      // constant value - but the DX version relies on being able to pass a 0
      // stride, so the whole thing needs a bit of rewrite.
      cgGLDisableClientState(i->first);
    } else {
      glBindBufferARB(GL_ARRAY_BUFFER, vbuffer->gl_buffer());
      cgGLSetParameterPointer(i->first,
                              element_count,
                              GLDataType(field),
                              vbuffer->stride(),
                              BUFFER_OFFSET(field.offset()));
      cgGLEnableClientState(i->first);
      *max_vertices = std::min(*max_vertices, stream.GetMaxVertices());
    }
  }
  return true;
}

// private member functions ----------------------------------------------------

// Searches the array of streams and returns the index of the stream that
// matches the semantic and index pair. if no match was found, return "-1"
int StreamBankGL::FindVertexStream(Stream::Semantic semantic, int index) {
  for (unsigned ii = 0; ii < vertex_stream_params_.size(); ++ii) {
    const Stream& stream = vertex_stream_params_[ii]->stream();
    if (stream.semantic() == semantic && stream.semantic_index() == index) {
      return static_cast<int>(ii);
    }
  }
  return -1;
}

}  // namespace o3d
