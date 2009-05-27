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


// This file contains definitions for the DrawContext class.  This class
// contains two pre-defined Parameters: o3d.view, for the viewing
// matrix, and o3d.projection, for the projection matrix.  There
// are convenience funtions for accessing these without looking them up
// by name.  These matrices are used by StandardParamMatrix4s, to implement
// the SAS transform semantics.  In addition, any user-defined
// parameters added to the DrawContext will be made "global", ie.,
// linked to all Shapes containing parameters of the same name, by
// Node::GenerateRenderTreeGroup().

#include "core/cross/precompile.h"
#include "core/cross/draw_context.h"

namespace o3d {

const char* DrawContext::kViewParamName =
    O3D_STRING_CONSTANT("view");
const char* DrawContext::kProjectionParamName =
    O3D_STRING_CONSTANT("projection");

O3D_DEFN_CLASS(DrawContext, ParamObject);
O3D_DEFN_CLASS(ParamDrawContext, RefParamBase);

DrawContext::DrawContext(ServiceLocator* service_locator)
    : ParamObject(service_locator),
      weak_pointer_manager_(this) {
  RegisterParamRef(kViewParamName, &view_param_);
  RegisterParamRef(kProjectionParamName, &projection_param_);
}

ObjectBase::Ref DrawContext::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new DrawContext(service_locator));
}

ObjectBase::Ref ParamDrawContext::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamDrawContext(service_locator, false, false));
}
}  // namespace o3d
