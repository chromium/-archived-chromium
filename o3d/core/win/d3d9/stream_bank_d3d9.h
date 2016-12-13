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


// This file contains the declaration of the StreamBankD3D9 class.

#ifndef O3D_CORE_WIN_D3D9_STREAM_BANK_D3D9_H_
#define O3D_CORE_WIN_D3D9_STREAM_BANK_D3D9_H_

#include <d3d9.h>
#include "core/cross/stream_bank.h"
#include "core/win/d3d9/effect_d3d9.h"

namespace o3d {

class EffectD3D9;

// StreamBankD3D9 is the DX9 implementation of the StreamBank.  It provides the
// necessary interfaces for setting the geometry streams on the StreamBank.
class StreamBankD3D9 : public StreamBank {
 public:
  explicit StreamBankD3D9(ServiceLocator* service_locator,
                          IDirect3DDevice9* d3d_device);
  ~StreamBankD3D9();

  // Sets the streams for rendering.
  // Parameter:
  //   max_vertrices: pointer to variable to receive the maximum vertices
  //     the streams can render.
  // Returns:
  //   True if all the streams are available.
  bool BindStreamsForRendering(unsigned int* max_vertices);

  // Checks for all required streams before rendering.
  bool CheckForMissingVertexStreams(EffectD3D9* effect,
                                    Stream::Semantic* missing_semantic,
                                    int* missing_semantic_index);

 protected:
  // Overridden from StreamBank.
  virtual void OnUpdateStreams();

 private:

  // Frees the vertex declaration if there is one.
  void FreeVertexDeclaration();

  IDirect3DDevice9* d3d_device_;
  IDirect3DVertexDeclaration9* vertex_declaration_;
};
}  // o3d

#endif  // O3D_CORE_WIN_D3D9_STREAM_BANK_D3D9_H_
