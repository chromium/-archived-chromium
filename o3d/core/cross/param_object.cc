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


// This file contains definitions for ParamObject, the base class for all
// objects that can have Params.

#include "core/cross/precompile.h"
#include "core/cross/param_object.h"
#include "base/cross/std_functional.h"
#include "core/cross/draw_context.h"
#include "core/cross/param.h"
#include "core/cross/standard_param.h"
#include "core/cross/iclass_manager.h"

namespace o3d {

O3D_DEFN_CLASS(ParamObject, NamedObject);

ParamObject::ParamObject(ServiceLocator* service_locator)
  : NamedObject(service_locator),
    class_manager_(service_locator->GetService<IClassManager>()),
    change_count_(1) {
}

ParamObject::~ParamObject() {
  // Tell each Param to Unbind so that other things will let go of it.
  {
    NamedParamRefMap::iterator end(params_.end());
    for (NamedParamRefMap::iterator iter(params_.begin());
         iter != end;
         ++iter) {
      iter->second->UnbindInput();
      iter->second->UnbindOutputs();
    }
  }

  // Delete all helpers contained in the map.
  {
    ParamRefHelperMultiMapIterator it, end = param_ref_helper_map_.end();
    for (it = param_ref_helper_map_.begin(); it != end; ++it) {
      delete it->second;
    }
  }
}

ObjectBase::Ref ParamObject::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamObject(service_locator));
}

// Factory method for Param objects.  Creates a new Param on the object.
Param* ParamObject::CreateParamByClass(const String& param_name,
                                       const ObjectBase::Class* type) {
  if (ObjectBase::ClassIsA(type, Param::GetApparentClass())) {
    Param::Ref param(down_cast<Param*>(
        class_manager_->CreateObjectByClass(type).Get()));

    // Tell the param to add the Param to its list.
    if (!param.IsNull())
      if (!AddParam(param_name, param.Get())) {
        // No need to delete the param. It's held by ref which will go zero
        // on return.
        return NULL;
      }

    return param.Get();
  }
  return NULL;
}

// Factory method for Param objects.  Creates a new Param on the object.
Param* ParamObject::CreateParamByClassName(const String& param_name,
                                           const String& class_type_name) {
  if (class_manager_->ClassNameIsAClass(class_type_name,
                                        Param::GetApparentClass())) {
    Param::Ref param(down_cast<Param*>(
        class_manager_->CreateObject(class_type_name).Get()));

    // Tell the param to add the Param to its list.
    if (!param.IsNull())
      if (!AddParam(param_name, param.Get())) {
        // No need to delete the param. It's held by ref which will go zero
        // on return.
        return NULL;
      }

    return param.Get();
  }
  return NULL;
}

// Looks in the param_map for a Param with the given name.
Param* ParamObject::GetUntypedParam(const String& name) const {
  NamedParamRefMap::const_iterator iter = params_.find(name);
  if (iter == params_.end()) {
    // Try adding the o3d namespace prefix
    String prefixed_name(O3D_STRING_CONSTANT("") + name);
    iter = params_.find(prefixed_name);
    if (iter == params_.end()) {
      return NULL;
    }
  }

  return iter->second.Get();
}

// Looks up the given Param name in the param_map, and returns it if
// it is of the correct type.  If it is of the wrong type, NULL is
// returned.  If the param does not exist, it is created with the
// given type.
Param* ParamObject::GetOrCreateParamByClass(const String& param_name,
                                            const ObjectBase::Class* type) {
  Param* param = GetUntypedParam(param_name);
  if (param) {
    if (param->GetClass() != type) {
      return NULL;
    }
  } else {
    param = CreateParamByClass(param_name, type);
  }
  return param;
}

// Copies all the params from a the given source_param_object.
// Does not replace any currently existing params with the same
// name.
void ParamObject::CopyParams(ParamObject* source_param_object) {
  const NamedParamRefMap& source_params = source_param_object->params();
  NamedParamRefMap::const_iterator end(source_params.end());
  for (NamedParamRefMap::const_iterator iter(source_params.begin());
       iter != end;
       ++iter) {
    Param* source_param = iter->second;
    Param* dest_param = GetUntypedParam(source_param->name());
    if (!dest_param) {
      // it doesn't exist so duplicate it
      dest_param = CreateParamByClass(source_param->name(),
                                      source_param->GetClass());
    }

    if (dest_param) {
      if (source_param->IsA(dest_param->GetClass())) {
        // copy the value from the source
        dest_param->CopyDataFromParam(source_param);
      }
    }
  }
}


