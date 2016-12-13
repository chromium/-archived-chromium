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


// This file contains the definition of the Stream class.

#include "core/cross/precompile.h"
#include "core/cross/stream.h"
#include "core/cross/buffer.h"

namespace o3d {

O3D_DEFN_CLASS(Stream, ObjectBase);
O3D_DEFN_CLASS(ParamVertexBufferStream, RefParamBase);

Stream::Stream(ServiceLocator* service_locator,
               Field *field,
               unsigned int start_index,
               Semantic semantic,
               int semantic_index)
    : ObjectBase(service_locator),
      field_(field),
      last_field_change_count_(0),
      start_index_(start_index),
      semantic_(semantic),
      semantic_index_(semantic_index) {
  DCHECK(field);
}

unsigned Stream::GetMaxVertices() const {
  Buffer* buffer = field_->buffer();

  if (!field_->buffer()) {
    return 0;
  }

  unsigned int num_elements = buffer->num_elements();
  if (start_index_ > num_elements) {
    return 0;
  }

  // TODO: If the number of elements is 1 we assume we want to repeat the
  // value (ie, use a stride of 0). We can't do this yet because it's hard to
  // implement in GL.
  // if (num_elements == 1) {
  //   return UINT_MAX;
  // }

  return num_elements - start_index_;
}

const char* Stream::GetSemanticDescription(Stream::Semantic semantic) {
  switch (semantic) {
    case POSITION:
      return "POSITION";
    case NORMAL:
      return "NORMAL";
    case TANGENT:
      return "TANGENT";
    case BINORMAL:
      return "BINORMAL";
    case COLOR:
      return "COLOR";
    case TEXCOORD:
      return "TEXCOORD";
    default:
      return "UNKNOWN";
  }
}

void ParamVertexBufferStream::CopyDataFromParam(Param *source_param) {
  DCHECK(false);  // this should never get called.
}

}  // namespace o3d
