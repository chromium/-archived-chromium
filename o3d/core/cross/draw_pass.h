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


// This file contains the declaration of DrawPass.

#ifndef O3D_CORE_CROSS_DRAW_PASS_H_
#define O3D_CORE_CROSS_DRAW_PASS_H_

#include "core/cross/render_node.h"
#include "core/cross/draw_list.h"

namespace o3d {

class TransformationContext;
class DrawList;
class RenderContext;

// A DrawPass is a RenderNode that renders a DrawList with a specific
// DrawContext.
class DrawPass : public RenderNode {
 public:
  typedef SmartPointer<DrawPass> Ref;

  static const char* kDrawListParamName;
  static const char* kSortMethodParamName;

  // Gets the draw list.
  DrawList* draw_list() const {
    return draw_list_param_->value();
  }

  // Sets the draw list.
  void set_draw_list(DrawList* value) {
    draw_list_param_->set_value(value);
  }

  // Gets the sort method.
  DrawList::SortMethod sort_method() const {
    return static_cast<DrawList::SortMethod>(sort_method_param_->value());
  }

  // Sets the sort method.
  void set_sort_method(DrawList::SortMethod value) {
    sort_method_param_->set_value(value);
  }

  // Renders this DrawPass.
  void Render(RenderContext* render_context);

 private:
  explicit DrawPass(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  TransformationContext* transformation_context_;

  // Predefined draw context parameter.
  ParamDrawContext::Ref draw_context_param_;
  ParamDrawList::Ref draw_list_param_;  // DrawList we will render.
  ParamInteger::Ref sort_method_param_;  // The order we will sort the DrawList.

  O3D_DECL_CLASS(DrawPass, RenderNode);
  DISALLOW_COPY_AND_ASSIGN(DrawPass);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_DRAW_PASS_H_




