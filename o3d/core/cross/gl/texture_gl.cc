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


// Implementations of the abstract Texture2D and TextureCUBE classes using
// the OpenGL graphics API.

#include "core/cross/precompile.h"
#include "core/cross/error.h"
#include "core/cross/types.h"
#include "core/cross/gl/renderer_gl.h"
#include "core/cross/gl/render_surface_gl.h"
#include "core/cross/gl/texture_gl.h"
#include "core/cross/gl/utils_gl.h"
#include "core/cross/gl/utils_gl-inl.h"

namespace o3d {

namespace {

Texture::RGBASwizzleIndices g_gl_abgr32f_swizzle_indices = {0, 1, 2, 3};

}  // anonymous namespace.

// Converts an O3D texture format to a GL texture format.
// Input is 'format'.
// GL has 2 notions of the format:
// - the internal format which describes how the format should be stored on the
// GPU
// - the (format, type) pair which describes how the input data to glTexImage2D
// is laid out. If format is 0, the data is compressed and needs to be passed
// to glCompressedTexImage2D.
// The internal format is returned in internal_format.
// The format is the return value of the function.
// The type is returned in data_type.
static GLenum GLFormatFromO3DFormat(Texture::Format format,
                                    GLenum *internal_format,
                                    GLenum *data_type) {
  switch (format) {
    case Texture::XRGB8: {
      *internal_format = GL_RGB;
      *data_type = GL_UNSIGNED_BYTE;
      return GL_BGRA;
    }
    case Texture::ARGB8: {
      *internal_format = GL_RGBA;
      *data_type = GL_UNSIGNED_BYTE;
      return GL_BGRA;
    }
    case Texture::ABGR16F: {
      *internal_format = GL_RGBA16F_ARB;
      *data_type = GL_HALF_FLOAT_ARB;
      return GL_RGBA;
    }
    case Texture::R32F: {
      *internal_format = GL_LUMINANCE32F_ARB;
      *data_type = GL_FLOAT;
      return GL_LUMINANCE;
    }
    case Texture::ABGR32F: {
      *internal_format = GL_RGBA32F_ARB;
      *data_type = GL_FLOAT;
      return GL_BGRA;
    }
    case Texture::DXT1: {
      if (GL_EXT_texture_compression_s3tc) {
        *internal_format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        *data_type = 0;
        return 0;
      } else {
        // TODO: we need to convert DXT1 -> RGBA8 but keep around the
        // pixels so that we can read them back (we won't try to convert back
        // to DXTC).
        LOG(ERROR) << "DXT1 compressed textures not supported yet.";
        *internal_format = 0;
        *data_type = GL_BYTE;
        return 0;
      }
    }
    case Texture::DXT3: {
      if (GL_EXT_texture_compression_s3tc) {
        *internal_format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        *data_type = 0;
        return 0;
      } else {
        // TODO: we need to convert DXT3 -> RGBA8 but keep around the
        // pixels so that we can read them back (we won't try to convert back
        // to DXTC).
        LOG(ERROR) << "DXT3 compressed textures not supported yet.";
        *internal_format = 0;
        *data_type = GL_BYTE;
        return 0;
      }
    }
    case Texture::DXT5: {
      if (GL_EXT_texture_compression_s3tc) {
        *internal_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        *data_type = 0;
        return 0;
      } else {
        // TODO: we need to convert DXT3 -> RGBA8 but keep around the
        // pixels so that we can read them back (we won't try to convert back
        // to DXTC).
        LOG(ERROR) << "DXT5 compressed textures not supported yet.";
        *internal_format = 0;
        *data_type = GL_BYTE;
        return 0;
      }
    }
  }
  // failed to find a matching format
  LOG(ERROR) << "Unrecognized Texture format type.";
  *internal_format = 0;
  *data_type = 0;
  return 0;
}

// Updates a GL image from a bitmap, rescaling if necessary.
static bool UpdateGLImageFromBitmap(GLenum target,
                                    unsigned int level,
                                    TextureCUBE::CubeFace face,
                                    const Bitmap &bitmap,
                                    bool resize_to_pot) {
  DCHECK(bitmap.image_data());
  unsigned int mip_width = std::max(1U, bitmap.width() >> level);
  unsigned int mip_height = std::max(1U, bitmap.height() >> level);
  const unsigned char *mip_data = bitmap.GetMipData(level, face);
  unsigned int mip_size =
      Bitmap::GetBufferSize(mip_width, mip_height, bitmap.format());
  scoped_array<unsigned char> temp_data;
  if (resize_to_pot) {
    unsigned int pot_width =
        std::max(1U, Bitmap::GetPOTSize(bitmap.width()) >> level);
    unsigned int pot_height =
        std::max(1U, Bitmap::GetPOTSize(bitmap.height()) >> level);
    unsigned int pot_size = Bitmap::GetBufferSize(pot_width, pot_height,
                                                  bitmap.format());
    temp_data.reset(new unsigned char[pot_size]);
    Bitmap::Scale(mip_width, mip_height, bitmap.format(), mip_data,
                  pot_width, pot_height, temp_data.get());
    mip_width = pot_width;
    mip_height = pot_height;
    mip_size = pot_size;
    mip_data = temp_data.get();
  }
  GLenum gl_internal_format = 0;
  GLenum gl_data_type = 0;
  GLenum gl_format = GLFormatFromO3DFormat(bitmap.format(), &gl_internal_format,
                                           &gl_data_type);
  if (gl_format) {
    glTexSubImage2D(target, level, 0, 0, mip_width, mip_height,
                    gl_format, gl_data_type, mip_data);
  } else {
    glCompressedTexSubImage2D(target, level, 0, 0, mip_width, mip_height,
                              gl_internal_format, mip_size, mip_data);
  }
  return glGetError() == GL_NO_ERROR;
}

// Creates the array of GL images for a particular face and upload the pixel
// data from the bitmap.
static bool CreateGLImagesAndUpload(GLenum target,
                                    GLenum internal_format,
                                    GLenum format,
                                    GLenum type,
                                    TextureCUBE::CubeFace face,
                                    const Bitmap &bitmap,
                                    bool resize_to_pot) {
  unsigned int mip_width = bitmap.width();
  unsigned int mip_height = bitmap.height();
  if (resize_to_pot) {
    mip_width = Bitmap::GetPOTSize(mip_width);
    mip_height = Bitmap::GetPOTSize(mip_height);
  }
  // glCompressedTexImage2D does't accept NULL as a parameter, so we need
  // to pass in some data. If we can pass in the original pixel data, we'll
  // do that, otherwise we'll pass an empty buffer. In that case, prepare it
  // here once for all.
  scoped_array<unsigned char> temp_data;
  if (!format && (!bitmap.image_data() || resize_to_pot)) {
    // Allocate a buffer big enough for the first level which is the biggest
    // one.
    unsigned int size = Bitmap::GetBufferSize(mip_width, mip_height,
                                              bitmap.format());
    temp_data.reset(new unsigned char[size]);
    memset(temp_data.get(), 0, size);
  }
  for (unsigned int i = 0; i < bitmap.num_mipmaps(); ++i) {
    // Upload pixels directly if we can, otherwise it will be done with
    // UpdateGLImageFromBitmap afterwards.
    unsigned char *data = resize_to_pot ? NULL : bitmap.GetMipData(i, face);

    if (format) {
      glTexImage2D(target, i, internal_format, mip_width, mip_height,
                   0, format, type, data);
      if (glGetError() != GL_NO_ERROR) {
        DLOG(ERROR) << "glTexImage2D failed";
        return false;
      }
    } else {
      unsigned int mip_size = Bitmap::GetBufferSize(mip_width, mip_height,
                                                    bitmap.format());
      DCHECK(data || temp_data.get());
      glCompressedTexImage2DARB(target, i, internal_format, mip_width,
                                mip_height, 0, mip_size,
                                data ? data : temp_data.get());
      if (glGetError() != GL_NO_ERROR) {
        DLOG(ERROR) << "glCompressedTexImage2D failed";
        return false;
      }
    }
    if (resize_to_pot && bitmap.image_data()) {
      if (!UpdateGLImageFromBitmap(target, i, face, bitmap, true)) {
        DLOG(ERROR) << "UpdateGLImageFromBitmap failed";
        return false;
      }
    }
    mip_width = std::max(1U, mip_width >> 1);
    mip_height = std::max(1U, mip_height >> 1);
  }
  return true;
}

// Texture2DGL -----------------------------------------------------------------

// Constructs a 2D texture object from an existing OpenGL 2D texture.
// NOTE: the Texture2DGL now owns the GL texture and will destroy it on exit.
Texture2DGL::Texture2DGL(ServiceLocator* service_locator,
                         GLint texture,
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
      renderer_(static_cast<RendererGL*>(
          service_locator->GetService<Renderer>())),
      gl_texture_(texture),
      has_levels_(0) {
  DLOG(INFO) << "Texture2DGL Construct from GLint";
  DCHECK_NE(format(), Texture::UNKNOWN_FORMAT);
}

// Creates a new texture object from scratch.
Texture2DGL* Texture2DGL::Create(ServiceLocator* service_locator,
                                 Bitmap *bitmap,
                                 bool enable_render_surfaces) {
  DLOG(INFO) << "Texture2DGL Create";
  DCHECK_NE(bitmap->format(), Texture::UNKNOWN_FORMAT);
  DCHECK(!bitmap->is_cubemap());
  RendererGL *renderer = static_cast<RendererGL *>(
      service_locator->GetService<Renderer>());
  renderer->MakeCurrentLazy();
  GLenum gl_internal_format = 0;
  GLenum gl_data_type = 0;
  GLenum gl_format = GLFormatFromO3DFormat(bitmap->format(),
                                           &gl_internal_format,
                                           &gl_data_type);
  if (gl_internal_format == 0) {
    DLOG(ERROR) << "Unsupported format in Texture2DGL::Create.";
    return NULL;
  }

  bool resize_to_pot = !renderer->supports_npot() && !bitmap->IsPOT();

  // Creates the OpenGL texture object, with all the required mip levels.
  GLuint gl_texture = 0;
  glGenTextures(1, &gl_texture);
  glBindTexture(GL_TEXTURE_2D, gl_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,
                  bitmap->num_mipmaps()-1);

  if (!CreateGLImagesAndUpload(GL_TEXTURE_2D, gl_internal_format, gl_format,
                               gl_data_type, TextureCUBE::FACE_POSITIVE_X,
                               *bitmap, resize_to_pot)) {
    DLOG(ERROR) << "Failed to create texture images.";
    glDeleteTextures(1, &gl_texture);
    return NULL;
  }
  glTexParameteri(GL_TEXTURE_CUBE_MAP,
                  GL_TEXTURE_MIN_FILTER,
                  GL_NEAREST_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  GLint gl_width;
  GLint gl_height;
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &gl_width);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &gl_height);

  DLOG(INFO) << "Created 2D texture (size=" << gl_width << "x" << gl_height
             << ", GLuint=" << gl_texture << ")";
  Texture2DGL *texture = new Texture2DGL(service_locator,
                                         gl_texture,
                                         *bitmap,
                                         resize_to_pot,
                                         enable_render_surfaces);

  // Setup the backing bitmap.
  texture->backing_bitmap_.SetFrom(bitmap);
  if (texture->backing_bitmap_.image_data()) {
    if (resize_to_pot) {
      texture->has_levels_ = (1 << bitmap->num_mipmaps()) - 1;
    } else {
      texture->backing_bitmap_.FreeData();
    }
  } else {
    // If no backing store was provided to the routine, and the hardware does
    // not support npot textures, allocate a 0-initialized mip-chain here
    // for use during Texture2DGL::Lock.
    if (resize_to_pot) {
      texture->backing_bitmap_.AllocateData();
      memset(texture->backing_bitmap_.image_data(), 0,
             texture->backing_bitmap_.GetTotalSize());
      texture->has_levels_ = (1 << bitmap->num_mipmaps()) - 1;
    }
  }
  CHECK_GL_ERROR();
  return texture;
}

void Texture2DGL::UpdateBackedMipLevel(unsigned int level) {
  DCHECK_LT(level, levels());
  DCHECK(backing_bitmap_.image_data());
  DCHECK_EQ(backing_bitmap_.width(), width());
  DCHECK_EQ(backing_bitmap_.height(), height());
  DCHECK_EQ(backing_bitmap_.format(), format());
  DCHECK(HasLevel(level));
  glBindTexture(GL_TEXTURE_2D, gl_texture_);
  UpdateGLImageFromBitmap(GL_TEXTURE_2D, level, TextureCUBE::FACE_POSITIVE_X,
                          backing_bitmap_, resize_to_pot_);
}

Texture2DGL::~Texture2DGL() {
  DLOG(INFO) << "Texture2DGL Destruct";
  if (gl_texture_) {
    renderer_->MakeCurrentLazy();
    glDeleteTextures(1, &gl_texture_);
    gl_texture_ = 0;
  }
  CHECK_GL_ERROR();
}

// Locks the given mipmap level of this texture for loading from main memory,
// and returns a pointer to the buffer.
bool Texture2DGL::Lock(int level, void** data) {
  DLOG(INFO) << "Texture2DGL Lock";
  renderer_->MakeCurrentLazy();
  if (level >= levels() || level < 0) {
    O3D_ERROR(service_locator())
        << "Trying to lock inexistent level " << level
        << " on Texture \"" << name();
    return false;
  }
  if (IsLocked(level)) {
    O3D_ERROR(service_locator())
        << "Level " << level << " of texture \"" << name()
        << "\" is already locked.";
    return false;
  }
  if (!backing_bitmap_.image_data()) {
    DCHECK_EQ(has_levels_, 0);
    backing_bitmap_.Allocate(format(), width(), height(), levels(), false);
  }
  *data = backing_bitmap_.GetMipData(level, TextureCUBE::FACE_POSITIVE_X);
  if (!HasLevel(level)) {
    // TODO: add some API so we don't have to copy back the data if we
    // will rewrite it all.
    DCHECK(!resize_to_pot_);
    GLenum gl_internal_format = 0;
    GLenum gl_data_type = 0;
    GLenum gl_format = GLFormatFromO3DFormat(format(),
                                             &gl_internal_format,
                                             &gl_data_type);
    glBindTexture(GL_TEXTURE_2D, gl_texture_);
    glGetTexImage(GL_TEXTURE_2D, level, gl_format, gl_data_type, *data);
    has_levels_ |= 1 << level;
  }
  locked_levels_ |= 1 << level;
  CHECK_GL_ERROR();
  return true;
}

// Unlocks the given mipmap level of this texture, uploading the main memory
// data buffer to GL.
bool Texture2DGL::Unlock(int level) {
  DLOG(INFO) << "Texture2DGL Unlock";
  renderer_->MakeCurrentLazy();
  if (level >= levels() || level < 0) {
    O3D_ERROR(service_locator())
        << "Trying to unlock inexistent level " << level
        << " on Texture \"" << name();
    return false;
  }
  if (!IsLocked(level)) {
    O3D_ERROR(service_locator())
        << "Level " << level << " of texture \"" << name()
        << "\" is not locked.";
    return false;
  }
  UpdateBackedMipLevel(level);
  locked_levels_ &= ~(1 << level);
  if (!resize_to_pot_ && (locked_levels_ == 0)) {
    backing_bitmap_.FreeData();
    has_levels_ = 0;
  }
  CHECK_GL_ERROR();
  return true;
}

RenderSurface::Ref Texture2DGL::GetRenderSurface(int mip_level, Pack *pack) {
  DCHECK_LT(mip_level, levels());
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

  RenderSurface::Ref render_surface(new RenderSurfaceGL(
      service_locator(),
      width()>> mip_level,
      height() >> mip_level,
      0,
      mip_level,
      this));

  if (!render_surface.IsNull()) {
    RegisterSurface(render_surface.Get(), pack);
  }

  return render_surface;
}

const Texture::RGBASwizzleIndices& Texture2DGL::GetABGR32FSwizzleIndices() {
  return g_gl_abgr32f_swizzle_indices;
}

// TextureCUBEGL ---------------------------------------------------------------

// Creates a texture from a pre-existing GL texture object.
TextureCUBEGL::TextureCUBEGL(ServiceLocator* service_locator,
                             GLint texture,
                             const Bitmap &bitmap,
                             bool resize_to_pot,
                             bool enable_render_surfaces)
    : TextureCUBE(service_locator,
                  bitmap.width(),
                  bitmap.format(),
                  bitmap.num_mipmaps(),
                  bitmap.CheckAlphaIsOne(),
                  resize_to_pot,
                  enable_render_surfaces),
      renderer_(static_cast<RendererGL*>(
          service_locator->GetService<Renderer>())),
      gl_texture_(texture) {
  DLOG(INFO) << "TextureCUBEGL Construct";
  for (unsigned int i = 0; i < 6; ++i) {
    has_levels_[i] = 0;
  }
}

TextureCUBEGL::~TextureCUBEGL() {
  DLOG(INFO) << "TextureCUBEGL Destruct";
  if (gl_texture_) {
    renderer_->MakeCurrentLazy();
    glDeleteTextures(1, &gl_texture_);
    gl_texture_ = 0;
  }
  CHECK_GL_ERROR();
}

static const int kCubemapFaceList[] = {
  GL_TEXTURE_CUBE_MAP_POSITIVE_X,
  GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
  GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
  GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
  GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
  GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
};

// Create a new Cube texture from scratch.
TextureCUBEGL* TextureCUBEGL::Create(ServiceLocator* service_locator,
                                     Bitmap *bitmap,
                                     bool enable_render_surfaces) {
  DLOG(INFO) << "TextureCUBEGL Create";
  CHECK_GL_ERROR();
  DCHECK(bitmap->is_cubemap());
  DCHECK_EQ(bitmap->width(), bitmap->height());
  RendererGL *renderer = static_cast<RendererGL *>(
      service_locator->GetService<Renderer>());
  renderer->MakeCurrentLazy();

  bool resize_to_pot = !renderer->supports_npot() && !bitmap->IsPOT();

  // Get gl formats
  GLenum gl_internal_format = 0;
  GLenum gl_data_type = 0;
  GLenum gl_format = GLFormatFromO3DFormat(bitmap->format(),
                                           &gl_internal_format,
                                           &gl_data_type);
  if (gl_internal_format == 0) {
    DLOG(ERROR) << "Unsupported format in TextureCUBEGL::Create.";
    return NULL;
  }

  // Creates the OpenGL texture object, with all the required mip levels.
  GLuint gl_texture = 0;
  glGenTextures(1, &gl_texture);
  glBindTexture(GL_TEXTURE_CUBE_MAP, gl_texture);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL,
                  bitmap->num_mipmaps()-1);

