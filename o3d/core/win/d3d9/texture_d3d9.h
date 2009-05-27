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


// This file contains the declarations for Texture2DD3D9 and TextureCUBED3D9.

#ifndef O3D_CORE_WIN_D3D9_TEXTURE_D3D9_H__
#define O3D_CORE_WIN_D3D9_TEXTURE_D3D9_H__

#include <atlbase.h>
#include <vector>

#include "core/cross/bitmap.h"
#include "core/cross/texture.h"
#include "core/cross/types.h"

interface IDirect3DTexture9;
interface IDirect3DCubeTexture9;
interface IDirect3DDevice9;

namespace o3d {

class RendererD3D9;

// Texture2DD3D9 implements the Texture2D interface with DX9.
class Texture2DD3D9 : public Texture2D {
 public:
  typedef SmartPointer<Texture2DD3D9> Ref;

  // Creates a new Texture2DD3D9 with the given specs.  If the D3D9 texture
  // creation fails then it returns NULL otherwise it returns a pointer to the
  // newly created Texture object.
  static Texture2DD3D9* Create(ServiceLocator* service_locator,
                               Bitmap* bitmap,
                               RendererD3D9* renderer,
                               bool enable_render_surfaces);

  virtual ~Texture2DD3D9();

  // Locks the image buffer of a given mipmap level for loading from
  // main memory.  A pointer to the current contents of the texture is returned
  // in texture_data.
  virtual bool Lock(int level, void** texture_data);

  // Notifies DX9 that the texture data has been updated.
  virtual bool Unlock(int level);

  // Returns the implementation-specific texture handle for this texture.
  virtual void* GetTextureHandle() const { return d3d_texture_; }

  // Returns a RenderSurface object associated with a mip_level of a texture.
  // Parameters:
  //  mip_level: [in] The mip-level of the surface to be returned.
  //  pack: [in] The pack in which the surface will reside.
  // Returns:
  //  Reference to the RenderSurface object.
  virtual RenderSurface::Ref GetRenderSurface(int mip_level, Pack* pack);

  // Handler for lost device. This invalidates the texture for a device reset.
  bool OnLostDevice();

  // Handler for reset device. This restores the texture after a device reset.
  bool OnResetDevice();

  // Gets a RGBASwizzleIndices that contains a mapping from
  // RGBA to the internal format used by the rendering API.
  virtual const RGBASwizzleIndices& GetABGR32FSwizzleIndices();

 private:
  // Initializes the Texture2DD3D9 from a DX9 texture.
  Texture2DD3D9(ServiceLocator* service_locator,
                IDirect3DTexture9* tex,
                const Bitmap& bitmap,
                bool resize_to_pot,
                bool enable_render_surfaces);

  // Updates a mip level, sending it from the backing bitmap to Direct3D,
  // rescaling it if resize_to_pot_ is set.
  bool UpdateBackedMipLevel(unsigned int level);

  // A pointer to the Direct3D 2D texture object containing this texture.
  CComPtr<IDirect3DTexture9> d3d_texture_;

  // A bitmap used to back the NPOT textures on POT-only hardware.
  Bitmap backing_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(Texture2DD3D9);
};

// TextureCUBED3D9 implements the TextureCUBE interface with DX9.
class TextureCUBED3D9 : public TextureCUBE {
 public:
  typedef SmartPointer<TextureCUBED3D9> Ref;

  // Creates a new TextureCUBED3D9 with the given specs.  If the D3D9 texture
  // creation fails then it returns NULL otherwise it returns a pointer to the
  // newly created Texture object.
  static TextureCUBED3D9* Create(ServiceLocator* service_locator,
                                 Bitmap* bitmap,
                                 RendererD3D9* renderer,
                                 bool enable_render_surfaces);

  virtual ~TextureCUBED3D9();

  // Locks the image buffer of a given face and mipmap level for loading from
  // main memory.
  bool Lock(CubeFace face, int level, void** texture_data);

  // Notifies DX9 that the image buffer of a given face and mipmap level has
  // been updated.
  bool Unlock(CubeFace face, int level);

  // Returns the implementation-specific texture handle for this texture.
  virtual void* GetTextureHandle() const { return d3d_cube_texture_; }

  // Returns a RenderSurface object associated with a given cube face and
  // mip_level of a texture.
  // Parameters:
  //  face: [in] The cube face from which to extract the surface.
  //  mip_level: [in] The mip-level of the surface to be returned.
  //  pack: [in] The pack in which the surface will reside.
  // Returns:
  //  Reference to the RenderSurface object.
  virtual RenderSurface::Ref GetRenderSurface(CubeFace face,
                                              int level,
                                              Pack* pack);

  // Handler for lost device. This invalidates the texture for a device reset.
  bool OnLostDevice();

  // Handler for reset device. This restores the texture after a device reset.
  bool OnResetDevice();

  // Gets a RGBASwizzleIndices that contains a mapping from
  // RGBA to the internal format used by the rendering API.
  virtual const RGBASwizzleIndices& GetABGR32FSwizzleIndices();

 private:
  TextureCUBED3D9(ServiceLocator* service_locator,
                  IDirect3DCubeTexture9* tex,
                  const Bitmap& bitmap,
                  bool resize_to_pot,
                  bool enable_render_surfaces);

  // Updates a mip level, sending it from the backing bitmap to Direct3D,
  // rescaling it if resize_to_pot_ is set.
  bool UpdateBackedMipLevel(unsigned int level, CubeFace face);

  // A pointer to the Direct3D cube texture object containing this texture.
  CComPtr<IDirect3DCubeTexture9> d3d_cube_texture_;

  // A bitmap used to back the NPOT textures on POT-only hardware.
  Bitmap backing_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(TextureCUBED3D9);
};

}  // namespace o3d

#endif  // O3D_CORE_WIN_D3D9_TEXTURE_D3D9_H__
