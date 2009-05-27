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


// This file contains the implementation of the SamplerCB class.

#include "core/cross/precompile.h"
#include "core/cross/error.h"
#include "core/cross/command_buffer/sampler_cb.h"
#include "core/cross/command_buffer/renderer_cb.h"
#include "command_buffer/common/cross/cmd_buffer_format.h"
#include "command_buffer/client/cross/cmd_buffer_helper.h"

namespace o3d {

using command_buffer::CommandBufferEntry;
using command_buffer::CommandBufferHelper;
using command_buffer::ResourceID;
namespace set_sampler_states = command_buffer::set_sampler_states;
namespace sampler = command_buffer::sampler;

namespace {

sampler::AddressingMode AddressModeToCB(Sampler::AddressMode o3d_mode) {
  switch (o3d_mode) {
    case Sampler::WRAP:
      return sampler::WRAP;
    case Sampler::MIRROR:
      return sampler::MIRROR_REPEAT;
    case Sampler::CLAMP:
      return sampler::CLAMP_TO_EDGE;
    case Sampler::BORDER:
      return sampler::CLAMP_TO_BORDER;
    default:
      DLOG(ERROR) << "Unknown Address mode " << static_cast<int>(o3d_mode);
      return sampler::WRAP;
  }
}

sampler::FilteringMode FilterTypeToCB(Sampler::FilterType o3d_mode) {
  switch (o3d_mode) {
    case Sampler::NONE:
      return sampler::NONE;
    case Sampler::POINT:
      return sampler::POINT;
    case Sampler::LINEAR:
    case Sampler::ANISOTROPIC:
      return sampler::LINEAR;
    default:
      return sampler::NONE;
  }
}

}  // anonymous namespace

SamplerCB::SamplerCB(ServiceLocator* service_locator, RendererCB* renderer)
    : Sampler(service_locator),
      renderer_(renderer) {
  DCHECK(renderer_);
  resource_id_ = renderer_->sampler_ids().AllocateID();
  CommandBufferEntry args[1];
  args[0].value_uint32 = resource_id_;
  renderer_->helper()->AddCommand(command_buffer::CREATE_SAMPLER, 1, args);
}

SamplerCB::~SamplerCB() {
  CommandBufferEntry args[1];
  args[0].value_uint32 = resource_id_;
  renderer_->helper()->AddCommand(command_buffer::DESTROY_SAMPLER, 1, args);
  renderer_->sampler_ids().FreeID(resource_id_);
}

void SamplerCB::SetTextureAndStates() {
  CommandBufferHelper *helper = renderer_->helper();
  sampler::AddressingMode address_mode_u_cb = AddressModeToCB(address_mode_u());
  sampler::AddressingMode address_mode_v_cb = AddressModeToCB(address_mode_v());
  sampler::FilteringMode mag_filter_cb = FilterTypeToCB(mag_filter());
  sampler::FilteringMode min_filter_cb = FilterTypeToCB(min_filter());
  sampler::FilteringMode mip_filter_cb = FilterTypeToCB(mip_filter());
  if (mag_filter_cb == sampler::NONE) mag_filter_cb = sampler::POINT;
  if (min_filter_cb == sampler::NONE) min_filter_cb = sampler::POINT;
  int max_max_anisotropy = set_sampler_states::MaxAnisotropy::kMask;
  unsigned int max_anisotropy_cb =
      std::max(1, std::min(max_max_anisotropy, max_anisotropy()));
  if (min_filter() != Sampler::ANISOTROPIC) {
    max_anisotropy_cb = 1;
  }
  CommandBufferEntry args[5];
  args[0].value_uint32 = resource_id_;
  args[1].value_uint32 =
      set_sampler_states::AddressingU::MakeValue(address_mode_u_cb) |
      set_sampler_states::AddressingV::MakeValue(address_mode_v_cb) |
      set_sampler_states::AddressingW::MakeValue(sampler::WRAP) |
      set_sampler_states::MagFilter::MakeValue(mag_filter_cb) |
      set_sampler_states::MinFilter::MakeValue(min_filter_cb) |
      set_sampler_states::MipFilter::MakeValue(mip_filter_cb) |
      set_sampler_states::MaxAnisotropy::MakeValue(max_anisotropy_cb);
  helper->AddCommand(command_buffer::SET_SAMPLER_STATES, 2, args);

  Float4 color = border_color();
  args[1].value_float = color[0];
  args[2].value_float = color[1];
  args[3].value_float = color[2];
  args[4].value_float = color[3];
  helper->AddCommand(command_buffer::SET_SAMPLER_BORDER_COLOR, 5, args);

  Texture *texture_object = texture();
  if (!texture_object) {
    texture_object = renderer_->error_texture();
    if (!texture_object) {
      O3D_ERROR(service_locator())
          << "Missing texture for sampler " << name();
      texture_object = renderer_->fallback_error_texture();
    }
  }

  if (texture_object) {
    args[1].value_uint32 =
        reinterpret_cast<ResourceID>(texture_object->GetTextureHandle());
    helper->AddCommand(command_buffer::SET_SAMPLER_TEXTURE, 2, args);
  }
}

}  // namespace o3d
