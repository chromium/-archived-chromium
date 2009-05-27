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
#include "core/cross/render_surface_set.h"

#include "core/cross/error.h"
#include "core/cross/renderer.h"

namespace o3d {

O3D_DEFN_CLASS(RenderSurfaceSet, RenderNode);

const char* RenderSurfaceSet::kRenderSurfaceParamName =
    O3D_STRING_CONSTANT("renderSurface");
const char* RenderSurfaceSet::kRenderDepthStencilSurfaceParamName =
    O3D_STRING_CONSTANT("renderDepthStencilSurface");

RenderSurfaceSet::RenderSurfaceSet(ServiceLocator* service_locator)
    : RenderNode(service_locator),
      old_render_surface_(NULL),
      old_depth_stencil_surface_(NULL) {
  RegisterParamRef(kRenderSurfaceParamName, &render_surface_param_);
  RegisterParamRef(kRenderDepthStencilSurfaceParamName,
                   &render_depth_stencil_surface_param_);
}

ObjectBase::Ref RenderSurfaceSet::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new RenderSurfaceSet(service_locator));
}

bool RenderSurfaceSet::ValidateBoundSurfaces() const {
  RenderSurface *surface = render_surface();
  RenderDepthStencilSurface *depth_stencil_surface =
      render_depth_stencil_surface();

  // Validate that at least one surface is assigned.
  if (!(surface || depth_stencil_surface)) {
    O3D_ERROR(service_locator())
        << "RenderSurfaceSet '" << name()
        << "' has neither a surface nor a depth stencil surface. "
        << "It must have at least one.";
    return false;
  }

  // If both surfaces are bound, validate that they share the same dimensions.
  if (surface && depth_stencil_surface) {
    if (surface->width() != depth_stencil_surface->width() ||
        surface->height() != depth_stencil_surface->height()) {
      O3D_ERROR(service_locator())
          << "RenderSurfaceSet '" << name()
          << "' has a surface and a depth stencil surface that do not match"
          << " dimensions.";
      return false;
    }
  }

  return true;
}

void RenderSurfaceSet::Render(RenderContext* render_context) {
  if (!ValidateBoundSurfaces()) {
    return;
  }
  render_context->renderer()->GetRenderSurfaces(
      &old_render_surface_,
      &old_depth_stencil_surface_);
  render_context->renderer()->SetRenderSurfaces(
      render_surface(),
      render_depth_stencil_surface());
}

void RenderSurfaceSet::PostRender(RenderContext* render_context) {
  if (!ValidateBoundSurfaces()) {
    return;
  }
  render_context->renderer()->SetRenderSurfaces(old_render_surface_,
                                                old_depth_stencil_surface_);
}

}  // namespace o3d
