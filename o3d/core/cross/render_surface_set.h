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


// This file contains the declaration of the RenderSurfaceSet class.

#ifndef O3D_CORE_CROSS_RENDER_SURFACE_SET_H_
#define O3D_CORE_CROSS_RENDER_SURFACE_SET_H_

#include "core/cross/render_node.h"
#include "core/cross/render_surface.h"

namespace o3d {

// A RenderSurfaceSet node will bind depth and color RenderSurface nodes
// to the current rendering context.  All RenderNodes descending
// from the given RenderSurfaceSet node will operate on the contents of
// the bound depth and color buffers.
// Note the following usage constraints of this node:
// 1)  The surface bound to render_surface must not be of a depth format.
// 2)  The surface bound to render_depth_surface must be of a depth format.
// 3)  If both a color and depth surface is bound, then they must be of matching
//     dimensions.
// 4)  At least one of render_surface and render_depth_surface must be non-NULL.
class RenderSurfaceSet : public RenderNode {
 public:
  typedef SmartPointer<RenderSurfaceSet> Ref;

  // The names of the RenderSurfaceSet parameters.
  static const char* kRenderSurfaceParamName;
  static const char* kRenderDepthStencilSurfaceParamName;

  RenderSurface* render_surface() const {
    return render_surface_param_->value();
  }

  // Assigns a render surface to be bound to the color buffer of the active
  // renderer.
  void set_render_surface(RenderSurface* value) {
    render_surface_param_->set_value(value);
  }

  RenderDepthStencilSurface* render_depth_stencil_surface() const {
    return render_depth_stencil_surface_param_->value();
  }

  // Assigns a render surface to be bound to the depth buffer of the active
  // renderer.
  void set_render_depth_stencil_surface(RenderDepthStencilSurface* value) {
    render_depth_stencil_surface_param_->set_value(value);
  }

  // Validates that the surfaces assigned to the depth and color parameters
  // meet the constraints specified above.  Returns true if the constraints
  // are satisfied.
  bool ValidateBoundSurfaces() const;

  // Overridden from RenderNode.  Binds the RenderSurfaces attached to the
  // parameters to the active renderer.
  virtual void Render(RenderContext* render_context);

  // Overridden from RenderNode.  Restores the depth and color RenderSurfaces
  // associated with the active renderer to their state before the most recent
  // call to RenderSurfaceSet::Render.
  virtual void PostRender(RenderContext* render_context);

 private:
  explicit RenderSurfaceSet(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  RenderSurface* old_render_surface_;
  RenderDepthStencilSurface* old_depth_stencil_surface_;

  ParamRenderSurface::Ref render_surface_param_;
  ParamRenderDepthStencilSurface::Ref render_depth_stencil_surface_param_;

  O3D_DECL_CLASS(RenderSurfaceSet, RenderNode)
  DISALLOW_COPY_AND_ASSIGN(RenderSurfaceSet);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_RENDER_SURFACE_SET_H_
