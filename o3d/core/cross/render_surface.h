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


// This file contains the declaration of the RenderSurface render node.

#ifndef O3D_CORE_CROSS_RENDER_SURFACE_H_
#define O3D_CORE_CROSS_RENDER_SURFACE_H_

#include "core/cross/param.h"
#include "core/cross/param_object.h"
#include "core/cross/texture_base.h"

namespace o3d {

// RenderSurfaceBase is the base class for RenderSurface and
// RenderDepthStencilSurface.
class RenderSurfaceBase : public ParamObject {
 public:
  typedef SmartPointer<RenderSurfaceBase> Ref;

  RenderSurfaceBase(ServiceLocator *service_locator,
                    int width,
                    int height);

  // Names of the RenderSurface Params.
  static const char *kWidthParamName;
  static const char *kHeightParamName;

  // Returns the width of the surface, in pixels.
  // Note that this is a read-only parameter.
  int width() const {
    return width_param_->value();
  }

  // Returns the height of the surface, in pixels.
  // Note that this is a read-only parameter.
  int height() const {
    return height_param_->value();
  }

 private:
  // The width of the surface, in pixels.
  ParamInteger::Ref width_param_;

  // The height of the surface, in pixels.
  ParamInteger::Ref height_param_;

  O3D_DECL_CLASS(RenderSurfaceBase, ParamObject);
  DISALLOW_COPY_AND_ASSIGN(RenderSurfaceBase);
};

// A RenderSurface class encapsulates the notion of a renderable surface.
// When used in conjunction with a RenderSurfaceSet node in the render graph,
// the API allows for rendering of primitives to the given surface.
// RenderSurface objects are not constructable through the Pack API, they may
// only be accessed through the texture GetRenderSurface(...) interfaces.
class RenderSurface : public RenderSurfaceBase {
 public:
  typedef SmartPointer<RenderSurface> Ref;
  typedef WeakPointer<RenderSurface> WeakPointerType;

  RenderSurface(ServiceLocator *service_locator,
                int width,
                int height,
                Texture *texture);

  // Names of the RenderSurface Params.
  static const char *kTextureParamName;

  // Returns the texture object containing this render surface.
  Texture* texture() const {
    return texture_param_->value();
  }

  // Gets a weak pointer to us.
  WeakPointerType GetWeakPointer() const {
    return weak_pointer_manager_.GetWeakPointer();
  }

 private:
  // Texture parameter of the texture in which this render surface is contained.
  ParamTexture::Ref texture_param_;

  // Manager for weak pointers to us.
  WeakPointerType::WeakPointerManager weak_pointer_manager_;

  O3D_DECL_CLASS(RenderSurface, RenderSurfaceBase);
  DISALLOW_COPY_AND_ASSIGN(RenderSurface);
};

// A RenderDepthStencilSurface represents a depth stencil render target.
class RenderDepthStencilSurface : public RenderSurfaceBase {
 public:
  typedef SmartPointer<RenderDepthStencilSurface> Ref;
  typedef WeakPointer<RenderDepthStencilSurface> WeakPointerType;

  RenderDepthStencilSurface(ServiceLocator *service_locator,
                            int width,
                            int height);

  // Gets a weak pointer to us.
  WeakPointerType GetWeakPointer() const {
    return weak_pointer_manager_.GetWeakPointer();
  }

 private:
  // Manager for weak pointers to us.
  WeakPointerType::WeakPointerManager weak_pointer_manager_;

  O3D_DECL_CLASS(RenderDepthStencilSurface, RenderSurfaceBase);
  DISALLOW_COPY_AND_ASSIGN(RenderDepthStencilSurface);
};

class ParamRenderSurface : public TypedRefParam<RenderSurface> {
 public:
  typedef SmartPointer<ParamRenderSurface> Ref;

  ParamRenderSurface(ServiceLocator *service_locator,
                     bool dynamic,
                     bool read_only)
      : TypedRefParam<RenderSurface>(service_locator, dynamic,
                                     read_only) {}

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator *service_locator);

  O3D_DECL_CLASS(ParamRenderSurface, RefParamBase);
  DISALLOW_COPY_AND_ASSIGN(ParamRenderSurface);
};

class ParamRenderDepthStencilSurface
    : public TypedRefParam<RenderDepthStencilSurface> {
 public:
  typedef SmartPointer<ParamRenderDepthStencilSurface> Ref;

  ParamRenderDepthStencilSurface(ServiceLocator *service_locator,
                                 bool dynamic,
                                 bool read_only)
      : TypedRefParam<RenderDepthStencilSurface>(service_locator, dynamic,
                                                 read_only) {}

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator *service_locator);

  O3D_DECL_CLASS(ParamRenderDepthStencilSurface, RefParamBase);
  DISALLOW_COPY_AND_ASSIGN(ParamRenderDepthStencilSurface);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_RENDER_SURFACE_H_
