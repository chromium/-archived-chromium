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


// This file contains the implementation of the SamplerD3D9 class.

#include "command_buffer/service/win/d3d9/gapi_d3d9.h"
#include "command_buffer/service/win/d3d9/sampler_d3d9.h"
#include "command_buffer/service/win/d3d9/texture_d3d9.h"

namespace o3d {
namespace command_buffer {

namespace {

// Converts an addressing mode to corresponding D3D values.
D3DTEXTUREADDRESS AddressModeToD3D(sampler::AddressingMode mode) {
  switch (mode) {
    case sampler::WRAP:
      return D3DTADDRESS_WRAP;
    case sampler::MIRROR_REPEAT:
      return D3DTADDRESS_MIRROR;
    case sampler::CLAMP_TO_EDGE:
      return D3DTADDRESS_CLAMP;
    case sampler::CLAMP_TO_BORDER:
      return D3DTADDRESS_BORDER;
  }
  DLOG(FATAL) << "Not reached";
  return D3DTADDRESS_WRAP;
}

// Converts a filtering mode to corresponding D3D values.
D3DTEXTUREFILTERTYPE FilteringModeToD3D(sampler::FilteringMode mode) {
  switch (mode) {
    case sampler::NONE:
      return D3DTEXF_NONE;
    case sampler::POINT:
      return D3DTEXF_POINT;
    case sampler::LINEAR:
      return D3DTEXF_LINEAR;
  }
  DLOG(FATAL) << "Not reached";
  return D3DTEXF_POINT;
}

}  // anonymous namespace

SamplerD3D9::SamplerD3D9()
    : texture_id_(kInvalidResource) {
  SetStates(sampler::CLAMP_TO_EDGE,
            sampler::CLAMP_TO_EDGE,
            sampler::CLAMP_TO_EDGE,
            sampler::LINEAR,
            sampler::LINEAR,
            sampler::POINT,
            1);
  RGBA black = {0, 0, 0, 1};
  SetBorderColor(black);
}

bool SamplerD3D9::ApplyStates(GAPID3D9 *gapi, unsigned int unit) const {
  DCHECK(gapi);
  TextureD3D9 *texture = gapi->GetTexture(texture_id_);
  if (!texture) {
    return false;
  }
  IDirect3DDevice9 *d3d_device = gapi->d3d_device();
  HR(d3d_device->SetTexture(unit, texture->d3d_base_texture()));
  HR(d3d_device->SetSamplerState(unit, D3DSAMP_ADDRESSU, d3d_address_u_));
  HR(d3d_device->SetSamplerState(unit, D3DSAMP_ADDRESSV, d3d_address_v_));
  HR(d3d_device->SetSamplerState(unit, D3DSAMP_ADDRESSW, d3d_address_w_));
  HR(d3d_device->SetSamplerState(unit, D3DSAMP_MAGFILTER, d3d_mag_filter_));
  HR(d3d_device->SetSamplerState(unit, D3DSAMP_MINFILTER, d3d_min_filter_));
  HR(d3d_device->SetSamplerState(unit, D3DSAMP_MIPFILTER, d3d_mip_filter_));
  HR(d3d_device->SetSamplerState(unit, D3DSAMP_MAXANISOTROPY,
                                 d3d_max_anisotropy_));
  HR(d3d_device->SetSamplerState(unit, D3DSAMP_BORDERCOLOR, d3d_border_color_));
  return true;
}

void SamplerD3D9::SetStates(sampler::AddressingMode addressing_u,
                            sampler::AddressingMode addressing_v,
                            sampler::AddressingMode addressing_w,
                            sampler::FilteringMode mag_filter,
                            sampler::FilteringMode min_filter,
                            sampler::FilteringMode mip_filter,
                            unsigned int max_anisotropy) {
  // These are validated in GAPIDecoder.cc
  DCHECK_NE(mag_filter, sampler::NONE);
  DCHECK_NE(min_filter, sampler::NONE);
  DCHECK_GT(max_anisotropy, 0);
  d3d_address_u_ = AddressModeToD3D(addressing_u);
  d3d_address_v_ = AddressModeToD3D(addressing_v);
  d3d_address_w_ = AddressModeToD3D(addressing_w);
  d3d_mag_filter_ = FilteringModeToD3D(mag_filter);
  d3d_min_filter_ = FilteringModeToD3D(min_filter);
  d3d_mip_filter_ = FilteringModeToD3D(mip_filter);
  if (max_anisotropy > 1) {
    d3d_mag_filter_ = D3DTEXF_ANISOTROPIC;
    d3d_min_filter_ = D3DTEXF_ANISOTROPIC;
  }
  d3d_max_anisotropy_ = max_anisotropy;
}

void SamplerD3D9::SetBorderColor(const RGBA &color) {
  d3d_border_color_ = RGBAToD3DCOLOR(color);
}

BufferSyncInterface::ParseError GAPID3D9::CreateSampler(
    ResourceID id) {
  // Dirty effect, because this sampler id may be used
  DirtyEffect();
  samplers_.Assign(id, new SamplerD3D9());
  return BufferSyncInterface::PARSE_NO_ERROR;
}

// Destroys the Sampler resource.
BufferSyncInterface::ParseError GAPID3D9::DestroySampler(ResourceID id) {
  // Dirty effect, because this sampler id may be used
  DirtyEffect();
  return samplers_.Destroy(id) ?
      BufferSyncInterface::PARSE_NO_ERROR :
      BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
}

BufferSyncInterface::ParseError GAPID3D9::SetSamplerStates(
    ResourceID id,
    sampler::AddressingMode addressing_u,
    sampler::AddressingMode addressing_v,
    sampler::AddressingMode addressing_w,
    sampler::FilteringMode mag_filter,
    sampler::FilteringMode min_filter,
    sampler::FilteringMode mip_filter,
    unsigned int max_anisotropy) {
  SamplerD3D9 *sampler = samplers_.Get(id);
  if (!sampler)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  // Dirty effect, because this sampler id may be used
  DirtyEffect();
  sampler->SetStates(addressing_u, addressing_v, addressing_w,
                     mag_filter, min_filter, mip_filter, max_anisotropy);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPID3D9::SetSamplerBorderColor(
    ResourceID id,
    const RGBA &color) {
  SamplerD3D9 *sampler = samplers_.Get(id);
  if (!sampler)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  // Dirty effect, because this sampler id may be used
  DirtyEffect();
  sampler->SetBorderColor(color);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPID3D9::SetSamplerTexture(
    ResourceID id,
    ResourceID texture_id) {
  SamplerD3D9 *sampler = samplers_.Get(id);
  if (!sampler)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  // Dirty effect, because this sampler id may be used
  DirtyEffect();
  sampler->SetTexture(texture_id);
  return BufferSyncInterface::PARSE_NO_ERROR;
}


}  // namespace command_buffer
}  // namespace o3d
