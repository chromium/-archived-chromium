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


#ifndef O3D_COMMAND_BUFFER_SERVICE_WIN_D3D9_TEXTURE_D3D9_H__
#define O3D_COMMAND_BUFFER_SERVICE_WIN_D3D9_TEXTURE_D3D9_H__

// This file contains the definition of the D3D9 versions of texture-related
// resource classes.

#include "command_buffer/common/cross/gapi_interface.h"
#include "command_buffer/service/win/d3d9/d3d9_utils.h"
#include "command_buffer/service/cross/resource.h"
#include "command_buffer/service/cross/texture_utils.h"

namespace o3d {
namespace command_buffer {

class GAPID3D9;

// The base class for a D3D texture resource, providing access to the base D3D
// texture that can be assigned to an effect parameter or a sampler unit.
class TextureD3D9 : public Texture {
 public:
  TextureD3D9(texture::Type type,
              unsigned int levels,
              texture::Format format,
              unsigned int flags)
      : Texture(type, levels, format, flags) {}
  // Gets the D3D base texture.
  virtual IDirect3DBaseTexture9 *d3d_base_texture() const = 0;
  // Sets data into a texture resource.
  virtual bool SetData(GAPID3D9 *gapi,
                       const Volume& volume,
                       unsigned int level,
                       texture::Face face,
                       unsigned int row_pitch,
                       unsigned int slice_pitch,
                       unsigned int size,
                       const void *data) = 0;
  // Gets data from a texture resource.
  virtual bool GetData(GAPID3D9 *gapi,
                       const Volume& volume,
                       unsigned int level,
                       texture::Face face,
                       unsigned int row_pitch,
                       unsigned int slice_pitch,
                       unsigned int size,
                       void *data) = 0;
 private:
  DISALLOW_COPY_AND_ASSIGN(TextureD3D9);
};

// A 2D texture resource for D3D.
class Texture2DD3D9 : public TextureD3D9 {
 public:
  Texture2DD3D9(unsigned int levels,
                texture::Format format,
                unsigned int flags,
                unsigned int width,
                unsigned int height,
                IDirect3DTexture9 *texture,
                IDirect3DTexture9 *shadow)
      : TextureD3D9(texture::TEXTURE_2D, levels, format, flags),
        width_(width),
        height_(height),
        d3d_texture_(texture),
        d3d_shadow_(shadow) {}
  virtual ~Texture2DD3D9();

  // Creates a 2D texture resource.
  static Texture2DD3D9 *Create(GAPID3D9 *gapi,
                               unsigned int width,
                               unsigned int height,
                               unsigned int levels,
                               texture::Format format,
                               unsigned int flags);
  // Sets data into a 2D texture resource.
  virtual bool SetData(GAPID3D9 *gapi,
                       const Volume& volume,
                       unsigned int level,
                       texture::Face face,
                       unsigned int row_pitch,
                       unsigned int slice_pitch,
                       unsigned int size,
                       const void *data);
  // Gets data from a 2D texture resource.
  virtual bool GetData(GAPID3D9 *gapi,
                       const Volume& volume,
                       unsigned int level,
                       texture::Face face,
                       unsigned int row_pitch,
                       unsigned int slice_pitch,
                       unsigned int size,
                       void *data);
  // Gets the D3D base texture.
  virtual IDirect3DBaseTexture9 *d3d_base_texture() const {
    return d3d_texture_;
  }
 private:
  unsigned int width_;
  unsigned int height_;
  IDirect3DTexture9 *d3d_texture_;
  IDirect3DTexture9 *d3d_shadow_;
  DISALLOW_COPY_AND_ASSIGN(Texture2DD3D9);
};

// A 3D texture resource for D3D.
class Texture3DD3D9 : public TextureD3D9 {
 public:
  Texture3DD3D9(unsigned int levels,
                texture::Format format,
                unsigned int flags,
                unsigned int width,
                unsigned int height,
                unsigned int depth,
                IDirect3DVolumeTexture9 *texture,
                IDirect3DVolumeTexture9 *shadow)
      : TextureD3D9(texture::TEXTURE_2D, levels, format, flags),
        width_(width),
        height_(height),
        depth_(depth),
        d3d_texture_(texture),
        d3d_shadow_(shadow) {}
  virtual ~Texture3DD3D9();
  // Creates a 3D texture resource.
  static Texture3DD3D9 *Create(GAPID3D9 *gapi,
                               unsigned int width,
                               unsigned int height,
                               unsigned int depth,
                               unsigned int levels,
                               texture::Format format,
                               unsigned int flags);
  // Sets data into a 3D texture resource.
  virtual bool SetData(GAPID3D9 *gapi,
                       const Volume& volume,
                       unsigned int level,
                       texture::Face face,
                       unsigned int row_pitch,
                       unsigned int slice_pitch,
                       unsigned int size,
                       const void *data);
  // Gets data from a 3D texture resource.
  virtual bool GetData(GAPID3D9 *gapi,
                       const Volume& volume,
                       unsigned int level,
                       texture::Face face,
                       unsigned int row_pitch,
                       unsigned int slice_pitch,
                       unsigned int size,
                       void *data);
  // Gets the D3D base texture.
  virtual IDirect3DBaseTexture9 *d3d_base_texture() const {
    return d3d_texture_;
  }
 private:
  unsigned int width_;
  unsigned int height_;
  unsigned int depth_;
  IDirect3DVolumeTexture9 *d3d_texture_;
  IDirect3DVolumeTexture9 *d3d_shadow_;
  DISALLOW_COPY_AND_ASSIGN(Texture3DD3D9);
};

// A cube map texture resource for D3D.
class TextureCubeD3D9 : public TextureD3D9 {
 public:
  TextureCubeD3D9(unsigned int levels,
                  texture::Format format,
                  unsigned int flags,
                  unsigned int side,
                  IDirect3DCubeTexture9 *texture,
                  IDirect3DCubeTexture9 *shadow)
      : TextureD3D9(texture::TEXTURE_CUBE, levels, format, flags),
        side_(side),
        d3d_texture_(texture),
        d3d_shadow_(shadow) {}
  virtual ~TextureCubeD3D9();
  // Creates a cube map texture resource.
  static TextureCubeD3D9 *Create(GAPID3D9 *gapi,
                                 unsigned int side,
                                 unsigned int levels,
                                 texture::Format format,
                                 unsigned int flags);
  // Sets data into a cube map texture resource.
  virtual bool SetData(GAPID3D9 *gapi,
                       const Volume& volume,
                       unsigned int level,
                       texture::Face face,
                       unsigned int row_pitch,
                       unsigned int slice_pitch,
                       unsigned int size,
                       const void *data);
  // Gets data from a cube map texture resource.
  virtual bool GetData(GAPID3D9 *gapi,
                       const Volume& volume,
                       unsigned int level,
                       texture::Face face,
                       unsigned int row_pitch,
                       unsigned int slice_pitch,
                       unsigned int size,
                       void *data);
  // Gets the D3D base texture.
  virtual IDirect3DBaseTexture9 *d3d_base_texture() const {
    return d3d_texture_;
  }
 private:
  unsigned int side_;
  IDirect3DCubeTexture9 *d3d_texture_;
  IDirect3DCubeTexture9 *d3d_shadow_;
  DISALLOW_COPY_AND_ASSIGN(TextureCubeD3D9);
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SERVICE_WIN_D3D9_TEXTURE_D3D9_H__
