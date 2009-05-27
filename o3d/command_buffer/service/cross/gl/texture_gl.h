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


// This file declares the TextureGL, Texture2DGL, Texture3DGL and TextureCubeGL
// classes.

#ifndef O3D_COMMAND_BUFFER_SERVICE_CROSS_GL_TEXTURE_GL_H_
#define O3D_COMMAND_BUFFER_SERVICE_CROSS_GL_TEXTURE_GL_H_

#include "command_buffer/common/cross/gapi_interface.h"
#include "command_buffer/service/cross/gl/gl_utils.h"
#include "command_buffer/service/cross/resource.h"
#include "command_buffer/service/cross/texture_utils.h"

namespace o3d {
namespace command_buffer {

// The base class for a GL texture resource, providing access to the base GL
// texture that can be assigned to an effect parameter or a sampler unit.
class TextureGL : public Texture {
 public:
  TextureGL(texture::Type type,
            unsigned int levels,
            texture::Format format,
            unsigned int flags,
            GLuint gl_texture)
      : Texture(type, levels, format, flags),
        gl_texture_(gl_texture) {}
  virtual ~TextureGL();

  // Gets the GL texture object.
  GLuint gl_texture() const { return gl_texture_; }

  // Sets data into a texture resource.
  virtual bool SetData(const Volume& volume,
                       unsigned int level,
                       texture::Face face,
                       unsigned int row_pitch,
                       unsigned int slice_pitch,
                       unsigned int size,
                       const void *data) = 0;

  // Gets data from a texture resource.
  virtual bool GetData(const Volume& volume,
                       unsigned int level,
                       texture::Face face,
                       unsigned int row_pitch,
                       unsigned int slice_pitch,
                       unsigned int size,
                       void *data) = 0;

 protected:
  const GLuint gl_texture_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TextureGL);
};

// A 2D texture resource for GL.
class Texture2DGL : public TextureGL {
 public:
  Texture2DGL(unsigned int levels,
              texture::Format format,
              unsigned int flags,
              unsigned int width,
              unsigned int height,
              GLuint gl_texture)
      : TextureGL(texture::TEXTURE_2D, levels, format, flags, gl_texture),
        width_(width),
        height_(height) {}

  // Creates a 2D texture resource.
  static Texture2DGL *Create(unsigned int width,
                             unsigned int height,
                             unsigned int levels,
                             texture::Format format,
                             unsigned int flags);

  // Sets data into a 2D texture resource.
  virtual bool SetData(const Volume& volume,
                       unsigned int level,
                       texture::Face face,
                       unsigned int row_pitch,
                       unsigned int slice_pitch,
                       unsigned int size,
                       const void *data);

  // Gets data from a 2D texture resource.
  virtual bool GetData(const Volume& volume,
                       unsigned int level,
                       texture::Face face,
                       unsigned int row_pitch,
                       unsigned int slice_pitch,
                       unsigned int size,
                       void *data);

 private:
  unsigned int width_;
  unsigned int height_;
  DISALLOW_COPY_AND_ASSIGN(Texture2DGL);
};

// A 3D texture resource for GL.
class Texture3DGL : public TextureGL {
 public:
  Texture3DGL(unsigned int levels,
              texture::Format format,
              unsigned int flags,
              unsigned int width,
              unsigned int height,
              unsigned int depth,
              GLuint gl_texture)
      : TextureGL(texture::TEXTURE_2D, levels, format, flags, gl_texture),
        width_(width),
        height_(height),
        depth_(depth) {}

  // Creates a 3D texture resource.
  static Texture3DGL *Create(unsigned int width,
                             unsigned int height,
                             unsigned int depth,
                             unsigned int levels,
                             texture::Format format,
                             unsigned int flags);

  // Sets data into a 3D texture resource.
  virtual bool SetData(const Volume& volume,
                       unsigned int level,
                       texture::Face face,
                       unsigned int row_pitch,
                       unsigned int slice_pitch,
                       unsigned int size,
                       const void *data);

  // Gets data from a 3D texture resource.
  virtual bool GetData(const Volume& volume,
                       unsigned int level,
                       texture::Face face,
                       unsigned int row_pitch,
                       unsigned int slice_pitch,
                       unsigned int size,
                       void *data);

 private:
  unsigned int width_;
  unsigned int height_;
  unsigned int depth_;
  DISALLOW_COPY_AND_ASSIGN(Texture3DGL);
};

// A cube map texture resource for GL.
class TextureCubeGL : public TextureGL {
 public:
  TextureCubeGL(unsigned int levels,
                texture::Format format,
                unsigned int flags,
                unsigned int side,
                GLuint gl_texture)
      : TextureGL(texture::TEXTURE_CUBE, levels, format, flags, gl_texture),
        side_(side) {}

  // Creates a cube map texture resource.
  static TextureCubeGL *Create(unsigned int side,
                               unsigned int levels,
                               texture::Format format,
                               unsigned int flags);

  // Sets data into a cube map texture resource.
  virtual bool SetData(const Volume& volume,
                       unsigned int level,
                       texture::Face face,
                       unsigned int row_pitch,
                       unsigned int slice_pitch,
                       unsigned int size,
                       const void *data);

  // Gets data from a cube map texture resource.
  virtual bool GetData(const Volume& volume,
                       unsigned int level,
                       texture::Face face,
                       unsigned int row_pitch,
                       unsigned int slice_pitch,
                       unsigned int size,
                       void *data);

 private:
  unsigned int side_;
  DISALLOW_COPY_AND_ASSIGN(TextureCubeGL);
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SERVICE_CROSS_GL_TEXTURE_GL_H_
