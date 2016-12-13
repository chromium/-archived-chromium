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


// This file contains the implementation of class ParamArray.

#include "core/cross/precompile.h"
#include "core/cross/param_array.h"
#include "core/cross/error.h"
#include "core/cross/iclass_manager.h"

namespace o3d {

O3D_DEFN_CLASS(ParamArray, NamedObject);
O3D_DEFN_CLASS(ParamParamArray, RefParamBase);

ParamArray::ParamArray(ServiceLocator* service_locator)
    : NamedObject(service_locator),
      class_manager_(service_locator->GetService<IClassManager>()),
      weak_pointer_manager_(this) {
}

ParamArray::~ParamArray() {
  // Tell each Param to Unbind so that other things will let go of it.
  {
    ParamRefVector::iterator end(params_.end());
    for (ParamRefVector::iterator iter(params_.begin());
         iter != end;
         ++iter) {
      Param* param = (*iter).Get();
      if (param) {
        param->UnbindInput();
        param->UnbindOutputs();
      }
    }
  }
}

Param* ParamArray::CreateParamByClass(unsigned index,
                                      const ObjectBase::Class* type) {
  if (!ObjectBase::ClassIsA(type, Param::GetApparentClass())) {
    O3D_ERROR(service_locator()) << type->name()
                                 << " is not a type of Param";
  } else {
    // Create and insert Params until creating this new Param will not be
    // out of range.
    if (params_.size() <= index) {
      params_.reserve(index + 1);
      while (params_.size() <= index) {
        Param::Ref param(down_cast<Param*>(
            class_manager_->CreateObjectByClass(type).Get()));
        if (param.IsNull()) {
          O3D_ERROR(service_locator())
              << "could not create param at index " << params_.size();
        }
        params_.push_back(param);
      }
    }

    // Create our new Param.
    {
      Param::Ref param(down_cast<Param*>(
          class_manager_->CreateObjectByClass(type).Get()));

      if (!param.IsNull()) {
        params_[index]->UnbindInput();
        params_[index]->UnbindOutputs();
        params_[index] = param;
        return param.Get();
      }
    }
  }
  return NULL;
}

Param* ParamArray::CreateParamByClassName(unsigned index,
                                          const String& class_type_name) {
  const ObjectBase::Class* class_type = class_manager_->GetClassByClassName(
      class_type_name);
  if (!class_type) {
    O3D_ERROR(service_locator()) << class_type_name
                                 << " is not a type of Param";
    return NULL;
  }

  return CreateParamByClass(index, class_type);
}

void ParamArray::ResizeByClass(unsigned num_params,
                               const ObjectBase::Class* type) {
  if (num_params > params_.size()) {
    CreateParamByClass(num_params - 1, type);
  } else if (num_params < params_.size()) {
    RemoveParams(num_params, params_.size() - num_params);
  }
}

void ParamArray::ResizeByClassName(unsigned num_params,
                                   const String& class_type_name) {
  const ObjectBase::Class* class_type = class_manager_->GetClassByClassName(
      class_type_name);
  if (!class_type) {
    O3D_ERROR(service_locator()) << class_type_name
                                 << " is not a type of Param";
    return;
  }

  ResizeByClass(num_params, class_type);
}

void ParamArray::RemoveParams(unsigned start_index, unsigned num_to_remove) {
  if (start_index < params_.size() && num_to_remove > 0) {
    unsigned end_index = start_index + num_to_remove;
    if (end_index > params_.size()) {
      end_index = params_.size();
    }
    for (unsigned ii = start_index; ii < end_index; ++ii) {
      params_[ii]->UnbindInput();
      params_[ii]->UnbindOutputs();
    }
    params_.erase(params_.begin() + start_index, params_.begin() + end_index);
  }
}

bool ParamArray::ParamInArray(const Param* param) const {
  for (unsigned ii = 0; ii < params_.size(); ++ii) {
    if (params_[ii] == param) {
      return true;
    }
  }
  return false;
}

ObjectBase::Ref ParamArray::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamArray(service_locator));
}

ObjectBase::Ref ParamParamArray::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamParamArray(service_locator, false, false));
}

}  // namespace o3d