  for (int face = 0; face < 6; ++face) {
    CreateGLImagesAndUpload(kCubemapFaceList[face], gl_internal_format,
                            gl_format, gl_data_type,
                            static_cast<CubeFace>(face), *bitmap,
                            resize_to_pot);
  }
  glTexParameteri(GL_TEXTURE_2D,
                  GL_TEXTURE_MIN_FILTER,
                  GL_NEAREST_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // Create a new texture object, which initializes the base Texture class
  // from the Bitmap information.
  TextureCUBEGL* texture = new TextureCUBEGL(service_locator,
                                             gl_texture,
                                             *bitmap,
                                             resize_to_pot,
                                             enable_render_surfaces);
  // Setup the backing bitmap, and upload the data if we have any.
  texture->backing_bitmap_.SetFrom(bitmap);
  if (texture->backing_bitmap_.image_data()) {
    if (resize_to_pot) {
      for (unsigned int face = 0; face < 6; ++face) {
        texture->has_levels_[face] = (1 << bitmap->num_mipmaps()) - 1;
      }
    } else {
      texture->backing_bitmap_.FreeData();
    }
  } else {
    // If no backing store was provided to the routine, and the hardware does
    // not support npot textures, allocate a 0-initialized mip-chain here
    // for use during TextureCUBEGL::Lock.
    if (resize_to_pot) {
      texture->backing_bitmap_.AllocateData();
      memset(texture->backing_bitmap_.image_data(), 0,
             texture->backing_bitmap_.GetTotalSize());
      for (unsigned int face = 0; face < 6; ++face) {
        texture->has_levels_[face] = (1 << bitmap->num_mipmaps()) - 1;
      }
    }
  }
  CHECK_GL_ERROR();
  DLOG(INFO) << "Created cube map texture (GLuint=" << gl_texture << ")";
  return texture;
}

void TextureCUBEGL::UpdateBackedMipLevel(unsigned int level,
                                         TextureCUBE::CubeFace face) {
  DCHECK_LT(level, levels());
  DCHECK(backing_bitmap_.image_data());
  DCHECK(backing_bitmap_.is_cubemap());
  DCHECK_EQ(backing_bitmap_.width(), edge_length());
  DCHECK_EQ(backing_bitmap_.height(), edge_length());
  DCHECK_EQ(backing_bitmap_.format(), format());
  DCHECK(HasLevel(level, face));
  glBindTexture(GL_TEXTURE_2D, gl_texture_);
  UpdateGLImageFromBitmap(kCubemapFaceList[face], level, face, backing_bitmap_,
                          resize_to_pot_);
}

RenderSurface::Ref TextureCUBEGL::GetRenderSurface(TextureCUBE::CubeFace face,
                                                   int mip_level,
                                                   Pack *pack) {
  DCHECK_LT(mip_level, levels());
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

  RenderSurface::Ref render_surface(new RenderSurfaceGL(
      service_locator(),
      edge_length() >> mip_level,
      edge_length() >> mip_level,
      kCubemapFaceList[face],
      mip_level,
      this));

  if (!render_surface.IsNull()) {
    RegisterSurface(render_surface.Get(), pack);
  }

  return render_surface;
}

// Locks the given face and mipmap level of this texture for loading from
// main memory, and returns a pointer to the buffer.
bool TextureCUBEGL::Lock(CubeFace face, int level, void** data) {
  DLOG(INFO) << "TextureCUBEGL Lock";
  renderer_->MakeCurrentLazy();
  if (level >= levels() || level < 0) {
    O3D_ERROR(service_locator())
        << "Trying to lock inexistent level " << level
        << " on Texture \"" << name();
    return false;
  }
  if (IsLocked(level, face)) {
    O3D_ERROR(service_locator())
        << "Level " << level << " Face " << face
        << " of texture \"" << name()
        << "\" is already locked.";
    return false;
  }
  if (!backing_bitmap_.image_data()) {
    for (unsigned int i = 0; i < 6; ++i) {
      DCHECK_EQ(has_levels_[i], 0);
    }
    backing_bitmap_.Allocate(format(), edge_length(), edge_length(),
                             levels(), true);
  }
  *data = backing_bitmap_.GetMipData(level, face);
  GLenum gl_target = kCubemapFaceList[face];
  if (!HasLevel(level, face)) {
    // TODO: add some API so we don't have to copy back the data if we
    // will rewrite it all.
    DCHECK(!resize_to_pot_);
    GLenum gl_internal_format = 0;
    GLenum gl_data_type = 0;
    GLenum gl_format = GLFormatFromO3DFormat(format(),
                                             &gl_internal_format,
                                             &gl_data_type);
    glBindTexture(GL_TEXTURE_CUBE_MAP, gl_texture_);
    glGetTexImage(gl_target, level, gl_format, gl_data_type, *data);
    has_levels_[face] |= 1 << level;
  }
  locked_levels_[face] |= 1 << level;
  CHECK_GL_ERROR();
  return false;
}

// Unlocks the given face and mipmap level of this texture.
bool TextureCUBEGL::Unlock(CubeFace face, int level) {
  DLOG(INFO) << "TextureCUBEGL Unlock";
  renderer_->MakeCurrentLazy();
  if (level >= levels() || level < 0) {
    O3D_ERROR(service_locator())
        << "Trying to unlock inexistent level " << level
        << " on Texture \"" << name();
    return false;
  }
  if (!IsLocked(level, face)) {
    O3D_ERROR(service_locator())
        << "Level " << level << " Face " << face
        << " of texture \"" << name()
        << "\" is not locked.";
    return false;
  }
  UpdateBackedMipLevel(level, face);
  locked_levels_[face] &= ~(1 << level);
  if (!resize_to_pot_) {
    bool has_locked_level = false;
    for (unsigned int i = 0; i < 6; ++i) {
      if (locked_levels_[i]) {
        has_locked_level = true;
        break;
      }
    }
    if (!has_locked_level) {
      backing_bitmap_.FreeData();
      for (unsigned int i = 0; i < 6; ++i) {
        has_levels_[i] = 0;
      }
    }
  }
  CHECK_GL_ERROR();
  return false;
}

const Texture::RGBASwizzleIndices& TextureCUBEGL::GetABGR32FSwizzleIndices() {
  return g_gl_abgr32f_swizzle_indices;
}

}  // namespace o3d
