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


// This file contains the declaration of the SamplerD3D9 class.

#ifndef O3D_CORE_WIN_D3D9_SAMPLER_D3D9_H_
#define O3D_CORE_WIN_D3D9_SAMPLER_D3D9_H_

#include <atlbase.h>
#include <d3d9.h>
#include "core/cross/sampler.h"

namespace o3d {

class RendererD3D9;

// Sampler2DD3D9 is an implementation of the Sampler object for D3D9.
class SamplerD3D9 : public Sampler {
 public:
  SamplerD3D9(ServiceLocator* service_locator, IDirect3DDevice9* d3d_device);
  virtual ~SamplerD3D9();

  // Sets the d3d texture and sampler states for the given sampler unit.
  void SetTextureAndStates(int sampler_unit);

  // Sets the d3d texture to NULL.
  void ResetTexture(int sampler_unit);

 private:
  void SetAddressMode(int sampler_unit,
                      D3DSAMPLERSTATETYPE sampler_type,
                      Sampler::AddressMode o3d_mode,
                      D3DTEXTUREADDRESS default_mode);

  RendererD3D9* renderer_;
  CComPtr<IDirect3DDevice9> d3d_device_;

  DISALLOW_COPY_AND_ASSIGN(SamplerD3D9);
};

}  // namespace o3d

#endif  // O3D_CORE_WIN_D3D9_SAMPLER_D3D9_H_
