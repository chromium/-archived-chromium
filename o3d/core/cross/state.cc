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


// This file contains the definition of the State class

#include "core/cross/precompile.h"
#include "core/cross/state.h"
#include "core/cross/types.h"
#include "core/cross/renderer.h"
#include "core/cross/error.h"

namespace o3d {

O3D_DEFN_CLASS(State, NamedObject);
O3D_DEFN_CLASS(ParamState, RefParamBase);

State::State(ServiceLocator* service_locator, Renderer *renderer)
    : ParamObject(service_locator) ,
      renderer_(renderer),
      weak_pointer_manager_(this) {
}

Param* State::GetUntypedStateParam(const String& state_name) {
  Param* param = GetUntypedParam(state_name);
  if (!param) {
    String name(state_name);
    const ObjectBase::Class* param_type =
        renderer_->GetStateParamType(name);
    if (!param_type) {
      // Try adding the o3d namespace prefix
      name = String(O3D_STRING_CONSTANT("") + state_name);
      param_type = renderer_->GetStateParamType(name);
    }
    if (param_type) {
      param = CreateParamByClass(name, param_type);
    }
  }
  return param;
}

// Before a Param is added, verify that if its name matches a state name
// that its type also matches.
bool State::OnBeforeAddParam(Param* param) {
  const ObjectBase::Class* param_type =
      renderer_->GetStateParamType(param->name());
  if (param_type) {
    return param->IsA(param_type);
  }
  return true;
}

const char* State::kAlphaTestEnableParamName =
    O3D_STRING_CONSTANT("AlphaTestEnable");
const char* State::kAlphaReferenceParamName =
    O3D_STRING_CONSTANT("AlphaReference");
const char* State::kAlphaComparisonFunctionParamName =
    O3D_STRING_CONSTANT("AlphaComparisonFunction");
const char* State::kCullModeParamName =
    O3D_STRING_CONSTANT("CullMode");
const char* State::kDitherEnableParamName =
    O3D_STRING_CONSTANT("DitherEnable");
const char* State::kLineSmoothEnableParamName =
    O3D_STRING_CONSTANT("LineSmoothEnable");
const char* State::kPointSpriteEnableParamName =
    O3D_STRING_CONSTANT("PointSpriteEnable");
const char* State::kPointSizeParamName =
    O3D_STRING_CONSTANT("PointSize");
const char* State::kPolygonOffset1ParamName =
    O3D_STRING_CONSTANT("PolygonOffset1");
const char* State::kPolygonOffset2ParamName =
    O3D_STRING_CONSTANT("PolygonOffset2");
const char* State::kFillModeParamName =
    O3D_STRING_CONSTANT("FillMode");
const char* State::kZEnableParamName =
    O3D_STRING_CONSTANT("ZEnable");
const char* State::kZWriteEnableParamName =
    O3D_STRING_CONSTANT("ZWriteEnable");
const char* State::kZComparisonFunctionParamName =
    O3D_STRING_CONSTANT("ZComparisonFunction");
const char* State::kAlphaBlendEnableParamName =
    O3D_STRING_CONSTANT("AlphaBlendEnable");
const char* State::kSourceBlendFunctionParamName =
    O3D_STRING_CONSTANT("SourceBlendFunction");
const char* State::kDestinationBlendFunctionParamName =
    O3D_STRING_CONSTANT("DestinationBlendFunction");
const char* State::kStencilEnableParamName =
    O3D_STRING_CONSTANT("StencilEnable");
const char* State::kStencilFailOperationParamName =
    O3D_STRING_CONSTANT("StencilFailOperation");
const char* State::kStencilZFailOperationParamName =
    O3D_STRING_CONSTANT("StencilZFailOperation");
const char* State::kStencilPassOperationParamName =
    O3D_STRING_CONSTANT("StencilPassOperation");
const char* State::kStencilComparisonFunctionParamName =
    O3D_STRING_CONSTANT("StencilComparisonFunction");
const char* State::kStencilReferenceParamName =
    O3D_STRING_CONSTANT("StencilReference");
const char* State::kStencilMaskParamName =
    O3D_STRING_CONSTANT("StencilMask");
const char* State::kStencilWriteMaskParamName =
    O3D_STRING_CONSTANT("StencilWriteMask");
const char* State::kColorWriteEnableParamName =
    O3D_STRING_CONSTANT("ColorWriteEnable");
const char* State::kBlendEquationParamName =
    O3D_STRING_CONSTANT("BlendEquation");
const char* State::kTwoSidedStencilEnableParamName =
    O3D_STRING_CONSTANT("TwoSidedStencilEnable");
const char* State::kCCWStencilFailOperationParamName =
    O3D_STRING_CONSTANT("CCWStencilFailOperation");
const char* State::kCCWStencilZFailOperationParamName =
    O3D_STRING_CONSTANT("CCWStencilZFailOperation");
const char* State::kCCWStencilPassOperationParamName =
    O3D_STRING_CONSTANT("CCWStencilPassOperation");
const char* State::kCCWStencilComparisonFunctionParamName =
    O3D_STRING_CONSTANT("CCWStencilComparisonFunction");
const char* State::kSeparateAlphaBlendEnableParamName =
    O3D_STRING_CONSTANT("SeparateAlphaBlendEnable");
const char* State::kSourceBlendAlphaFunctionParamName =
    O3D_STRING_CONSTANT("SourceBlendAlphaFunction");
const char* State::kDestinationBlendAlphaFunctionParamName =
    O3D_STRING_CONSTANT("DestinationBlendAlphaFunction");
const char* State::kBlendAlphaEquationParamName =
    O3D_STRING_CONSTANT("BlendAlphaEquation");


ObjectBase::Ref State::Create(ServiceLocator* service_locator) {
  Renderer* renderer = service_locator->GetService<Renderer>();
  if (NULL == renderer) {
    O3D_ERROR(service_locator) << "No Render Device Available";
    return ObjectBase::Ref();
  }
  return ObjectBase::Ref(new State(service_locator, renderer));
}

ObjectBase::Ref ParamState::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamState(service_locator, false, false));
}

}  // namespace o3d
