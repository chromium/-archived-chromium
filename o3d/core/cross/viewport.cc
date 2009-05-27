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


// This file contains the Viewport render node implementation

#include "core/cross/precompile.h"
#include "core/cross/viewport.h"
#include "core/cross/renderer.h"

namespace o3d {

O3D_DEFN_CLASS(Viewport, RenderNode);

const char* Viewport::kViewportParamName =
    O3D_STRING_CONSTANT("viewport");
const char* Viewport::kDepthRangeParamName =
    O3D_STRING_CONSTANT("depthRange");

Viewport::Viewport(ServiceLocator* service_locator)
    : RenderNode(service_locator) {
  RegisterParamRef(kViewportParamName, &viewport_param_);
  RegisterParamRef(kDepthRangeParamName, &depth_range_param_);

  set_viewport(Float4(0.0, 0.0f, 1.0f, 1.0f));
  set_depth_range(Float2(0.0, 1.0f));
}

ObjectBase::Ref Viewport::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new Viewport(service_locator));
}

void Viewport::Render(RenderContext* render_context) {
  render_context->renderer()->GetViewport(&old_viewport_, &old_depth_range_);
  render_context->renderer()->SetViewport(viewport(), depth_range());
}

void Viewport::PostRender(RenderContext* render_context) {
  render_context->renderer()->SetViewport(old_viewport_, old_depth_range_);
}

}  // namespace o3d
