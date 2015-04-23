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


// This file contains the implementation of Function.

#include "core/cross/precompile.h"
#include "core/cross/function.h"

namespace o3d {

O3D_DEFN_CLASS(FunctionContext, ObjectBase);
O3D_DEFN_CLASS(Function, NamedObject);
O3D_DEFN_CLASS(ParamFunction, RefParamBase);
O3D_DEFN_CLASS(FunctionEval, ParamObject);

FunctionContext::FunctionContext(ServiceLocator* service_locator)
    : ObjectBase(service_locator) {
}

Function::Function(ServiceLocator* service_locator)
    : NamedObject(service_locator),
      weak_pointer_manager_(this) {
}

const char* FunctionEval::kInputParamName =
    O3D_STRING_CONSTANT("input");
const char* FunctionEval::kFunctionObjectParamName =
    O3D_STRING_CONSTANT("functionObject");
const char* FunctionEval::kOutputParamName =
    O3D_STRING_CONSTANT("output");

FunctionEval::FunctionEval(ServiceLocator* service_locator)
    : ParamObject(service_locator),
      function_context_(NULL) {
  RegisterParamRef(kInputParamName, &input_param_);
  RegisterParamRef(kFunctionObjectParamName, &function_object_param_);
  SlaveParamFloat::RegisterParamRef(kOutputParamName,
                                    &output_param_,
                                    this);
}

void FunctionEval::UpdateOutputs() {
  if (output_param_->input_connection() == NULL) {
    Function* function = function_object_param_->value();
    if (function) {
      // If we don't have a function context or if the one we have is the wrong
      // type then get a new one.
      if (!function_context_ ||
          !function_context_->IsA(function->GetFunctionContextClass())) {
        function_context_.Reset();
        function_context_ = FunctionContext::Ref(
            function->CreateFunctionContext());
      }
      output_param_->set_dynamic_value(
          function->Evaluate(input_param_->value(), function_context_.Get()));
    } else {
      output_param_->set_dynamic_value(input_param_->value());
    }
  }
}

ObjectBase::Ref FunctionEval::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new FunctionEval(service_locator));
}

ObjectBase::Ref ParamFunction::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamFunction(service_locator, false, false));
}

}  // namespace o3d


