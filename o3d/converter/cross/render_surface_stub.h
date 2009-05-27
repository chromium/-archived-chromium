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

#ifndef O3D_CONVERTER_CROSS_RENDER_SURFACE_STUB_H_
#define O3D_CONVERTER_CROSS_RENDER_SURFACE_STUB_H_

#include "core/cross/render_surface.h"
#include "core/cross/texture.h"

namespace o3d {

class RenderSurfaceStub : public RenderSurface {
 public:
  typedef SmartPointer<RenderSurfaceStub> Ref;

  // Constructs a RenderSurfaceStub instance.
  // Parameters:
  //  service_locator:  Service locator for the instance.
  RenderSurfaceStub(ServiceLocator *service_locator,
                    int width,
                    int height)
      : RenderSurface(service_locator, width, height, NULL) {}
  virtual ~RenderSurfaceStub() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(RenderSurfaceStub);
};

class RenderDepthStencilSurfaceStub : public RenderDepthStencilSurface {
 public:
  typedef SmartPointer<RenderDepthStencilSurfaceStub> Ref;

  RenderDepthStencilSurfaceStub(ServiceLocator *service_locator,
                                int width,
                                int height)
      : RenderDepthStencilSurface(service_locator, width, height) {}
  virtual ~RenderDepthStencilSurfaceStub() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(RenderDepthStencilSurfaceStub);
};

}  // namespace o3d

#endif  // O3D_CONVERTER_CROSS_RENDER_SURFACE_STUB_H_
