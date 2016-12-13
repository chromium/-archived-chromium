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


// This file contains the class declaration for SamplerGL.

#ifndef O3D_CORE_CROSS_GL_SAMPLER_GL_H_
#define O3D_CORE_CROSS_GL_SAMPLER_GL_H_

#include "core/cross/sampler.h"

namespace o3d {

class RendererGL;

// SamplerGL is an implementation of the Sampler object for GL.
class SamplerGL : public Sampler {
 public:
  explicit SamplerGL(ServiceLocator* service_locator);
  virtual ~SamplerGL();

  // Sets the gl texture and sampler states.
  void SetTextureAndStates(CGparameter cg_param);

  // Unbinds the GL texture.
  void ResetTexture(CGparameter cg_param);

 private:

  RendererGL* renderer_;

  DISALLOW_COPY_AND_ASSIGN(SamplerGL);
};
}  // namespace o3d


#endif  // O3D_CORE_CROSS_GL_SAMPLER_GL_H_
