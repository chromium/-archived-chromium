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


// This file implements the sampler-related GAPI functions on GL.

#include "command_buffer/service/cross/gl/gapi_gl.h"
#include "command_buffer/service/cross/gl/sampler_gl.h"

namespace o3d {
namespace command_buffer {

namespace {

// Gets the GL enum corresponding to an addressing mode.
GLenum GLAddressMode(sampler::AddressingMode o3d_mode) {
  switch (o3d_mode) {
    case sampler::WRAP:
      return GL_REPEAT;
    case sampler::MIRROR_REPEAT:
      return GL_MIRRORED_REPEAT;
    case sampler::CLAMP_TO_EDGE:
      return GL_CLAMP_TO_EDGE;
    case sampler::CLAMP_TO_BORDER:
      return GL_CLAMP_TO_BORDER;
    default:
      DLOG(FATAL) << "Not Reached";
      return GL_REPEAT;
  }
}

// Gets the GL enum for the minification filter based on the command buffer min
// and mip filtering modes.
GLenum GLMinFilter(sampler::FilteringMode min_filter,
                   sampler::FilteringMode mip_filter) {
  switch (min_filter) {
    case sampler::POINT:
      if (mip_filter == sampler::NONE)
        return GL_NEAREST;
      else if (mip_filter == sampler::POINT)
        return GL_NEAREST_MIPMAP_NEAREST;
      else if (mip_filter == sampler::LINEAR)
        return GL_NEAREST_MIPMAP_LINEAR;
    case sampler::LINEAR:
      if (mip_filter == sampler::NONE)
        return GL_LINEAR;
      else if (mip_filter == sampler::POINT)
        return GL_LINEAR_MIPMAP_NEAREST;
      else if (mip_filter == sampler::LINEAR)
        return GL_LINEAR_MIPMAP_LINEAR;
    default:
      DLOG(FATAL) << "Not Reached";
      return GL_LINEAR_MIPMAP_NEAREST;
  }
}

// Gets the GL enum for the magnification filter based on the command buffer mag
// filtering mode.
GLenum GLMagFilter(sampler::FilteringMode mag_filter) {
  switch (mag_filter) {
    case sampler::POINT:
      return GL_NEAREST;
    case sampler::LINEAR:
      return GL_LINEAR;
    default:
      DLOG(FATAL) << "Not Reached";
      return GL_LINEAR;
  }
}

// Gets the GL enum representing the GL target based on the texture type.
GLenum GLTextureTarget(texture::Type type) {
  switch (type) {
    case texture::TEXTURE_2D:
      return GL_TEXTURE_2D;
    case texture::TEXTURE_3D:
      return GL_TEXTURE_3D;
    case texture::TEXTURE_CUBE:
      return GL_TEXTURE_CUBE_MAP;
  }
}

}  // anonymous namespace

SamplerGL::SamplerGL()
    : texture_id_(kInvalidResource),
      gl_texture_(0) {
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

bool SamplerGL::ApplyStates(GAPIGL *gapi) {
  DCHECK(gapi);
  TextureGL *texture = gapi->GetTexture(texture_id_);
  if (!texture) {
    gl_texture_ = 0;
    return false;
  }
  GLenum target = GLTextureTarget(texture->type());
  gl_texture_ = texture->gl_texture();
  glBindTexture(target, gl_texture_);
  glTexParameteri(target, GL_TEXTURE_WRAP_S, gl_wrap_s_);
  glTexParameteri(target, GL_TEXTURE_WRAP_T, gl_wrap_t_);
  glTexParameteri(target, GL_TEXTURE_WRAP_R, gl_wrap_r_);
  glTexParameteri(target, GL_TEXTURE_MIN_FILTER, gl_min_filter_);
  glTexParameteri(target, GL_TEXTURE_MAG_FILTER, gl_mag_filter_);
  glTexParameteri(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_max_anisotropy_);
  glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, gl_border_color_);
  return true;
}

void SamplerGL::SetStates(sampler::AddressingMode addressing_u,
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
  gl_wrap_s_ = GLAddressMode(addressing_u);
  gl_wrap_t_ = GLAddressMode(addressing_v);
  gl_wrap_r_ = GLAddressMode(addressing_w);
  gl_mag_filter_ = GLMagFilter(mag_filter);
  gl_min_filter_ = GLMinFilter(min_filter, mip_filter);
  gl_max_anisotropy_ = max_anisotropy;
}

void SamplerGL::SetBorderColor(const RGBA &color) {
  gl_border_color_[0] = color.red;
  gl_border_color_[1] = color.green;
  gl_border_color_[2] = color.blue;
  gl_border_color_[3] = color.alpha;
}

BufferSyncInterface::ParseError GAPIGL::CreateSampler(
    ResourceID id) {
  // Dirty effect, because this sampler id may be used.
  DirtyEffect();
  samplers_.Assign(id, new SamplerGL());
  return BufferSyncInterface::PARSE_NO_ERROR;
}

// Destroys the Sampler resource.
BufferSyncInterface::ParseError GAPIGL::DestroySampler(ResourceID id) {
  // Dirty effect, because this sampler id may be used.
  DirtyEffect();
  return samplers_.Destroy(id) ?
      BufferSyncInterface::PARSE_NO_ERROR :
      BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
}

BufferSyncInterface::ParseError GAPIGL::SetSamplerStates(
    ResourceID id,
    sampler::AddressingMode addressing_u,
    sampler::AddressingMode addressing_v,
    sampler::AddressingMode addressing_w,
    sampler::FilteringMode mag_filter,
    sampler::FilteringMode min_filter,
    sampler::FilteringMode mip_filter,
    unsigned int max_anisotropy) {
  SamplerGL *sampler = samplers_.Get(id);
  if (!sampler)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  // Dirty effect, because this sampler id may be used.
  DirtyEffect();
  sampler->SetStates(addressing_u, addressing_v, addressing_w,
                     mag_filter, min_filter, mip_filter, max_anisotropy);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPIGL::SetSamplerBorderColor(
    ResourceID id,
    const RGBA &color) {
  SamplerGL *sampler = samplers_.Get(id);
  if (!sampler)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  // Dirty effect, because this sampler id may be used.
  DirtyEffect();
  sampler->SetBorderColor(color);
  return BufferSyncInterface::PARSE_NO_ERROR;
}

BufferSyncInterface::ParseError GAPIGL::SetSamplerTexture(
    ResourceID id,
    ResourceID texture_id) {
  SamplerGL *sampler = samplers_.Get(id);
  if (!sampler)
    return BufferSyncInterface::PARSE_INVALID_ARGUMENTS;
  // Dirty effect, because this sampler id may be used.
  DirtyEffect();
  sampler->SetTexture(texture_id);
  return BufferSyncInterface::PARSE_NO_ERROR;
}


}  // namespace command_buffer
}  // namespace o3d
