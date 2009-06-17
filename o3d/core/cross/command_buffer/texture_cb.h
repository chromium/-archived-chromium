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


// This file contains the declarations for Texture2DCB and TextureCUBECB.

#ifndef O3D_CORE_CROSS_COMMAND_BUFFER_TEXTURE_CB_H__
#define O3D_CORE_CROSS_COMMAND_BUFFER_TEXTURE_CB_H__

// Precompiled header comes before everything else.
#include "core/cross/precompile.h"

#include "core/cross/bitmap.h"
#include "core/cross/texture.h"
#include "core/cross/types.h"
#include "command_buffer/common/cross/resource.h"

namespace o3d {

class RendererCB;

// Texture2DCB -----------------------------------------------------------------

// Texture2DCB implements the Texture2D interface for command buffers.
class Texture2DCB : public Texture2D {
 public:
  typedef SmartPointer<Texture2DCB> Ref;

  virtual ~Texture2DCB();

  // Creates a new Texture2DCB with the given specs. If the texture creation
  // fails then it returns NULL otherwise it returns a pointer to the newly
  // created Texture object.
  // The created texture takes ownership of the bitmap data.
  static Texture2DCB* Create(ServiceLocator* service_locator,
                             Bitmap *bitmap,
                             bool enable_render_surfaces);

  // Locks the image buffer of a given mipmap level for writing from main
  // memory.
  virtual bool Lock(int level, void** texture_data);

  // Unlocks this texture and returns it to OpenCB control.
  virtual bool Unlock(int level);

  // Returns a RenderSurface object associated with a mip_level of a texture.
  // Parameters:
  //  mip_level: [in] The mip-level of the surface to be returned.
  //  pack: [in] The pack in which the surface will reside.
  // Returns:
  //  Reference to the RenderSurface object.
  virtual RenderSurface::Ref GetRenderSurface(int mip_level, Pack *pack);

  // Returns the implementation-specific texture handle for this texture.
  virtual void* GetTextureHandle() const {
    return reinterpret_cast<void*>(resource_id_);
  }

  // Gets the texture resource ID.
  command_buffer::ResourceID resource_id() const { return resource_id_; }

  // Gets a RGBASwizzleIndices that contains a mapping from
  // RGBA to the internal format used by the rendering API.
  virtual const RGBASwizzleIndices& GetABGR32FSwizzleIndices();


 private:
  // Initializes the Texture2DCB from a preexisting OpenCB texture handle
  // and raw Bitmap data.
  // The texture takes ownership of the bitmap data.
  Texture2DCB(ServiceLocator* service_locator,
              command_buffer::ResourceID resource_id,
              const Bitmap &bitmap,
              bool resize_npot,
              bool enable_render_surfaces);

  // Returns true if the backing bitmap has the data for the level.
  bool HasLevel(unsigned int level) {
    DCHECK_LT(level, levels());
    return (has_levels_ & (1 << level)) != 0;
  }

  RendererCB* renderer_;
  command_buffer::ResourceID resource_id_;

  // A bitmap used to back the NPOT textures on POT-only hardware, and to back
  // the pixel buffer for Lock().
  Bitmap backing_bitmap_;

  // Bitfield that indicates mip levels that are currently stored in the
  // backing bitmap.
  unsigned int has_levels_;
};

// TextureCUBECB ---------------------------------------------------------------

// TextureCUBECB implements the TextureCUBE interface with OpenCB.
class TextureCUBECB : public TextureCUBE {
 public:
  typedef SmartPointer<TextureCUBECB> Ref;
  virtual ~TextureCUBECB();

  // Create a new Cube texture from scratch.
  static TextureCUBECB* TextureCUBECB::Create(ServiceLocator* service_locator,
                                              Bitmap *bitmap,
                                              bool enable_render_surfaces);

  // Locks the image buffer of a given face and mipmap level for loading
  // from main memory.
  virtual bool Lock(CubeFace face, int level, void** texture_data);

  // Unlocks the image buffer of a given face and mipmap level.
  virtual bool Unlock(CubeFace face, int level);

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

  // Returns the implementation-specific texture handle for this texture.
  virtual void* GetTextureHandle() const {
    return reinterpret_cast<void*>(resource_id_);
  }

  // Gets the texture resource ID.
  command_buffer::ResourceID resource_id() const { return resource_id_; }

  // Gets a RGBASwizzleIndices that contains a mapping from
  // RGBA to the internal format used by the rendering API.
  virtual const RGBASwizzleIndices& GetABGR32FSwizzleIndices();

 private:
  // Creates a texture from a pre-existing texture resource.
  TextureCUBECB(ServiceLocator* service_locator,
                command_buffer::ResourceID texture,
                const Bitmap &bitmap,
                bool resize_to_pot,
                bool enable_render_surfaces);


  // Returns true if the backing bitmap has the data for the level.
  bool HasLevel(unsigned int level, CubeFace face) {
    DCHECK_LT(level, levels());
    return (has_levels_[face] & (1 << level)) != 0;
  }

  RendererCB* renderer_;
  command_buffer::ResourceID resource_id_;

  // A bitmap used to back the NPOT textures on POT-only hardware, and to back
  // the pixel buffer for Lock().
  Bitmap backing_bitmap_;

  // Bitfields that indicates mip levels that are currently stored in the
  // backing bitmap, one per face.
  unsigned int has_levels_[6];
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_COMMAND_BUFFER_TEXTURE_CB_H__
