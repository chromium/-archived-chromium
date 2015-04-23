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


// This file defines a few utilities for Direct3D.

#ifndef O3D_COMMAND_BUFFER_SERVICE_WIN_D3D9_D3D9_UTILS_H__
#define O3D_COMMAND_BUFFER_SERVICE_WIN_D3D9_D3D9_UTILS_H__

#ifndef NOMINMAX
// windows.h defines min() and max() as macros, conflicting with std::min and
// std::max unless NOMINMAX is defined.
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <dxerr.h>
#include <algorithm>
#include "command_buffer/common/cross/gapi_interface.h"

#if defined (_DEBUG)

#ifndef HR
#define HR(x) {                                                              \
    HRESULT hr = x;                                                          \
    if (FAILED(hr)) {                                                        \
      LOG(ERROR) << "DirectX error at " << __FILE__ << ":" << __LINE__       \
                 << " when calling " << #x << ": " << DXGetErrorStringA(hr); \
    }                                                                        \
  }
#endif

#else  // _DEBUG

#ifndef HR
#define HR(x) x;
#endif

#endif  // _DEBUG

namespace o3d {
namespace command_buffer {

union FloatAndDWORD {
  float float_value;
  DWORD dword_value;
};

// Bit casts a float into a DWORD. That's what D3D expects for some values.
inline DWORD FloatAsDWORD(float value) {
  volatile FloatAndDWORD float_and_dword;
  float_and_dword.float_value = value;
  return float_and_dword.dword_value;
}

// Clamps a float to [0 .. 1] and maps it to [0 .. 255]
inline unsigned int FloatToClampedByte(float value) {
  value = std::min(1.f, std::max(0.f, value));
  return static_cast<unsigned int>(value * 255);
}

// Converts a RGBA color into a D3DCOLOR
inline D3DCOLOR RGBAToD3DCOLOR(const RGBA &color) {
  return D3DCOLOR_RGBA(FloatToClampedByte(color.red),
                       FloatToClampedByte(color.green),
                       FloatToClampedByte(color.blue),
                       FloatToClampedByte(color.alpha));
}

inline bool VerifyHResult(HRESULT hr, const char* file, int line,
                          const char* call) {
  if (FAILED(hr)) {
    DLOG(ERROR) << "DX Error in file " << file
                << " line " << line << L": "
                << DXGetErrorStringA(hr) << L": " << call;
    return false;
  }
  return true;
}

static bool D3DSemanticToCBSemantic(
    D3DDECLUSAGE semantic,
    unsigned int semantic_index,
    vertex_struct::Semantic *out_semantic,
    unsigned int *out_semantic_index) {
  // TODO: what meaning do we really want to put to our semantics ? How
  // do they match the semantics that are set in the effect ? What combination
  // of (semantic, index) are supposed to work ?
  switch (semantic) {
    case D3DDECLUSAGE_POSITION:
      if (semantic_index != 0) return false;
      *out_semantic = vertex_struct::POSITION;
      *out_semantic_index = 0;
      return true;
    case D3DDECLUSAGE_NORMAL:
      if (semantic_index != 0) return false;
      *out_semantic = vertex_struct::NORMAL;
      *out_semantic_index = 0;
      return true;
    case D3DDECLUSAGE_TANGENT:
      if (semantic_index != 0) return false;
      *out_semantic = vertex_struct::TEX_COORD;
      *out_semantic_index = 6;
      return true;
    case D3DDECLUSAGE_BINORMAL:
      if (semantic_index != 0) return false;
      *out_semantic = vertex_struct::TEX_COORD;
      *out_semantic_index = 7;
      return true;
    case D3DDECLUSAGE_COLOR:
      if (semantic_index > 1) return false;
      *out_semantic = vertex_struct::COLOR;
      *out_semantic_index = semantic_index;
      return true;
    case D3DDECLUSAGE_TEXCOORD:
      *out_semantic = vertex_struct::TEX_COORD;
      *out_semantic_index = semantic_index;
      return true;
    default:
      return false;
  }
}
}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SERVICE_WIN_D3D9_D3D9_UTILS_H__