void ParamObject::GetParamsFast(ParamVector* param_array) const {
  param_array->clear();
  param_array->reserve(params().size());
  std::transform(params().begin(),
                 params().end(),
                 std::back_inserter(*param_array),
                 base::select2nd<NamedParamRefMap::value_type>());
}

ParamVector ParamObject::GetParams() const {
  ParamVector param_array;
  GetParamsFast(&param_array);
  return param_array;
}

// Inserts the Param in the ParamObject's map of Params (indexed by name)
bool ParamObject::AddParam(const String& param_name, Param *param) {
  // Makes sure the param lasts through this function.
  Param::Ref temp(param);

  param->SetName(param_name);

  if (!OnBeforeAddParam(param)) {
    return false;
  }

  // Inserts new Param in the map so that it can be found by name fast.
  std::pair<NamedParamRefMap::iterator, bool> result = params_.insert(
      std::make_pair(param->name(), Param::Ref(param)));
  if (!result.second) {
    return false;
  }

  param->set_owner(this);

  // Also update any refs to params by this name.
  ParamRefHelperMultiMapRange range =
      param_ref_helper_map_.equal_range(param->name());
  for (ParamRefHelperMultiMapIterator iter(range.first);
       iter != range.second;
       ++iter) {
    iter->second->UpdateParamRef(param);
  }

  ++change_count_;
  OnAfterAddParam(param);

  return true;
}

bool ParamObject::RemoveParam(Param *param) {
  if (!OnBeforeRemoveParam(param)) {
    return false;
  }

  // Removes new Param in the map.
  NamedParamRefMap::iterator end(params_.end());
  for (NamedParamRefMap::iterator iter(params_.begin());
       iter != end;
       ++iter) {
    if (iter->second == param) {
      param->set_owner(NULL);
      params_.erase(iter);
      ++change_count_;
      OnAfterRemoveParam(param);
      return true;
    }
  }
  return false;
}

bool ParamObject::IsAddedParam(Param* param) {
  if (param->owner() != this)
    return false;
  for (ParamRefHelperMultiMap::const_iterator it =
           param_ref_helper_map_.begin();
       it != param_ref_helper_map_.end(); ++it) {
    ParamRefHelperBase* helper = it->second;
    if (helper->IsParam(param))
      return false;
  }
  return true;
}

namespace {

// Check if a Param is from the given ParamObject.
//
// Parameters:
//   param: param to check for. param_object: param_object to check on.
// Returns:
//   true if param is a Param on param_object Return
bool CheckParamIsFromParamObject(const Param* param,
                                 const ParamObject* param_object) {
  const NamedParamRefMap& params = param_object->params();
  NamedParamRefMap::const_iterator end(params.end());
  for (NamedParamRefMap::const_iterator iter(params.begin());
       iter != end;
       ++iter) {
    if (param == iter->second) {
      return true;
    }
  }
  return false;
}

}  // anonymous namespace

void ParamObject::GetInputsForParam(const Param* param,
                                    ParamVector* inputs) const {
  DCHECK(param);
  DCHECK(inputs);
  DCHECK(CheckParamIsFromParamObject(param, this));
  inputs->clear();
  ConcreteGetInputsForParam(param, inputs);
}

void ParamObject::GetOutputsForParam(const Param* param,
                                     ParamVector* outputs) const {
  DCHECK(param);
  DCHECK(outputs);
  DCHECK(CheckParamIsFromParamObject(param, this));
  outputs->clear();
  ConcreteGetOutputsForParam(param, outputs);
}

void ParamObject::ConcreteGetInputsForParam(const Param* param,
                                            ParamVector* inputs) const {
  // The default does nothing.
}

void ParamObject::ConcreteGetOutputsForParam(const Param* param,
                                             ParamVector* outputs) const {
  // The default does nothing.
}
}  // namespace o3d
