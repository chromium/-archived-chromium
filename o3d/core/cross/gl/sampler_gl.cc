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


// This file contains the implementation for SamplerGL.

// Precompiled header comes before everything else.
#include "core/cross/precompile.h"
#include "core/cross/error.h"
#include "core/cross/gl/renderer_gl.h"
#include "core/cross/gl/sampler_gl.h"

namespace o3d {

SamplerGL::SamplerGL(ServiceLocator* service_locator)
    : Sampler(service_locator),
      renderer_(static_cast<RendererGL*>(
          service_locator->GetService<Renderer>())) {
}

SamplerGL::~SamplerGL() {
}

namespace {

unsigned int GLAddressMode(Sampler::AddressMode o3d_mode,
                           unsigned int default_mode) {
  unsigned int gl_mode = default_mode;
  switch (o3d_mode) {
    case Sampler::WRAP:
      gl_mode = GL_REPEAT;
      break;
    case Sampler::MIRROR:
      gl_mode = GL_MIRRORED_REPEAT;
      break;
    case Sampler::CLAMP:
      gl_mode = GL_CLAMP_TO_EDGE;
      break;
    case Sampler::BORDER:
      gl_mode = GL_CLAMP_TO_BORDER;
      break;
    default:
      DLOG(ERROR) << "Unknown Address mode " << static_cast<int>(o3d_mode);
      break;
  }
  return gl_mode;
}

unsigned int GLMinFilter(Sampler::FilterType o3d_filter,
                         Sampler::FilterType mip_filter) {
  switch (o3d_filter) {
    case Sampler::POINT:
      if (mip_filter == Sampler::NONE)
        return GL_NEAREST;
      else if (mip_filter == Sampler::POINT)
        return GL_NEAREST_MIPMAP_NEAREST;
      else if (mip_filter == Sampler::LINEAR)
        return GL_NEAREST_MIPMAP_LINEAR;
    default:
      DLOG(ERROR) << "Unknown filter " << static_cast<int>(o3d_filter);
      // fall through
    case Sampler::ANISOTROPIC:  // Anisotropy is handled in SetTextureAndStates
    case Sampler::LINEAR:
      if (mip_filter == Sampler::NONE)
        return GL_LINEAR;
      else if (mip_filter == Sampler::POINT)
        return GL_LINEAR_MIPMAP_NEAREST;
      else if (mip_filter == Sampler::LINEAR)
        return GL_LINEAR_MIPMAP_LINEAR;
  }
}

unsigned int GLMagFilter(Sampler::FilterType o3d_filter) {
  switch (o3d_filter) {
    case Sampler::POINT:
      return GL_NEAREST;
    case Sampler::LINEAR:
      return GL_LINEAR;
    default:
      DLOG(ERROR) << "Unknown filter " << static_cast<int>(o3d_filter);
      return GL_LINEAR;
  }
}

GLenum GLTextureTarget(Texture* texture) {
  if (texture->IsA(Texture2D::GetApparentClass())) {
    return GL_TEXTURE_2D;
  } else if (texture->IsA(TextureCUBE::GetApparentClass())) {
    return GL_TEXTURE_CUBE_MAP;
  } else {
    DLOG(ERROR) << "Unknown texture target";
    return 0;
  }
}

}  // namespace

void SamplerGL::SetTextureAndStates(CGparameter cg_param) {
  // Get the texture object associated with this sampler.
  Texture* texture_object = texture();

  if (!texture_object) {
    texture_object = renderer_->error_texture();
    if (!texture_object) {
      O3D_ERROR(service_locator())
          << "Missing texture for sampler " << name();
      texture_object = renderer_->fallback_error_texture();
    }
  }

  GLuint handle = reinterpret_cast<GLuint>(texture_object->GetTextureHandle());
  if (handle) {
    cgGLSetTextureParameter(cg_param, handle);
    cgGLEnableTextureParameter(cg_param);
  } else {
    cgGLSetTextureParameter(cg_param, 0);
    cgGLDisableTextureParameter(cg_param);
    return;
  }

  // TODO: this is a slow check and needs to be moved to initialization
  //     time.
  GLenum target = GLTextureTarget(texture_object);

  if (target) {
    GLenum texture_unit = cgGLGetTextureEnum(cg_param);
    ::glActiveTextureARB(texture_unit);
    glBindTexture(target, handle);
    glTexParameteri(target,
                    GL_TEXTURE_WRAP_S,
                    GLAddressMode(address_mode_u(), GL_REPEAT));
    glTexParameteri(target,
                    GL_TEXTURE_WRAP_T,
                    GLAddressMode(address_mode_v(), GL_REPEAT));
    if (texture_object->IsA(TextureCUBE::GetApparentClass())) {
      glTexParameteri(target,
                      GL_TEXTURE_WRAP_R,
                      GLAddressMode(address_mode_w(), GL_REPEAT));
    }
    glTexParameteri(target,
                    GL_TEXTURE_MIN_FILTER,
                    GLMinFilter(min_filter(), mip_filter()));
    glTexParameteri(target,
                    GL_TEXTURE_MAG_FILTER,
                    GLMagFilter(mag_filter()));

    Float4 color = border_color();
    GLfloat gl_color[4] = {color[0], color[1], color[2], color[3]};
    glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, gl_color);

    // Check for anisotropic texture filtering.
    if (GLEW_EXT_texture_filter_anisotropic) {
      int gl_max_anisotropy =
          (min_filter() == ANISOTROPIC) ? max_anisotropy() : 1;
      glTexParameteri(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_max_anisotropy);
    }
  }
}

void SamplerGL::ResetTexture(CGparameter cg_param) {
  Texture* the_texture = texture();
  if (the_texture) {
    // TODO: this is a slow check and needs to be moved to initialization
    //     time.
    GLenum target = GLTextureTarget(the_texture);
    if (target) {
      GLenum texture_unit = cgGLGetTextureEnum(cg_param);
      glActiveTextureARB(texture_unit);
      glBindTexture(target, 0);
    }
  }
}
}  // namespace o3d
