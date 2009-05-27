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


// This file contains the definition of the SamplerD3D9 class, implementing
// samplers for D3D.

#ifndef O3D_COMMAND_BUFFER_SERVICE_WIN_D3D9_SAMPLER_D3D9_H_
#define O3D_COMMAND_BUFFER_SERVICE_WIN_D3D9_SAMPLER_D3D9_H_

#include "command_buffer/common/cross/gapi_interface.h"
#include "command_buffer/service/win/d3d9/d3d9_utils.h"
#include "command_buffer/service/cross/resource.h"

namespace o3d {
namespace command_buffer {

class GAPID3D9;

// D3D9 version of Sampler.
class SamplerD3D9 : public Sampler {
 public:
  SamplerD3D9();

  // Applies sampler states to D3D.
  bool ApplyStates(GAPID3D9 *gapi, unsigned int unit) const;

  // Sets sampler states.
  void SetStates(sampler::AddressingMode addressing_u,
                 sampler::AddressingMode addressing_v,
                 sampler::AddressingMode addressing_w,
                 sampler::FilteringMode mag_filter,
                 sampler::FilteringMode min_filter,
                 sampler::FilteringMode mip_filter,
                 unsigned int max_anisotropy);

  // Sets the border color states.
  void SetBorderColor(const RGBA &color);

  // Sets the texture.
  void SetTexture(ResourceID texture) { texture_id_ = texture; }
 private:
  D3DTEXTUREADDRESS d3d_address_u_;
  D3DTEXTUREADDRESS d3d_address_v_;
  D3DTEXTUREADDRESS d3d_address_w_;
  D3DTEXTUREFILTERTYPE d3d_mag_filter_;
  D3DTEXTUREFILTERTYPE d3d_min_filter_;
  D3DTEXTUREFILTERTYPE d3d_mip_filter_;
  DWORD d3d_max_anisotropy_;
  D3DCOLOR d3d_border_color_;
  ResourceID texture_id_;
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SERVICE_WIN_D3D9_SAMPLER_D3D9_H_
