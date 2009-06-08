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


#include "core/cross/precompile.h"
#include "core/win/d3d9/texture_d3d9.h"

#include "core/cross/error.h"
#include "core/cross/types.h"
#include "core/win/d3d9/utils_d3d9.h"
#include "core/win/d3d9/renderer_d3d9.h"
#include "core/win/d3d9/render_surface_d3d9.h"

namespace o3d {

namespace {

Texture::RGBASwizzleIndices g_d3d_abgr32f_swizzle_indices = {2, 1, 0, 3};

// Converts an O3D texture format to a D3D texture format.
D3DFORMAT DX9Format(Texture::Format format) {
  switch (format) {
    case Texture::XRGB8:  return D3DFMT_X8R8G8B8;
    case Texture::ARGB8:  return D3DFMT_A8R8G8B8;
    case Texture::ABGR16F:  return D3DFMT_A16B16G16R16F;
    case Texture::R32F:  return D3DFMT_R32F;
    case Texture::ABGR32F:  return D3DFMT_A32B32G32R32F;
    case Texture::DXT1:  return D3DFMT_DXT1;
    case Texture::DXT3:  return D3DFMT_DXT3;
    case Texture::DXT5:  return D3DFMT_DXT5;
    default:  return D3DFMT_UNKNOWN;
  };
}

// Converts a TextureCUBE::CubeFace value to an equivalent D3D9 value.
static D3DCUBEMAP_FACES DX9CubeFace(TextureCUBE::CubeFace face) {
  switch (face) {
    case TextureCUBE::FACE_POSITIVE_X:
      return D3DCUBEMAP_FACE_POSITIVE_X;
    case TextureCUBE::FACE_NEGATIVE_X:
      return D3DCUBEMAP_FACE_NEGATIVE_X;
    case TextureCUBE::FACE_POSITIVE_Y:
      return D3DCUBEMAP_FACE_POSITIVE_Y;
    case TextureCUBE::FACE_NEGATIVE_Y:
      return D3DCUBEMAP_FACE_NEGATIVE_Y;
    case TextureCUBE::FACE_POSITIVE_Z:
      return D3DCUBEMAP_FACE_POSITIVE_Z;
    case TextureCUBE::FACE_NEGATIVE_Z:
      return D3DCUBEMAP_FACE_NEGATIVE_Z;
  }

  // TODO: Figure out how to get errors out of here to the client.
  DLOG(ERROR) << "Unknown Cube Face enumeration " << face;
  return D3DCUBEMAP_FACE_FORCE_DWORD;
}

// Constructs an Direct3D texture object.  Out variable return the status of
// the constructed texture including if resize to POT is required, and the
// actual mip dimensions used.
HRESULT CreateTexture2DD3D9(RendererD3D9* renderer,
                            Bitmap* bitmap,
                            bool enable_render_surfaces,
                            bool* resize_to_pot,
                            unsigned int* mip_width,
                            unsigned int* mip_height,
                            IDirect3DTexture9** d3d_texture) {
  IDirect3DDevice9 *d3d_device = renderer->d3d_device();
  *resize_to_pot = !renderer->supports_npot() && !bitmap->IsPOT();
  *mip_width = bitmap->width();
  *mip_height = bitmap->height();

  if (*resize_to_pot) {
    *mip_width = Bitmap::GetPOTSize(*mip_width);
    *mip_height = Bitmap::GetPOTSize(*mip_height);
  }

  DWORD usage = (enable_render_surfaces) ? D3DUSAGE_RENDERTARGET : 0;
  D3DPOOL pool = (enable_render_surfaces) ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
  D3DFORMAT format = DX9Format(bitmap->format());

  HRESULT tex_result = d3d_device->CreateTexture(*mip_width,
                                                 *mip_height,
                                                 bitmap->num_mipmaps(),
                                                 usage,
                                                 format,
                                                 pool,
                                                 d3d_texture,
                                                 NULL);
  if (!HR(tex_result)) {
    DLOG(ERROR) << "2D texture creation failed with the following parameters: "
                << "(" << *mip_width << " x " << *mip_height << ") x "
                << bitmap->num_mipmaps() << "; format = " << format;
  }
  return tex_result;
}

// Constructs an Direct3D cube texture object.  Out variable return the
// status of the constructed texture including if resize to POT is required,
// and the actual mip edge length used.
HRESULT CreateTextureCUBED3D9(RendererD3D9* renderer,
                              Bitmap* bitmap,
                              bool enable_render_surfaces,
                              bool* resize_to_pot,
                              unsigned int* edge_width,
                              IDirect3DCubeTexture9** d3d_texture) {
  IDirect3DDevice9 *d3d_device = renderer->d3d_device();
  *resize_to_pot = !renderer->supports_npot() && !bitmap->IsPOT();
  *edge_width = bitmap->width();
  if (*resize_to_pot) {
    *edge_width = Bitmap::GetPOTSize(*edge_width);
  }

  DWORD usage = (enable_render_surfaces) ? D3DUSAGE_RENDERTARGET : 0;
  D3DPOOL pool = (enable_render_surfaces) ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
  D3DFORMAT format = DX9Format(bitmap->format());

  HRESULT tex_result = d3d_device->CreateCubeTexture(*edge_width,
                                                     bitmap->num_mipmaps(),
                                                     usage,
                                                     format,
                                                     pool,
                                                     d3d_texture,
                                                     NULL);
  if (!HR(tex_result)) {
    DLOG(ERROR) << "CUBE texture creation failed with the following "
                << "parameters: "
                << "(" << edge_width << " x " << edge_width << ") x "
                << bitmap->num_mipmaps() << "; format = " << format;
  }

  return tex_result;
}

// Class providing a construction callback routine for extracting a
// RenderSurface from a cube-face and mip-level of a cube-texture.
// Note:  This class maintains a reference-counted pointer to the texture
// object, so that the lifetime of the Texture is guaranteed to be at least
// as long as that of the class.
class CubeFaceSurfaceConstructor : public SurfaceConstructor {
 public:
  CubeFaceSurfaceConstructor(TextureCUBED3D9 *texture,
                             TextureCUBE::CubeFace face,
                             int mip_level)
      : cube_texture_(texture),
        face_(face),
        mip_level_(mip_level) {
  }

