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


// This file contains the declarations for RenderSurfaceGL and
// RenderDepthStencilSurfaceGL.

#ifndef O3D_CORE_CROSS_GL_RENDER_SURFACE_GL_H_
#define O3D_CORE_CROSS_GL_RENDER_SURFACE_GL_H_

#include "core/cross/render_surface.h"
#include "core/cross/texture.h"

namespace o3d {

class RenderSurfaceGL : public RenderSurface {
 public:
  typedef SmartPointer<RenderSurfaceGL> Ref;

  // Constructs a RenderSurfaceGL instance associated with the texture argument.
  // Parameters:
  //  service_locator:  Service locator for the instance.
  //  width:  The width of the surface, in pixels.
  //  height:  The height of the surface, in pixels.
  //  cube_face:  The face of the cube texture to which the surface is to be
  //    associated.  NOTE: If the texture is a 2d texture, then the value of
  //    this argument is irrelevent.
  //  mip_level:  The mip-level of the texture to associate with the surface.
  //  texture:  The texture to associate with the surface.
  RenderSurfaceGL(ServiceLocator *service_locator,
                  int width,
                  int height,
                  GLenum cube_face,
                  int mip_level,
                  Texture *texture);
  virtual ~RenderSurfaceGL();

  GLenum cube_face() const {
    return cube_face_;
  }

  int mip_level() const {
    return mip_level_;
  }
 private:
  GLenum cube_face_;
  int mip_level_;
  DISALLOW_COPY_AND_ASSIGN(RenderSurfaceGL);
};

class RenderDepthStencilSurfaceGL : public RenderDepthStencilSurface {
 public:
  typedef SmartPointer<RenderDepthStencilSurfaceGL> Ref;

  RenderDepthStencilSurfaceGL(ServiceLocator *service_locator,
                              int width,
                              int height);
  virtual ~RenderDepthStencilSurfaceGL();

  GLuint depth_buffer() const {
    return render_buffers_[0];
  }

  GLuint stencil_buffer() const {
    return render_buffers_[1];
  }
 private:
  // Handles to the depth and stencil render-buffers, respectively.
  GLuint render_buffers_[2];
  DISALLOW_COPY_AND_ASSIGN(RenderDepthStencilSurfaceGL);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_GL_RENDER_SURFACE_GL_H_
