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


// This file contains the definition of VertexSource.

#include "core/cross/precompile.h"
#include "core/cross/vertex_source.h"

namespace o3d {

O3D_DEFN_CLASS(VertexSource, ParamObject);

bool VertexSource::BindStream(VertexSource* source,
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

bool VertexSource::UnbindStream(Stream::Semantic semantic, int semantic_index) {
  ParamVertexBufferStream* dest_param = GetVertexStreamParam(
      semantic, semantic_index);
  if (dest_param && dest_param->input_connection() != NULL) {
    dest_param->UnbindInput();
    return true;
  }
  return false;
}

}  // namespace o3d