  virtual HRESULT ConstructSurface(IDirect3DSurface9** surface) {
    IDirect3DCubeTexture9* d3d_cube_texture =
        static_cast<IDirect3DCubeTexture9*>(cube_texture_->GetTextureHandle());
    return d3d_cube_texture->GetCubeMapSurface(DX9CubeFace(face_),
                                               mip_level_,
                                               surface);
  }

 private:
  TextureCUBED3D9::Ref cube_texture_;
  TextureCUBE::CubeFace face_;
  int mip_level_;
  DISALLOW_COPY_AND_ASSIGN(CubeFaceSurfaceConstructor);
};

// Class providing a construction callback routine for extracting a
// RenderSurface from a mip-level of a texture.
// Note:  This class maintains a reference-counted pointer to the texture
// object, so that the lifetime of the Texture is guaranteed to be at least
// as long as that of the class.
class TextureSurfaceConstructor : public SurfaceConstructor {
 public:
  TextureSurfaceConstructor(Texture2DD3D9* texture, int mip_level)
      : texture_(texture),
        mip_level_(mip_level) {
  }

  virtual HRESULT ConstructSurface(IDirect3DSurface9** surface) {
    IDirect3DTexture9* d3d_texture =
        static_cast<IDirect3DTexture9*>(texture_->GetTextureHandle());
    return d3d_texture->GetSurfaceLevel(mip_level_, surface);
  }

