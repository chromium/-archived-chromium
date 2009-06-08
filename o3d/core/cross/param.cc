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


// This file contains definitions for the Param class and the ones derived from
// it for each type of data (ParamFloat, ParamFloat2, ParamFloat3,
// ParamMatrix, ParamBoolean, ParamString, ParamInteger, ParamTexture).

#include "core/cross/precompile.h"
#include "core/cross/param.h"
#include "core/cross/param_object.h"
#include "core/cross/error.h"

namespace o3d {

O3D_DEFN_CLASS(Param, NamedObjectBase);
O3D_DEFN_CLASS(RefParamBase, Param);
O3D_DEFN_CLASS(ParamFloat, Param);
O3D_DEFN_CLASS(ParamFloat2, Param);
O3D_DEFN_CLASS(ParamFloat3, Param);
O3D_DEFN_CLASS(ParamFloat4, Param);
O3D_DEFN_CLASS(ParamInteger, Param);
O3D_DEFN_CLASS(ParamBoolean, Param);
O3D_DEFN_CLASS(ParamString, Param);
O3D_DEFN_CLASS(ParamMatrix4, Param);

// Base constructor that sets the Param type, resets input connection
// and handle.
Param::Param(ServiceLocator* service_locator, bool dynamic, bool read_only)
    : NamedObjectBase(service_locator),
      evaluation_counter_(service_locator->GetService<EvaluationCounter>()),
      input_connection_(NULL),
      not_cachable_count_(0),
      dynamic_(dynamic),
      read_only_(read_only),
      handle_(NULL),
      owner_(NULL),
      last_evaluation_count_(evaluation_counter_->evaluation_count() - 1),
      update_input_(true) {
}

Param::~Param() {
  // There can't be any output connections since they'd have a reference to us.
  DCHECK(output_connections_.empty());
  UnbindInput();
  DCHECK(input_connection_ == NULL);
}

const String& Param::name() const {
  return name_;
}

void Param::SetName(const String& name) {
  DCHECK(!name.empty());
  DCHECK(name_.empty());
  name_ = name;
}

// Updates the contents of data_ by recursively traversing the input connections
// and evaluating them.
void Param::ComputeValue() {
  if (input_connection_) {
    if (update_input_) {
      input_connection_->UpdateValue();
    }
    CopyDataFromParam(input_connection_);
  }
}

void Param::MarkAsReadOnly() {
  DCHECK(input_connection_ == NULL);
  read_only_ = true;
}

// Adds an output connection to the Param by just inserting
// it in the array.
void Param::AddOutputConnection(Param* param) {
  output_connections_.push_back(param);
}

bool Param::UnbindOutput(Param* param) {
  ParamVector::iterator pos = std::find(output_connections_.begin(),
                                        output_connections_.end(),
                                        param);
  if (pos != output_connections_.end()) {
    output_connections_.erase(pos);
    param->ResetInputConnection();
    return true;
  }
  return false;
}

void Param::ResetInputConnection() {
  // Keep a temporary reference to the input_connection
  // so it does not get deleted when we input_connection_.Reset()
  // before we get a chance to call
  // DecrementNotCachableCountOnParamChainForInput.
  Param::Ref temp(input_connection_);
  input_connection_.Reset();
  // This is called after input_connection_.Reset() for symmetry
  // with the way DecrementNotCachableCountOnParamChainForInput
  // is called.
  DecrementNotCachableCountOnParamChainForInput(temp);
  OnAfterUnbindInput(temp.Get());
}

void Param::set_owner(ParamObject* owner) {
  DCHECK((owner_ == NULL && owner != NULL) ||
         (owner_ != NULL && owner == NULL));
  owner_ = owner;
}

namespace {

// Check if a param is in the given param array
// Parameters:
//   param: param to search for.
//   params: ParamVector to search.
// Returns:
//   true if param is in params.
bool ParamInParams(Param* param, const ParamVector* params) {
  return std::find(params->begin(), params->end(), param) != params->end();
}

}  // anonymous namespace

void Param::AddInputsRecursive(const Param* original,
                               ParamVector* params) const {
  if (owner_) {
    ParamVector owner_params;
    owner_->GetInputsForParam(this, &owner_params);
    for (unsigned ii = 0; ii < owner_params.size(); ++ii) {
      Param* param = owner_params[ii];
      if (param != original && !ParamInParams(param, params)) {
        params->push_back(param);
        param->AddInputsRecursive(original, params);
      }
    }
  }
  if (input_connection_ &&
      input_connection_ != original &&
      !ParamInParams(input_connection_, params)) {
    params->push_back(input_connection_);
    input_connection_->AddInputsRecursive(original, params);
  }
}

void Param::AddOutputsRecursive(const Param* original,
                                ParamVector* params) const {
  if (owner_) {
    ParamVector owner_params;
    owner_->GetOutputsForParam(this, &owner_params);
    for (unsigned ii = 0; ii < owner_params.size(); ++ii) {
      Param* param = owner_params[ii];
      if (param != original && !ParamInParams(param, params)) {
        params->push_back(param);
        param->AddOutputsRecursive(original, params);
      }
    }
  }
  for (unsigned ii = 0; ii < output_connections_.size(); ++ii) {
    Param* param = output_connections_[ii];
    if (param != original && !ParamInParams(param, params)) {
      params->push_back(param);
      param->AddOutputsRecursive(original, params);
    }
  }
}

void Param::GetInputs(ParamVector* params) const {
  DCHECK(params);
  params->clear();
  AddInputsRecursive(this, params);
}

void Param::GetOutputs(ParamVector* params) const {
  DCHECK(params);
  params->clear();
  AddOutputsRecursive(this, params);
}

void Param::InvalidateAllOutputs() const {
  ParamVector params;
  GetOutputs(&params);
  for (unsigned ii = 0; ii < params.size(); ++ii) {
    params[ii]->Invalidate();
  }
}

void Param::InvalidateAllInputs() {
  ParamVector params;
  GetInputs(&params);
  for (unsigned ii = 0; ii < params.size(); ++ii) {
    params[ii]->Invalidate();
  }
  Invalidate();
}

void Param::InvalidateAllParameters() {
  evaluation_counter_->InvalidateAllParameters();
}

void Param::SetNotCachable() {
  DCHECK(not_cachable_count_ == 0);
  not_cachable_count_ = 1;
}

void Param::IncrementNotCachableCountOnParamChainForInput(Param* input) {
  if (input && !input->cachable()) {
    ++not_cachable_count_;
    ParamVector params;
    GetOutputs(&params);
    for (ParamVector::size_type ii = 0; ii < params.size(); ++ii) {
      ++params[ii]->not_cachable_count_;
    }
  }
}

void Param::DecrementNotCachableCountOnParamChainForInput(Param* input) {
  if (input && !input->cachable()) {
    --not_cachable_count_;
    ParamVector params;
    GetOutputs(&params);
    for (ParamVector::size_type ii = 0; ii < params.size(); ++ii) {
      --params[ii]->not_cachable_count_;
    }
  }
}

// Connects two Params using a direct connection or unbinds the input if NULL
// is specified as the source Param.
bool Param::Bind(Param *source_param) {
  if (!source_param) {
    UnbindInput();
    return true;
  }

  // When we clear a previous input connection our ref count could go to zero if
  // we don't hold this reference.
  Param::Ref temp(this);

  if (read_only_) {
    O3D_ERROR(service_locator()) << "attempt to bind source param '"
                                 << source_param->name()
                                 << "' to read only param '" << name() << "'";
    return false;
  }

  // Check to make sure the two Params are of the same type
  if (!source_param->IsA(GetClass())) {
    O3D_ERROR(service_locator())
        << "attempt to bind incompatible source param '"
        << source_param->name() << "' of type '"
        << source_param->GetClassName()
        << "' to read only param '" << name()
        << "' of type '" << GetClassName() << "'";
    return false;
  }

  // Check if we are connecting the same thing. It's either this or holding
  // a temp reference to the source becuase when we clear input_connection_
  // the ref count of source_param could go to zero and get freed.
  if (source_param == input_connection_) {
    return true;
  }

  // if we already had an input connection, disconnect it.
  if (input_connection_) {
    // UnbindOutput will clear input_connection_.
    bool result = input_connection_->UnbindOutput(this);
    DCHECK(result);
  }

  DCHECK(input_connection_ == NULL);

  // If our input is not cachable we need to increment the not_cachable_count
  // for ourselves and all the outputs further down the chain.
  IncrementNotCachableCountOnParamChainForInput(source_param);

  // Everything checks out, bind these params.
  input_connection_ = Param::Ref(source_param);
  source_param->AddOutputConnection(this);
  evaluation_counter_->InvalidateAllParameters();
  OnAfterBindInput();
  return true;
}

// Breaks any input connection.
void Param::UnbindInput() {
  Param *source_param = input_connection();
  if (source_param != NULL) {
    bool success = source_param->UnbindOutput(this);
    DLOG_ASSERT(success);
    DCHECK(input_connection_ == NULL);
  }
}

// Breaks all output connections on the given param.
void Param::UnbindOutputs() {
  // We need to keep a ref to ourselves because as inputs get cleared
  // they could release the last reference to us.
  Param::Ref temp(this);

  const ParamVector& params(output_connections());

  while (!params.empty()) {
    Param* param = params.front();
    param->UnbindInput();
  }
}

void Param::ReportReadOnlyError() {
  O3D_ERROR(service_locator()) << "attempt to set read only param '"
                               << name() << "'";
}

void Param::ReportDynamicSetError() {
  O3D_ERROR(service_locator()) << "attempt to set dynamic param '"
                               << name() << "'";
}

ObjectBase::Ref ParamFloat::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamFloat(service_locator, false, false));
}

ObjectBase::Ref ParamFloat2::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamFloat2(service_locator, false, false));
}

ObjectBase::Ref ParamFloat3::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamFloat3(service_locator, false, false));
}

ObjectBase::Ref ParamFloat4::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamFloat4(service_locator, false, false));
}

ObjectBase::Ref ParamInteger::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamInteger(service_locator, false, false));
}

ObjectBase::Ref ParamBoolean::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamBoolean(service_locator, false, false));
}

ObjectBase::Ref ParamString::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamString(service_locator, false, false));
}

ObjectBase::Ref ParamMatrix4::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamMatrix4(service_locator, false, false));
}
}  // namespace o3d
