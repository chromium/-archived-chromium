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


#ifndef O3D_CORE_WIN_D3D9_RENDER_SURFACE_D3D9_H_
#define O3D_CORE_WIN_D3D9_RENDER_SURFACE_D3D9_H_

#include <d3d9.h>
#include <atlbase.h>
#include "core/cross/render_surface.h"

namespace o3d {

// A memento class that is capable of constructing and returning
// a DirectX surface.  It maintains all of the parameters relevant to the
// construction internally.
class SurfaceConstructor {
 public:
  SurfaceConstructor() {}
  virtual ~SurfaceConstructor() {}
  virtual HRESULT ConstructSurface(IDirect3DSurface9** surface) = 0;
 private:
  DISALLOW_COPY_AND_ASSIGN(SurfaceConstructor);
};

class RenderSurfaceD3D9 : public RenderSurface {
 public:
  typedef SmartPointer<RenderSurfaceD3D9> Ref;

  RenderSurfaceD3D9(ServiceLocator *service_locator,
                    int width,
                    int height,
                    Texture *texture,
                    SurfaceConstructor *surface_constructor);

  IDirect3DSurface9* GetSurfaceHandle() const {
    return direct3d_surface_;
  }

  // Handler for lost device. This invalidates the render surface for a
  // device reset.
  bool OnLostDevice();

  // Handler for reset device. This restores the render surface after a
  // device reset.
  bool OnResetDevice();

  // Clears the surface to 0, 0, 0, 0.
  // TODO: Move this to texture, expose it to JavaScript and let
  //     the user supply an RGBA color.
  void Clear();

 private:
  CComPtr<IDirect3DSurface9> direct3d_surface_;
  scoped_ptr<SurfaceConstructor> surface_constructor_;
  DISALLOW_COPY_AND_ASSIGN(RenderSurfaceD3D9);
};

class RenderDepthStencilSurfaceD3D9 : public RenderDepthStencilSurface {
 public:
  typedef SmartPointer<RenderDepthStencilSurfaceD3D9> Ref;

  RenderDepthStencilSurfaceD3D9(ServiceLocator *service_locator,
                                int width,
                                int height,
                                SurfaceConstructor *surface_constructor);

  IDirect3DSurface9* GetSurfaceHandle() const {
    return direct3d_surface_;
  }

  // Handler for lost device. This invalidates the render surface for a
  // device reset.
  bool OnLostDevice();

  // Handler for reset device. This restores the render surface after a
  // device reset.
  bool OnResetDevice();

 private:
  CComPtr<IDirect3DSurface9> direct3d_surface_;
  scoped_ptr<SurfaceConstructor> surface_constructor_;
  DISALLOW_COPY_AND_ASSIGN(RenderDepthStencilSurfaceD3D9);
};

}  // namespace o3d

#endif  // O3D_CORE_WIN_D3D9_RENDER_SURFACE_D3D9_H_
