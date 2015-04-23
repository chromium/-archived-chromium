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


// This file contains declarations for the DrawContext class.  This class
// contains two pre-defined Parameters: o3d.view, for the viewing
// matrix, and o3d.projection, for the projection matrix.  There
// are convenience funtions for accessing these without looking them up
// by name.  These matrices are used by StandardParamMatrix4s, to implement
// the SAS transform semantics.  In addition, any user-defined
// parameters added to the DrawContext will be made "global", ie.,
// linked to all Shapes containing parameters of the same name, by
// Node::GenerateRenderTreeGroup().

#ifndef O3D_CORE_CROSS_DRAW_CONTEXT_H_
#define O3D_CORE_CROSS_DRAW_CONTEXT_H_

#include "core/cross/param_object.h"

namespace o3d {

class DrawContext : public ParamObject {
 public:
  typedef SmartPointer<DrawContext> Ref;
  typedef WeakPointer<DrawContext> WeakPointerType;

  static const char* kViewParamName;
  static const char* kProjectionParamName;

  // Gets the view matrix.
  const Matrix4 view() const {
    return view_param_->value();
  }

  // Sets the view matrix.
  void set_view(const Matrix4& value) {
    view_param_->set_value(value);
  }

  // Gets the projection matrix.
  const Matrix4 projection() const {
    return projection_param_->value();
  }

  // Sets the projection matrix.
  void set_projection(const Matrix4& value) {
    projection_param_->set_value(value);
  }

  // Gets a weak pointer to us.
  WeakPointerType GetWeakPointer() const {
    return weak_pointer_manager_.GetWeakPointer();
  }

 private:
  explicit DrawContext(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // Predefined view matrix parameter.
  ParamMatrix4::Ref view_param_;

  // Predefined projection matrix parameter.
  ParamMatrix4::Ref projection_param_;

  // Manager for weak pointers to us.
  WeakPointerType::WeakPointerManager weak_pointer_manager_;

  O3D_DECL_CLASS(DrawContext, ParamObject)
  DISALLOW_COPY_AND_ASSIGN(DrawContext);
};

class ParamDrawContext : public TypedRefParam<DrawContext> {
 public:
  typedef SmartPointer<ParamDrawContext> Ref;

  ParamDrawContext(ServiceLocator* service_locator,
                   bool dynamic,
                   bool read_only)
      : TypedRefParam<DrawContext>(service_locator, dynamic, read_only) {}

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(ParamDrawContext, RefParamBase)
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_DRAW_CONTEXT_H_
