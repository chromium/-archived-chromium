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


#include "core/cross/precompile.h"
#include "core/cross/semantic_manager.h"
#include "core/cross/standard_param.h"

namespace o3d {

const InterfaceId SemanticManager::kInterfaceId =
    InterfaceTraits<SemanticManager>::kInterfaceId;

SemanticManager::SemanticManager(ServiceLocator* service_locator)
    : service_(service_locator, this) {
  // Create an object to hold one of each type of sas param.
  sas_param_object_ = ParamObject::Ref(new ParamObject(service_locator));
  sas_param_object_->set_name(O3D_STRING_CONSTANT("sasParamObject"));

  // Initializes the static map between SAS parameter names and the
  // corresponding StandardParamMatrix4 type and adds one of each SAS
  // param type of the sas_param_object_ by class name.
#undef O3D_STANDARD_ANNOTATION_ENTRY
#define O3D_STANDARD_ANNOTATION_ENTRY(enum_name, class_name)              \
  sas_map_[#class_name] = class_name ## ParamMatrix4::GetApparentClass(); \
  sas_param_object_->CreateParamByClass(                                  \
      class_name ## ParamMatrix4::GetApparentClass()->name(),             \
      class_name ## ParamMatrix4::GetApparentClass());

  // Create the standard annotations (this uses the above ENTRY macro).
  O3D_STANDARD_ANNOTATIONS;
}

SemanticManager::~SemanticManager() {
  sas_param_object_.Reset();
}

// Looks up an SAS transform semantic by name, and returns the Semantic enum.
const ObjectBase::Class* SemanticManager::LookupSemantic(
    const String& semantic) const {
  SasMap::const_iterator iter = sas_map_.find(semantic);
  return (iter != sas_map_.end()) ? iter->second : NULL;
}
}  // namespace o3d
