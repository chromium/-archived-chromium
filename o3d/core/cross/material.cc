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


// This file contains the definition of Material.

#include "core/cross/precompile.h"
#include "core/cross/material.h"

namespace o3d {

O3D_DEFN_CLASS(Material, ParamObject);
O3D_DEFN_CLASS(ParamMaterial, RefParamBase);

const char* Material::kStateParamName =
    O3D_STRING_CONSTANT("state");
const char* Material::kEffectParamName =
    O3D_STRING_CONSTANT("effect");
const char* Material::kDrawListParamName =
    O3D_STRING_CONSTANT("drawList");

Material::Material(ServiceLocator* service_locator)
    : ParamObject(service_locator),
      weak_pointer_manager_(this) {
  RegisterParamRef(kStateParamName, &state_param_ref_);
  RegisterParamRef(kEffectParamName, &effect_param_ref_);
  RegisterParamRef(kDrawListParamName, &draw_list_param_);
}

ObjectBase::Ref Material::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new Material(service_locator));
}

ObjectBase::Ref ParamMaterial::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamMaterial(service_locator, false, false));
}

}  // namespace o3d