 private:
  Texture2DD3D9::Ref texture_;
  int mip_level_;
  DISALLOW_COPY_AND_ASSIGN(TextureSurfaceConstructor);
};

}  // unnamed namespace

// Constructs a 2D texture object from the given (existing) D3D 2D texture.
Texture2DD3D9::Texture2DD3D9(ServiceLocator* service_locator,
                             IDirect3DTexture9* tex,
                             const Bitmap &bitmap,
                             bool resize_to_pot,
                             bool enable_render_surfaces)
    : Texture2D(service_locator,
                bitmap.width(),
                bitmap.height(),
                bitmap.format(),
                bitmap.num_mipmaps(),
                bitmap.CheckAlphaIsOne(),
                resize_to_pot,
                enable_render_surfaces),
      d3d_texture_(tex) {
  DCHECK(tex);
}

// Attempts to create a IDirect3DTexture9 with the given specs.  If the creation
// of the texture succeeds then it creates a Texture2DD3D9 object around it and
// returns it.  This is the safe way to create a Texture2DD3D9 object that
// contains a valid D3D9 texture.
Texture2DD3D9* Texture2DD3D9::Create(ServiceLocator* service_locator,
                                     Bitmap* bitmap,
                                     RendererD3D9* renderer,
                                     bool enable_render_surfaces) {
  DCHECK_NE(bitmap->format(), Texture::UNKNOWN_FORMAT);
  DCHECK(!bitmap->is_cubemap());
  CComPtr<IDirect3DTexture9> d3d_texture;
  bool resize_to_pot;
  unsigned int mip_width, mip_height;
  if (!HR(CreateTexture2DD3D9(renderer,
                              bitmap,
                              enable_render_surfaces,
                              &resize_to_pot,
                              &mip_width,
                              &mip_height,
                              &d3d_texture))) {
    DLOG(ERROR) << "Failed to create Texture2D (D3D9) : ";
    return NULL;
  }

  Texture2DD3D9 *texture = new Texture2DD3D9(service_locator,
                                             d3d_texture,
                                             *bitmap,
                                             resize_to_pot,
                                             enable_render_surfaces);

  texture->backing_bitmap_.SetFrom(bitmap);
  if (texture->backing_bitmap_.image_data()) {
    for (unsigned int i = 0; i < bitmap->num_mipmaps(); ++i) {
      if (!texture->UpdateBackedMipLevel(i)) {
        DLOG(ERROR) << "Failed to upload bitmap to texture.";
        delete texture;
        return NULL;
      }
      mip_width = std::max(1U, mip_width >> 1);
      mip_height = std::max(1U, mip_height >> 1);
    }
    if (!resize_to_pot)
      texture->backing_bitmap_.FreeData();
  } else {
    if (resize_to_pot) {
      texture->backing_bitmap_.AllocateData();
      memset(texture->backing_bitmap_.image_data(), 0,
             texture->backing_bitmap_.GetTotalSize());
    }
  }

  return texture;
}

// Destructor releases the D3D9 texture resource.
Texture2DD3D9::~Texture2DD3D9() {
  d3d_texture_ = NULL;
}

bool Texture2DD3D9::UpdateBackedMipLevel(unsigned int level) {
  DCHECK_LT(level, static_cast<unsigned int>(levels()));
  DCHECK(backing_bitmap_.image_data());
  DCHECK_EQ(backing_bitmap_.width(), width());
  DCHECK_EQ(backing_bitmap_.height(), height());
  DCHECK_EQ(backing_bitmap_.format(), format());
  DCHECK_EQ(backing_bitmap_.num_mipmaps(), levels());

  unsigned int mip_width = std::max(1, width() >> level);
  unsigned int mip_height = std::max(1, height() >> level);
  unsigned int rect_width = mip_width;
  unsigned int rect_height = mip_height;
  if (resize_to_pot_) {
    rect_width = std::max(1U, Bitmap::GetPOTSize(width()) >> level);
    rect_height = std::max(1U, Bitmap::GetPOTSize(height()) >> level);
  }

  RECT rect = {0, 0, rect_width, rect_height};
  D3DLOCKED_RECT out_rect;
  out_rect.pBits = 0;

  if (!HR(d3d_texture_->LockRect(level, &out_rect, &rect, 0))) {
    DLOG(ERROR) << "Failed to lock texture level " << level << ".";
    return false;
  }

  DCHECK(out_rect.pBits);
  // TODO: check that the returned pitch is what we expect.

  const unsigned char *mip_data =
      backing_bitmap_.GetMipData(level, TextureCUBE::FACE_POSITIVE_X);
  if (resize_to_pot_) {
    Bitmap::Scale(mip_width, mip_height, format(), mip_data,
                  rect_width, rect_height,
                  static_cast<unsigned char *>(out_rect.pBits));
  } else {
    unsigned int mip_size =
        Bitmap::GetBufferSize(mip_width, mip_height, format());
    memcpy(out_rect.pBits, mip_data, mip_size);
  }

  if (!HR(d3d_texture_->UnlockRect(level))) {
    O3D_ERROR(service_locator())
        << "Failed to unlock texture level " << level << ".";
    return false;
  }
  return true;
}

RenderSurface::Ref Texture2DD3D9::GetRenderSurface(int mip_level, Pack* pack) {
  DCHECK(pack);
  if (!render_surfaces_enabled()) {
    O3D_ERROR(service_locator())
        << "Attempting to get RenderSurface from non-render-surface-enabled"
        << " Texture: " << name();
    return RenderSurface::Ref(NULL);
  }

  if (mip_level >= levels() || mip_level < 0) {
    O3D_ERROR(service_locator())
        << "Attempting to access non-existent mip_level " << mip_level
        << " in render-target texture \"" << name() << "\".";
    return RenderSurface::Ref(NULL);
  }

  RenderSurface::Ref render_surface(
      new RenderSurfaceD3D9(
          service_locator(),
          width() >> mip_level,
          height() >> mip_level,
          this,
          new TextureSurfaceConstructor(this, mip_level)));

  if (!render_surface.IsNull()) {
    RegisterSurface(render_surface.Get(), pack);
  }

  return render_surface;
}

// Locks the given mipmap level of this texture for loading from main memory,
// and returns a pointer to the buffer.
bool Texture2DD3D9::Lock(int level, void** texture_data) {
  if (level >= levels() || level < 0) {
    O3D_ERROR(service_locator())
        << "Trying to lock inexistent level " << level << " on Texture \""
        << name() << "\"";
    return false;
  }
  if (IsLocked(level)) {
    O3D_ERROR(service_locator())
        << "Level " << level << " of texture \"" << name()
        << "\" is already locked.";
    return false;
  }
  if (render_surfaces_enabled()) {
    O3D_ERROR(service_locator())
        << "Attempting to lock a render-target texture: " << name();
    return false;
  }
  if (resize_to_pot_) {
    DCHECK(backing_bitmap_.image_data());
    *texture_data = backing_bitmap_.GetMipData(level,
                                               TextureCUBE::FACE_POSITIVE_X);
    locked_levels_ |= 1 << level;
    return true;
  } else {
    RECT rect = {0, 0, width(), height()};
    D3DLOCKED_RECT out_rect = {0};

    if (HR(d3d_texture_->LockRect(level, &out_rect, &rect, 0))) {
      *texture_data = out_rect.pBits;
      locked_levels_ |= 1 << level;
      return true;
    } else {
      O3D_ERROR(service_locator()) << "Failed to Lock Texture2D (D3D9)";
      *texture_data = NULL;
      return false;
    }
  }
}

// Unlocks the given mipmap level of this texture.
bool Texture2DD3D9::Unlock(int level) {
  if (level >= levels() || level < 0) {
    O3D_ERROR(service_locator())
        << "Trying to unlock inexistent level " << level << " on Texture \""
        << name() << "\"";
    return false;
  }
  if (!IsLocked(level)) {
    O3D_ERROR(service_locator())
        << "Level " << level << " of texture \"" << name()
        << "\" is not locked.";
    return false;
  }
  bool result = false;
  if (resize_to_pot_) {
    result = UpdateBackedMipLevel(level);
  } else {
    result = HR(d3d_texture_->UnlockRect(level));
  }
  if (result) {
    locked_levels_ &= ~(1 << level);
  } else {
    O3D_ERROR(service_locator()) << "Failed to Unlock Texture2D (D3D9)";
  }
  return result;
}

bool Texture2DD3D9::OnLostDevice() {
  // Textures created with RenderSurface support are placed in the default
  // pool, so release them here.
  if (render_surfaces_enabled()) {
    d3d_texture_ = NULL;
  }
  return true;
}

bool Texture2DD3D9::OnResetDevice() {
  if (render_surfaces_enabled()) {
    DCHECK(d3d_texture_ == NULL);
    Renderer* renderer = service_locator()->GetService<Renderer>();
    RendererD3D9 *renderer_d3d9 = down_cast<RendererD3D9*>(renderer);
    bool resize_to_pot;
    unsigned int mip_width, mip_height;
    return HR(CreateTexture2DD3D9(renderer_d3d9,
                                  &backing_bitmap_,
                                  render_surfaces_enabled(),
                                  &resize_to_pot,
                                  &mip_width,
                                  &mip_height,
                                  &d3d_texture_));
  }
  return true;
}

const Texture::RGBASwizzleIndices& Texture2DD3D9::GetABGR32FSwizzleIndices() {
  return g_d3d_abgr32f_swizzle_indices;
}

// Constructs a cube texture object from the given (existing) D3D Cube texture.
TextureCUBED3D9::TextureCUBED3D9(ServiceLocator* service_locator,
                                 IDirect3DCubeTexture9* tex,
                                 const Bitmap& bitmap,
                                 bool resize_to_pot,
                                 bool enable_render_surfaces)
    : TextureCUBE(service_locator,
                  bitmap.width(),
                  bitmap.format(),
                  bitmap.num_mipmaps(),
                  bitmap.CheckAlphaIsOne(),
                  resize_to_pot,
                  enable_render_surfaces),
      d3d_cube_texture_(tex) {
}

// Attempts to create a D3D9 CubeTexture with the given specs.  If creation
// fails the method returns NULL.  Otherwise, it wraps around the newly created
// texture a TextureCUBED3D9 object and returns a pointer to it.
TextureCUBED3D9* TextureCUBED3D9::Create(ServiceLocator* service_locator,
                                         Bitmap *bitmap,
                                         RendererD3D9 *renderer,
                                         bool enable_render_surfaces) {
  DCHECK_NE(bitmap->format(), Texture::UNKNOWN_FORMAT);
  DCHECK(bitmap->is_cubemap());
  DCHECK_EQ(bitmap->width(), bitmap->height());

  CComPtr<IDirect3DCubeTexture9> d3d_texture;
  bool resize_to_pot;
  unsigned int edge;
  if (!HR(CreateTextureCUBED3D9(renderer,
                                bitmap,
                                enable_render_surfaces,
                                &resize_to_pot,
                                &edge,
                                &d3d_texture))) {
    DLOG(ERROR) << "Failed to create TextureCUBE (D3D9)";
    return NULL;
  }

  TextureCUBED3D9 *texture = new TextureCUBED3D9(service_locator,
                                                 d3d_texture,
                                                 *bitmap,
                                                 resize_to_pot,
                                                 enable_render_surfaces);

  texture->backing_bitmap_.SetFrom(bitmap);
  if (texture->backing_bitmap_.image_data()) {
    for (int face = 0; face < 6; ++face) {
      unsigned int mip_edge = edge;
      for (unsigned int i = 0; i < bitmap->num_mipmaps(); ++i) {
        if (!texture->UpdateBackedMipLevel(i, static_cast<CubeFace>(face))) {
          DLOG(ERROR) << "Failed to upload bitmap to texture.";
          delete texture;
          return NULL;
        }
        mip_edge = std::max(1U, mip_edge >> 1);
      }
    }
    if (!resize_to_pot)
      texture->backing_bitmap_.FreeData();
  } else {
    if (resize_to_pot) {
      texture->backing_bitmap_.AllocateData();
      memset(texture->backing_bitmap_.image_data(), 0,
             texture->backing_bitmap_.GetTotalSize());
    }
  }

  return texture;
}



// Destructor releases the D3D9 texture resource.
TextureCUBED3D9::~TextureCUBED3D9() {
  for (unsigned int i = 0; i < 6; ++i) {
    if (locked_levels_[i] != 0) {
      O3D_ERROR(service_locator())
          << "TextureCUBE \"" << name() << "\" was never unlocked before "
          << "being destroyed.";
      break;  // No need to report it more than once.
    }
  }
  d3d_cube_texture_ = NULL;
}

bool TextureCUBED3D9::UpdateBackedMipLevel(unsigned int level,
                                           TextureCUBE::CubeFace face) {
  DCHECK_LT(level, static_cast<unsigned int>(levels()));
  DCHECK(backing_bitmap_.image_data());
  DCHECK(backing_bitmap_.is_cubemap());
  DCHECK_EQ(backing_bitmap_.width(), edge_length());
  DCHECK_EQ(backing_bitmap_.height(), edge_length());
  DCHECK_EQ(backing_bitmap_.format(), format());
  DCHECK_EQ(backing_bitmap_.num_mipmaps(), levels());

  unsigned int mip_edge = std::max(1, edge_length() >> level);
  unsigned int rect_edge = mip_edge;
  if (resize_to_pot_) {
    rect_edge = std::max(1U, Bitmap::GetPOTSize(edge_length()) >> level);
  }

  RECT rect = {0, 0, rect_edge, rect_edge};
  D3DLOCKED_RECT out_rect;
  out_rect.pBits = 0;

  if (!HR(d3d_cube_texture_->LockRect(DX9CubeFace(face), level, &out_rect,
                                      &rect, 0))) {
    O3D_ERROR(service_locator())
        << "Failed to lock texture level " << level << " face " << face << ".";
    return false;
  }

  DCHECK(out_rect.pBits);
  // TODO: check that the returned pitch is what we expect.

  const unsigned char *mip_data = backing_bitmap_.GetMipData(level, face);
  if (resize_to_pot_) {
    Bitmap::Scale(mip_edge, mip_edge, format(), mip_data,
                  rect_edge, rect_edge,
                  static_cast<unsigned char *>(out_rect.pBits));
  } else {
    unsigned int mip_size =
        Bitmap::GetBufferSize(mip_edge, mip_edge, format());
    memcpy(out_rect.pBits, mip_data, mip_size);
  }

  if (!HR(d3d_cube_texture_->UnlockRect(DX9CubeFace(face), level))) {
    O3D_ERROR(service_locator())
        << "Failed to unlock texture level " << level << " face " << face
        << ".";
    return false;
  }
  return true;
}

RenderSurface::Ref TextureCUBED3D9::GetRenderSurface(CubeFace face,
                                                     int mip_level,
                                                     Pack* pack) {
  if (!render_surfaces_enabled()) {
    O3D_ERROR(service_locator())
        << "Attempting to get RenderSurface from non-render-surface-enabled"
        << " Texture: " << name();
    return RenderSurface::Ref(NULL);
  }

  if (mip_level >= levels() || mip_level < 0) {
    O3D_ERROR(service_locator())
        << "Attempting to access non-existent mip_level " << mip_level
        << " in render-target texture \"" << name() << "\".";
    return RenderSurface::Ref(NULL);
  }

  int edge = edge_length() >> mip_level;
  RenderSurface::Ref render_surface(
      new RenderSurfaceD3D9(
          service_locator(),
          edge,
          edge,
          this,
          new CubeFaceSurfaceConstructor(this, face, mip_level)));

  if (!render_surface.IsNull()) {
    RegisterSurface(render_surface.Get(), pack);
  }

  return render_surface;
}

// Locks the given face and mipmap level of this texture for loading from
// main memory, and returns a pointer to the buffer.
bool TextureCUBED3D9::Lock(CubeFace face, int level, void** texture_data) {
  if (level >= levels() || level < 0) {
    O3D_ERROR(service_locator())
        << "Trying to lock inexistent level " << level << " on Texture \""
        << name();
    return false;
  }
  if (IsLocked(level, face)) {
    O3D_ERROR(service_locator())
        << "Level " << level << " Face " << face << " of texture \"" << name()
        << "\" is already locked.";
    return false;
  }
  if (render_surfaces_enabled()) {
    O3D_ERROR(service_locator())
        << "Attempting to lock a render-target texture: " << name();
    return false;
  }
  if (resize_to_pot_) {
    DCHECK(backing_bitmap_.image_data());
    *texture_data = backing_bitmap_.GetMipData(level, face);
    locked_levels_[face] |= 1 << level;
    return true;
  } else {
    RECT rect = {0, 0, edge_length(), edge_length()};
    D3DLOCKED_RECT out_rect = {0};

    if (HR(d3d_cube_texture_->LockRect(DX9CubeFace(face), level,
                                       &out_rect, &rect, 0))) {
      *texture_data = out_rect.pBits;
      locked_levels_[face] |= 1 << level;
      return true;
    } else {
      O3D_ERROR(service_locator()) << "Failed to Lock Texture2D (D3D9)";
      *texture_data = NULL;
      return false;
    }
  }
}

// Unlocks the given face and mipmap level of this texture.
bool TextureCUBED3D9::Unlock(CubeFace face, int level) {
  if (level >= levels() || level < 0) {
    O3D_ERROR(service_locator())
        << "Trying to unlock inexistent level " << level << " on Texture \""
        << name();
    return false;
  }
  if (!IsLocked(level, face)) {
    O3D_ERROR(service_locator())
        << "Level " << level << " of texture \"" << name()
        << "\" is not locked.";
    return false;
  }
  bool result = false;
  if (resize_to_pot_) {
    result = UpdateBackedMipLevel(level, face);
  } else {
    result = HR(d3d_cube_texture_->UnlockRect(DX9CubeFace(face),
                                              level));
  }
  if (result) {
    locked_levels_[face] &= ~(1 << level);
  } else {
    O3D_ERROR(service_locator()) << "Failed to Unlock Texture2D (D3D9)";
  }
  return result;
}

bool TextureCUBED3D9::OnLostDevice() {
  // Textures created with RenderSurface support are placed in the default
  // pool, so release them here.
  if (render_surfaces_enabled()) {
    d3d_cube_texture_ = NULL;
  }
  return true;
}

bool TextureCUBED3D9::OnResetDevice() {
  if (render_surfaces_enabled()) {
    DCHECK(d3d_cube_texture_ == NULL);
    Renderer* renderer = service_locator()->GetService<Renderer>();
    RendererD3D9 *renderer_d3d9 = down_cast<RendererD3D9*>(renderer);
    bool resize_to_pot;
    unsigned int mip_edge;
    return HR(CreateTextureCUBED3D9(renderer_d3d9,
                                    &backing_bitmap_,
                                    render_surfaces_enabled(),
                                    &resize_to_pot,
                                    &mip_edge,
                                    &d3d_cube_texture_));
  }
  return true;
}

const Texture::RGBASwizzleIndices& TextureCUBED3D9::GetABGR32FSwizzleIndices() {
  return g_d3d_abgr32f_swizzle_indices;
}

}  // namespace o3d
