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


// This file contains definitions of handy methods used by the
// O3D DirectX code.

#include "core/cross/precompile.h"
#include "core/win/d3d9/utils_d3d9.h"

#include "core/cross/field.h"

namespace o3d {

// Converts from a Field datatype to a suitable dx9 type
D3DDECLTYPE DX9DataType(const Field& field) {
  if (field.IsA(FloatField::GetApparentClass())) {
    switch (field.num_components()) {
      case 1:
        return D3DDECLTYPE_FLOAT1;
      case 2:
        return D3DDECLTYPE_FLOAT2;
      case 3:
        return D3DDECLTYPE_FLOAT3;
      case 4:
        return D3DDECLTYPE_FLOAT4;
    }
  } else if (field.IsA(UByteNField::GetApparentClass())) {
    switch (field.num_components()) {
      case 4:
        return D3DDECLTYPE_D3DCOLOR;
    }
  }
  DLOG(ERROR) << "Unknown Stream DataType";
  return D3DDECLTYPE_UNUSED;
}

// Converts from Stream::Semantic to a suitable dx9 usage type.
D3DDECLUSAGE DX9UsageType(Stream::Semantic semantic) {
  switch (semantic) {
    case Stream::POSITION:
      return D3DDECLUSAGE_POSITION;
      break;
    case Stream::NORMAL:
      return D3DDECLUSAGE_NORMAL;
      break;
    case Stream::TANGENT:
      return D3DDECLUSAGE_TANGENT;
      break;
    case Stream::BINORMAL:
      return D3DDECLUSAGE_BINORMAL;
      break;
    case Stream::COLOR:
      return D3DDECLUSAGE_COLOR;
      break;
    case Stream::TEXCOORD:
      return D3DDECLUSAGE_TEXCOORD;
      break;
  }
  DLOG(ERROR) << "Unknown DX9 Usage Type";
  return D3DDECLUSAGE_SAMPLE;
}

// Convertes a dx9 semantic to a matching Stream semantic.
Stream::Semantic SemanticFromDX9UsageType(D3DDECLUSAGE usage) {
  switch (usage) {
    case D3DDECLUSAGE_POSITION:
      return Stream::POSITION;
      break;
    case D3DDECLUSAGE_NORMAL:
      return Stream::NORMAL;
      break;
    case D3DDECLUSAGE_TANGENT:
      return Stream::TANGENT;
      break;
    case D3DDECLUSAGE_BINORMAL:
      return Stream::BINORMAL;
      break;
    case D3DDECLUSAGE_COLOR:
      return Stream::COLOR;
      break;
    case D3DDECLUSAGE_TEXCOORD:
      return Stream::TEXCOORD;
      break;
    default:
      DLOG(ERROR) << "Unknown DX9 semantic type";
      return Stream::UNKNOWN_SEMANTIC;
  }
}

}  // namespace o3d
