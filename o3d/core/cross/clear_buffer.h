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


// This file contains the ClearBuffer render node declaration.

#ifndef O3D_CORE_CROSS_CLEAR_BUFFER_H_
#define O3D_CORE_CROSS_CLEAR_BUFFER_H_

#include "core/cross/render_node.h"

namespace o3d {

// A ClearBuffer is a render node that clears the color buffer, zbuffer and/or
// stencil buffer of the current render target.
class ClearBuffer : public RenderNode {
 public:
  typedef SmartPointer<ClearBuffer> Ref;

  virtual ~ClearBuffer();

  // Gets the clear color.
  const Float4 clear_color() const {
    return color_param_ref_->value();
  }

  // Sets the clear color.
  void set_clear_color(const Float4& value) {
    color_param_ref_->set_value(value);
  }

  // Gets the clear color flag.
  bool clear_color_flag() const {
    return color_flag_param_ref_->value();
  }

  // Sets the clear color flag.
  void set_clear_color_flag(bool value) {
    color_flag_param_ref_->set_value(value);
  }

  // Gets the depth.
  float clear_depth() const {
    return depth_param_ref_->value();
  }

  // Sets the depth.
  void set_clear_depth(float value) {
    depth_param_ref_->set_value(value);
  }

  // Gets the clear depth flag.
  bool clear_depth_flag() const {
    return depth_flag_param_ref_->value();
  }

  // Sets the clear depth flag.
  void set_clear_depth_flag(bool value) {
    depth_flag_param_ref_->set_value(value);
  }

  // Gets the stencil clear value..
  int clear_stencil() const {
    return stencil_param_ref_->value();
  }

  // Sets the stencil clear value..
  void set_clear_stencil(int value) {
    stencil_param_ref_->set_value(value);
  }

  // Gets the clear stencil flag.
  bool clear_stencil_flag() const {
    return stencil_flag_param_ref_->value();
  }

  // Sets the clear stencil flag.
  void set_clear_stencil_flag(bool value) {
    stencil_flag_param_ref_->set_value(value);
  }

  // Names of ClearBuffer Params.
  static const char* kClearColorParamName;
  static const char* kClearColorFlagParamName;
  static const char* kClearDepthParamName;
  static const char* kClearDepthFlagParamName;
  static const char* kClearStencilParamName;
  static const char* kClearStencilFlagParamName;

  // Overridden from RenderNode. Renders with the specifed Effect, State and
  // params
  virtual void Render(RenderContext* render_context);

 private:
  explicit ClearBuffer(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  ParamFloat4::Ref color_param_ref_;  // Color to clear buffer.
  ParamBoolean::Ref color_flag_param_ref_;  // True = clear color.
  ParamFloat::Ref depth_param_ref_;  // Value to set depth buffer.
  ParamBoolean::Ref depth_flag_param_ref_;  // True = clear depth buffer.
  ParamInteger::Ref stencil_param_ref_;  // Value to set stencil.
  ParamBoolean::Ref stencil_flag_param_ref_;  // True = clear stencil buffer.

  O3D_DECL_CLASS(ClearBuffer, RenderNode);
  DISALLOW_COPY_AND_ASSIGN(ClearBuffer);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_CLEAR_BUFFER_H_




