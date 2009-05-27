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


// This file contains the definition of SamplerD3D9.

#include "core/cross/precompile.h"

#include "core/win/d3d9/sampler_d3d9.h"

#include "core/cross/error.h"
#include "core/cross/renderer.h"
#include "core/cross/renderer_platform.h"
#include "core/win/d3d9/utils_d3d9.h"

namespace o3d {

SamplerD3D9::SamplerD3D9(ServiceLocator* service_locator,
                         IDirect3DDevice9* d3d_device)
    : Sampler(service_locator),
      renderer_(static_cast<RendererD3D9*>(
          service_locator->GetService<Renderer>())),
      d3d_device_(d3d_device) {
  DCHECK(d3d_device);
}

SamplerD3D9::~SamplerD3D9() {
}

void SamplerD3D9::SetAddressMode(int sampler_unit,
                                 D3DSAMPLERSTATETYPE sampler_type,
                                 Sampler::AddressMode o3d_mode,
                                 D3DTEXTUREADDRESS default_mode) {
  D3DTEXTUREADDRESS d3d_mode = default_mode;
  switch (o3d_mode) {
    case WRAP:
      d3d_mode = D3DTADDRESS_WRAP;
      break;
    case MIRROR:
      d3d_mode = D3DTADDRESS_MIRROR;
      break;
    case CLAMP:
      d3d_mode = D3DTADDRESS_CLAMP;
      break;
    case BORDER:
      d3d_mode = D3DTADDRESS_BORDER;
      break;
    default:
      DLOG(ERROR) << "Unknown Address mode " << static_cast<int>(o3d_mode);
      return;
  }

  HR(d3d_device_->SetSamplerState(sampler_unit, sampler_type, d3d_mode));
}

namespace {

D3DTEXTUREFILTERTYPE D3DMagFilter(Sampler::FilterType o3d_mag_filter) {
  // For mag filters only POINT and LINEAR make sense.  Everything else
  // we convert to LINEAR.
  switch (o3d_mag_filter) {
    case Sampler::POINT:
      return D3DTEXF_POINT;
    case Sampler::LINEAR:
      return D3DTEXF_LINEAR;
    default:
      return D3DTEXF_LINEAR;
  }
}

D3DTEXTUREFILTERTYPE D3DMinFilter(Sampler::FilterType o3d_mag_filter) {
  // Allowable min filters are POINT, LINEAR and ANISOTROPIC
  switch (o3d_mag_filter) {
    case Sampler::POINT:
      return D3DTEXF_POINT;
    case Sampler::LINEAR:
      return D3DTEXF_LINEAR;
    case Sampler::ANISOTROPIC:
      return D3DTEXF_ANISOTROPIC;
    default:
      return D3DTEXF_LINEAR;
  }
}

D3DTEXTUREFILTERTYPE D3DMipFilter(Sampler::FilterType o3d_mag_filter) {
  // Allowable mip filters are NONE, POINT and LINEAR
  switch (o3d_mag_filter) {
    case Sampler::NONE:
      return D3DTEXF_NONE;
    case Sampler::POINT:
      return D3DTEXF_POINT;
    case Sampler::LINEAR:
      return D3DTEXF_LINEAR;
    default:
      return D3DTEXF_LINEAR;
  }
}

}  // namespace

void SamplerD3D9::SetTextureAndStates(int sampler_unit) {
  DLOG_ASSERT(d3d_device_);

  // First get the d3d texture and set it.
  Texture* texture_object = texture();
  if (!texture_object) {
    texture_object = renderer_->error_texture();
    if (!texture_object) {
      O3D_ERROR(service_locator())
          << "Missing texture for sampler " << name();
      texture_object = renderer_->fallback_error_texture();
    }
  }

  IDirect3DBaseTexture9* d3d_texture =
      static_cast<IDirect3DBaseTexture9*>(texture_object->GetTextureHandle());

  HR(d3d_device_->SetTexture(sampler_unit, d3d_texture));

  SetAddressMode(sampler_unit,
                 D3DSAMP_ADDRESSU,
                 address_mode_u(),
                 D3DTADDRESS_WRAP);

  SetAddressMode(sampler_unit,
                 D3DSAMP_ADDRESSV,
                 address_mode_v(),
                 D3DTADDRESS_WRAP);

  if (texture_object->IsA(TextureCUBE::GetApparentClass())) {
    SetAddressMode(sampler_unit,
                   D3DSAMP_ADDRESSW,
                   address_mode_w(),
                   D3DTADDRESS_WRAP);
  }

  HR(d3d_device_->SetSamplerState(sampler_unit,
                                  D3DSAMP_MAGFILTER,
                                  D3DMagFilter(mag_filter())));

  HR(d3d_device_->SetSamplerState(sampler_unit,
                                  D3DSAMP_MINFILTER,
                                  D3DMinFilter(min_filter())));

  HR(d3d_device_->SetSamplerState(sampler_unit,
                                  D3DSAMP_MIPFILTER,
                                  D3DMipFilter(mip_filter())));

  Float4 color = border_color();
  DWORD d3d_color = D3DCOLOR_COLORVALUE(color[0], color[1], color[2], color[3]);
  HR(d3d_device_->SetSamplerState(sampler_unit,
                                  D3DSAMP_BORDERCOLOR,
                                  d3d_color));

  HR(d3d_device_->SetSamplerState(sampler_unit,
                                  D3DSAMP_MAXANISOTROPY,
                                  max_anisotropy()));
}

void SamplerD3D9::ResetTexture(int sampler_unit) {
  DLOG_ASSERT(d3d_device_);

  HR(d3d_device_->SetTexture(sampler_unit, NULL));
}

}  // namespace o3d
