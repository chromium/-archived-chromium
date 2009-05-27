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


// File containing a base class for texture objects.

#ifndef O3D_CORE_CROSS_TEXTURE_BASE_H_
#define O3D_CORE_CROSS_TEXTURE_BASE_H_

#include "core/cross/param.h"
#include "core/cross/param_object.h"

namespace o3d {

class Pack;
class Renderer;
class RenderSurface;

// The Texture class is a base class for image data used in texture
// mapping.  It is an abstract class.  Concrete implementations should
// implement the GetTextureHandle() member function.
class Texture : public ParamObject {
 public:
  typedef SmartPointer<Texture> Ref;
  typedef WeakPointer<Texture> WeakPointerType;

  enum Type { TEXTURE_1D, TEXTURE_2D, TEXTURE_3D, TEXTURE_CUBE };

  enum Format {
    UNKNOWN_FORMAT,
    XRGB8,  // actual format in memory is B G R X
    ARGB8,  // actual format in memory is B G R A
    ABGR16F,
    R32F,
    ABGR32F,
    DXT1,
    DXT3,
    DXT5
  };

  typedef unsigned RGBASwizzleIndices[4];

  // This is the maximum texture size we allow and hence the largest
  // render target and depth stencil as well. This is because it's the limit of
  // some low-end machines that are still pretty common.
  //
  // NOTE: class Bitmap supports a larger size. The plan is to expose Bitmap
  // to Javascript so you can download larger images, scale them, then put
  // them in a texture.
  static const int MAX_DIMENSION = 2048;

  Texture(ServiceLocator* service_locator,
          Format format,
          int levels,
          bool alpha_is_one,
          bool resize_to_pot,
          bool enable_render_surfaces);
  virtual ~Texture() {}

  static const char* kLevelsParamName;

  // Returns the implementation-specific texture handle.
  virtual void* GetTextureHandle() const = 0;

  bool alpha_is_one() const { return alpha_is_one_; }
  void set_alpha_is_one(bool value)  { alpha_is_one_ = value; }

  // Gets the levels.
  int levels() const {
    return levels_param_->value();
  }

  // Gets the format of the texture resource.
  Format format() const {
    return format_;
  }

  bool render_surfaces_enabled() const {
    return render_surfaces_enabled_;
  }

  // Gets a RGBASwizzleIndices that contains a mapping from
  // RGBA to the internal format used by the graphics API.
  virtual const RGBASwizzleIndices& GetABGR32FSwizzleIndices() = 0;

  // Gets a weak pointer to us.
  WeakPointerType GetWeakPointer() const {
    return weak_pointer_manager_.GetWeakPointer();
  }

 protected:
  void set_levels(int levels) {
    levels_param_->set_read_only_value(levels);
  }

  void set_format(Format format) {
    format_ = format;
  }

  // Whether or not to resize NPOT textures to POT when passing to the
  // underlying graphics API.
  bool resize_to_pot_;

  static void RegisterSurface(RenderSurface* surface, Pack* pack);

 private:
  // The number of mipmap levels contained in this texture.
  ParamInteger::Ref levels_param_;

  // true if all the alpha values in this texture are 1.0.
  bool alpha_is_one_;

  // The data format of each pixel.
  Format format_;

  // Manager for weak pointers to us.
  WeakPointerType::WeakPointerManager weak_pointer_manager_;

  bool render_surfaces_enabled_;

  O3D_DECL_CLASS(Texture, ParamObject);
  DISALLOW_COPY_AND_ASSIGN(Texture);
};

class ParamTexture : public TypedRefParam<Texture> {
 public:
  typedef SmartPointer<ParamTexture> Ref;

  ParamTexture(ServiceLocator* service_locator, bool dynamic, bool read_only)
      : TypedRefParam<Texture>(service_locator, dynamic, read_only) {}

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(ParamTexture, RefParamBase)
  DISALLOW_COPY_AND_ASSIGN(ParamTexture);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_TEXTURE_BASE_H_
