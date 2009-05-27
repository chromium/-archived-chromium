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


// This file contains the implementation of the Sampler class.

#include "core/cross/precompile.h"
#include "core/cross/param.h"
#include "core/cross/renderer.h"
#include "core/cross/sampler.h"
#include "core/cross/types.h"
#include "core/cross/error.h"


namespace o3d {

// Names of Params that get created for every Sampler.
const char* Sampler::kAddressUModeParamName =
    O3D_STRING_CONSTANT("addressModeU");
const char* Sampler::kAddressVModeParamName =
    O3D_STRING_CONSTANT("addressModeV");
const char* Sampler::kAddressWModeParamName =
    O3D_STRING_CONSTANT("addressModeW");
const char* Sampler::kMagFilterParamName =
    O3D_STRING_CONSTANT("magFilter");
const char* Sampler::kMinFilterParamName =
    O3D_STRING_CONSTANT("minFilter");
const char* Sampler::kMipFilterParamName =
    O3D_STRING_CONSTANT("mipFilter");
const char* Sampler::kBorderColorParamName =
    O3D_STRING_CONSTANT("borderColor");
const char* Sampler::kMaxAnisotropyParamName =
    O3D_STRING_CONSTANT("maxAnisotropy");

const char* Sampler::kTextureParamName =
    O3D_STRING_CONSTANT("texture");

O3D_DEFN_CLASS(Sampler, ParamObject);
O3D_DEFN_CLASS(ParamSampler, RefParamBase);


Sampler::Sampler(ServiceLocator* service_locator)
    : ParamObject(service_locator),
      weak_pointer_manager_(this) {
  // Create all the Params for the Sampler, register them and set their initial
  // values to their defaults.
  RegisterAndSetParam(kAddressUModeParamName,
                      WRAP,
                      &address_mode_u_param_ref_);
  RegisterAndSetParam(kAddressVModeParamName,
                      WRAP,
                      &address_mode_v_param_ref_);
  RegisterAndSetParam(kAddressWModeParamName,
                      WRAP,
                      &address_mode_w_param_ref_);
  RegisterAndSetParam(kMagFilterParamName,
                      LINEAR,
                      &mag_filter_param_ref_);
  RegisterAndSetParam(kMinFilterParamName,
                      LINEAR,
                      &min_filter_param_ref_);
  RegisterAndSetParam(kMipFilterParamName,
                      LINEAR,
                      &mip_filter_param_ref_);

  RegisterParamRef(kBorderColorParamName, &border_color_param_ref_);
  border_color_param_ref_.Get()->set_value(Float4(0, 0, 0, 0));

  RegisterAndSetParam(kMaxAnisotropyParamName, 1, &max_anisotropy_param_ref_);

  RegisterParamRef(kTextureParamName, &texture_param_ref_);
  texture_param_ref_.Get()->set_value(NULL);
}

// Creates (if it doesn't already exist) and registers a Param in the
// ParamObject and sets its value to the default_value.
template<typename T>
void Sampler::RegisterAndSetParam(const String& param_name,
                                  unsigned int default_value,
                                  T* typed_param_ref_pointer) {
  RegisterParamRef(param_name, typed_param_ref_pointer);
  typed_param_ref_pointer->Get()->set_value(default_value);
}


ObjectBase::Ref Sampler::Create(ServiceLocator* service_locator) {
  Renderer* renderer = service_locator->GetService<Renderer>();
  if (NULL == renderer) {
    O3D_ERROR(service_locator) << "No Render Device Available";
    return ObjectBase::Ref();
  }
  return ObjectBase::Ref(renderer->CreateSampler());
}


ObjectBase::Ref ParamSampler::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamSampler(service_locator, false, false));
}

}  // namespace o3d
