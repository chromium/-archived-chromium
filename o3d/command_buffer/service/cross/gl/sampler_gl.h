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


// This file declares the SamplerGL class.

#ifndef O3D_COMMAND_BUFFER_SERVICE_CROSS_GL_SAMPLER_GL_H_
#define O3D_COMMAND_BUFFER_SERVICE_CROSS_GL_SAMPLER_GL_H_

#include "command_buffer/common/cross/gapi_interface.h"
#include "command_buffer/service/cross/gl/gl_utils.h"
#include "command_buffer/service/cross/resource.h"

namespace o3d {
namespace command_buffer {

class GAPIGL;

// GL version of Sampler.
class SamplerGL : public Sampler {
 public:
  SamplerGL();

  // Applies sampler states to GL.
  bool ApplyStates(GAPIGL *gapi);

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

  GLuint gl_texture() const { return gl_texture_; }

 private:
  GLenum gl_wrap_s_;
  GLenum gl_wrap_t_;
  GLenum gl_wrap_r_;
  GLenum gl_mag_filter_;
  GLenum gl_min_filter_;
  GLuint gl_max_anisotropy_;
  GLfloat gl_border_color_[4];
  GLuint gl_texture_;
  ResourceID texture_id_;
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SERVICE_CROSS_GL_SAMPLER_GL_H_
