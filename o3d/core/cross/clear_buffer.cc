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


// This file contains the ClearBuffer render node implementation

#include "core/cross/precompile.h"
#include "core/cross/clear_buffer.h"
#include "core/cross/renderer.h"

namespace o3d {

O3D_DEFN_CLASS(ClearBuffer, RenderNode)

const char* ClearBuffer::kClearColorParamName =
    O3D_STRING_CONSTANT("clearColor");
const char* ClearBuffer::kClearColorFlagParamName =
    O3D_STRING_CONSTANT("clearColorFlag");
const char* ClearBuffer::kClearDepthParamName =
    O3D_STRING_CONSTANT("clearDepth");
const char* ClearBuffer::kClearDepthFlagParamName =
    O3D_STRING_CONSTANT("clearDepthFlag");
const char* ClearBuffer::kClearStencilParamName =
    O3D_STRING_CONSTANT("clearStencil");
const char* ClearBuffer::kClearStencilFlagParamName =
    O3D_STRING_CONSTANT("clearStencilFlag");

ClearBuffer::ClearBuffer(ServiceLocator* service_locator)
    : RenderNode(service_locator) {
  RegisterParamRef(kClearColorParamName, &color_param_ref_);
  RegisterParamRef(kClearColorFlagParamName, &color_flag_param_ref_);
  RegisterParamRef(kClearDepthParamName, &depth_param_ref_);
  RegisterParamRef(kClearDepthFlagParamName, &depth_flag_param_ref_);
  RegisterParamRef(kClearStencilParamName, &stencil_param_ref_);
  RegisterParamRef(kClearStencilFlagParamName, &stencil_flag_param_ref_);

  set_clear_color(Float4(0.0, 0.0f, 0.0f, 1.0f));
  set_clear_color_flag(true);
  set_clear_depth(1.0f);
  set_clear_depth_flag(true);
  set_clear_stencil(0);
  set_clear_stencil_flag(true);
}

ClearBuffer::~ClearBuffer() {
}

ObjectBase::Ref ClearBuffer::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ClearBuffer(service_locator));
}

void ClearBuffer::Render(RenderContext* render_context) {
  render_context->renderer()->Clear(
      clear_color(),
      clear_color_flag(),
      clear_depth(),
      clear_depth_flag(),
      clear_stencil(),
      clear_stencil_flag());
}

}  // namespace o3d
