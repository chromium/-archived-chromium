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
#include "core/cross/render_surface.h"

namespace o3d {

O3D_DEFN_CLASS(RenderSurfaceBase, ParamObject);
O3D_DEFN_CLASS(RenderSurface, RenderSurfaceBase);
O3D_DEFN_CLASS(RenderDepthStencilSurface, RenderSurfaceBase);
O3D_DEFN_CLASS(ParamRenderSurface, RefParamBase);
O3D_DEFN_CLASS(ParamRenderDepthStencilSurface, RefParamBase);

const char* RenderSurfaceBase::kWidthParamName =
    O3D_STRING_CONSTANT("width");
const char* RenderSurfaceBase::kHeightParamName =
    O3D_STRING_CONSTANT("height");
const char* RenderSurface::kTextureParamName =
    O3D_STRING_CONSTANT("texture");

RenderSurfaceBase::RenderSurfaceBase(ServiceLocator* service_locator,
                                     int width,
                                     int height)
    : ParamObject(service_locator) {
  RegisterReadOnlyParamRef(kWidthParamName, &width_param_);
  RegisterReadOnlyParamRef(kHeightParamName, &height_param_);

  width_param_->set_read_only_value(width);
  height_param_->set_read_only_value(height);
}

RenderSurface::RenderSurface(ServiceLocator* service_locator,
                             int width,
                             int height,
                             Texture* texture)
    : RenderSurfaceBase(service_locator, width, height),
      weak_pointer_manager_(this) {
  RegisterReadOnlyParamRef(kTextureParamName, &texture_param_);

  texture_param_->set_read_only_value(texture);
}

RenderDepthStencilSurface::RenderDepthStencilSurface(
    ServiceLocator* service_locator,
    int width,
    int height)
    : RenderSurfaceBase(service_locator, width, height),
      weak_pointer_manager_(this) {
}

ObjectBase::Ref ParamRenderSurface::Create(ServiceLocator* client) {
  return ObjectBase::Ref(new ParamRenderSurface(client, false, false));
}

ObjectBase::Ref ParamRenderDepthStencilSurface::Create(ServiceLocator* client) {
  return ObjectBase::Ref(new ParamRenderDepthStencilSurface(client,
                                                            false,
                                                            false));
}

}  // namespace o3d
