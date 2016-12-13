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


// This file contains definitions of handy methods and macros used by the
// O3D DirectX code.

#ifndef O3D_CORE_WIN_D3D9_UTILS_D3D9_H_
#define O3D_CORE_WIN_D3D9_UTILS_D3D9_H_

#include <d3d9.h>
#include <dxerr9.h>
#include "base/logging.h"
#include "core/cross/stream.h"

#define HR(x) ::o3d::VerifyHResult((x), __FILE__, __LINE__, #x)

namespace o3d {

class Field;

inline bool VerifyHResult(HRESULT hr, const char* file, int line,
                          const char* call) {
  if (FAILED(hr)) {
    DLOG(ERROR) << "DX Error in file " << file
                << " line " << line << L": "
                << DXGetErrorString9(hr) << L": " << call;
    return false;
  }
  return true;
}

union Float2DWord {
  float my_float;
  DWORD my_dword;
};

D3DDECLTYPE DX9DataType(const Field& stream);
D3DDECLUSAGE DX9UsageType(Stream::Semantic semantic);
Stream::Semantic SemanticFromDX9UsageType(D3DDECLUSAGE usage);

}  // namespace o3d

#endif  // O3D_CORE_WIN_D3D9_UTILS_D3D9_H_
