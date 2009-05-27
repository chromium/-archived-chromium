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


// This file contains the implementation of RenderSurfaceGL and
// RenderDepthStencilSurfaceGL.

#include "core/cross/precompile.h"
#include "core/cross/gl/render_surface_gl.h"
#include "core/cross/gl/utils_gl-inl.h"

namespace o3d {

RenderSurfaceGL::RenderSurfaceGL(ServiceLocator *service_locator,
                                 int width,
                                 int height,
                                 GLenum cube_face,
                                 int mip_level,
                                 Texture *texture)
    : RenderSurface(service_locator, width, height, texture),
      cube_face_(cube_face),
      mip_level_(mip_level) {
  DCHECK(texture);
}

RenderSurfaceGL::~RenderSurfaceGL() {
}

RenderDepthStencilSurfaceGL::RenderDepthStencilSurfaceGL(
    ServiceLocator *service_locator,
    int width,
    int height)
    : RenderDepthStencilSurface(service_locator, width, height) {

  // If packed depth stencil is supported, create only one buffer for both
  // depth and stencil.
  if (GLEW_EXT_packed_depth_stencil) {
    glGenRenderbuffersEXT(1, render_buffers_);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, render_buffers_[0]);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,
                             GL_DEPTH24_STENCIL8_EXT,
                             width,
                             height);
    CHECK_GL_ERROR();
    render_buffers_[1] = render_buffers_[0];
  } else {
    glGenRenderbuffersEXT(2, render_buffers_);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, render_buffers_[0]);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,
                             GL_DEPTH_COMPONENT24,
                             width,
                             height);
    CHECK_GL_ERROR();

    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, render_buffers_[1]);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,
                             GL_STENCIL_INDEX8_EXT,
                             width,
                             height);
    CHECK_GL_ERROR();
  }
}

RenderDepthStencilSurfaceGL::~RenderDepthStencilSurfaceGL() {
  if (GLEW_EXT_packed_depth_stencil) {
    glDeleteRenderbuffersEXT(1, render_buffers_);
  } else {
    glDeleteRenderbuffersEXT(2, render_buffers_);
  }
}

}  // end namespace o3d
