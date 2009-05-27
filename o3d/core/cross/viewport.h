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


// This file contains the Viewport render node declaration.

#ifndef O3D_CORE_CROSS_VIEWPORT_H_
#define O3D_CORE_CROSS_VIEWPORT_H_

#include "core/cross/render_node.h"

namespace o3d {

// A Viewport is a render node that sets the render viewport and depth range.
// It uses a Float4 in the format (left, top, width, height) where left, top,
// width and height are in a 0.0 to 1.0 range that represent positions and
// dimensions relative to the size of the client's rendering area. The depth
// range is represented by a Float2 in the format (min Z, max Z)
//
// Note: The viewport values must describe a rectangle that is 100% inside the
// client area. In other words, (0.5, 0.0, 1.0, 1.0) would describe an area that
// is 1/2 off right side of the screen. That is an invalid value and will be
// clipped to (0.5, 0.0, 0.5, 1.0).
class Viewport : public RenderNode {
 public:
  typedef SmartPointer<Viewport> Ref;

  // Gets the viewport.
  const Float4 viewport() const {
    return viewport_param_->value();
  }

  // Sets the viewport.
  void set_viewport(const Float4& value) {
    viewport_param_->set_value(value);
  }

  // Gets the depth range.
  const Float2 depth_range() const {
    return depth_range_param_->value();
  }

  // Sets the depth range.
  void set_depth_range(const Float2& value) {
    depth_range_param_->set_value(value);
  }

  // Names of Viewport Params.
  static const char* kViewportParamName;
  static const char* kDepthRangeParamName;

  // Overridden from RenderNode. Sets the viewport.
  virtual void Render(RenderContext* render_context);

  // Overridden from RenderNode. Restores the viewport.
  virtual void PostRender(RenderContext* render_context);

 private:
  explicit Viewport(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  ParamFloat4::Ref viewport_param_;  // viewport (left, top, width, height)
  ParamFloat2::Ref depth_range_param_;  // minZ (def. 0.0), maxZ (def. 1.0)

  Float4 old_viewport_;
  Float2 old_depth_range_;

  O3D_DECL_CLASS(Viewport, RenderNode);
  DISALLOW_COPY_AND_ASSIGN(Viewport);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_VIEWPORT_H_




