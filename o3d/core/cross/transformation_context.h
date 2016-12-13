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


#ifndef O3D_CORE_CROSS_TRANSFORMATION_CONTEXT_H_
#define O3D_CORE_CROSS_TRANSFORMATION_CONTEXT_H_

#include "core/cross/service_implementation.h"
#include "core/cross/math_types.h"

namespace o3d {

// Holds the current transformation state, including the world, view and
// projection matrices. TODO: it would be nice if this wasn't global state.
class TransformationContext {
 public:
  static const InterfaceId kInterfaceId;

  explicit TransformationContext(ServiceLocator* service_locator)
      : service_(service_locator, this) {}

  // Retrieves the current world matrix.
  const Matrix4& world() const {
    return world_;
  }

  // Retrieves the current view matrix.
  const Matrix4& view() const {
    return view_;
  }

  // Retrieves the current projection matrix.
  const Matrix4& projection() const {
    return projection_;
  }

  // Retrieves the current view_projection matrix.
  const Matrix4& view_projection() const {
    return view_projection_;
  }

  // Retrieves the current world_view_projection matrix.
  const Matrix4& world_view_projection() const {
    return world_view_projection_;
  }

  // Sets the current world matrix.
  void set_world(const Matrix4& world) {
    world_ = world;
  }

  // Sets the current view matrix.
  void set_view(const Matrix4& view) {
    view_ = view;
  }

  // Sets the current projection matrix.
  void set_projection(const Matrix4& projection) {
    projection_ = projection;
  }

  // Sets the current view projection matrix.
  void set_view_projection(const Matrix4& view_projection) {
    view_projection_ = view_projection;
  }

  // Sets the current world view projection matrix.
  void set_world_view_projection(const Matrix4& world_view_projection) {
    world_view_projection_ = world_view_projection;
  }

 private:
  ServiceImplementation<TransformationContext> service_;

  // The following five matrices represent the transformation hierarchy during
  // drawing of the scene.  These are set in the draw code prior to rendering,
  // so that StandardParamMatrix4s evaluate correctly.

  // The current world matrix.
  Matrix4 world_;
  // The current view matrix.
  Matrix4 view_;
  // The current projection matrix.
  Matrix4 projection_;
  // The current view_projection matrix.
  Matrix4 view_projection_;
  // The current world_view_projection matrix.
  Matrix4 world_view_projection_;
};
}  // namespace o3d

#endif  // O3D_CORE_CROSS_TRANSFORMATION_CONTEXT_H_
