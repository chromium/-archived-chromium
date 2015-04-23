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


// This file contains the Function declaration.

#ifndef O3D_CORE_CROSS_FUNCTION_H_
#define O3D_CORE_CROSS_FUNCTION_H_

#include "core/cross/param_object.h"

namespace o3d {

// An FunctionContext is passed to a Function to allow caching and other
// performance enhancements. FunctionContext is abstract. To get an
// FunctionContext that is compatible with a particular function call
// Function::CreateFunctionContext
class FunctionContext : public ObjectBase {
 public:
  typedef SmartPointer<FunctionContext> Ref;

 protected:
  explicit FunctionContext(ServiceLocator* service_locator);

 private:
  O3D_DECL_CLASS(FunctionContext, ObjectBase);
  DISALLOW_COPY_AND_ASSIGN(FunctionContext);
};

// A Function is a class that has an Evaluate method. Evaluate takes 1 input and
// returns 1 output.
class Function : public NamedObject {
 public:
  typedef SmartPointer<Function> Ref;
  typedef WeakPointer<Function> WeakPointerType;

  // Gets a output for this function for the given input.
  // Parameters:
  //   input: input to function.
  //   context: FunctionContext compatible with function. May be null.
  // Returns:
  //   The output for the given input.
  virtual float Evaluate(float input, FunctionContext* context) const = 0;

  // Creates an evaluation context that can be used for this Function.
  // Returns:
  //   An evaluation context compatible with this Function.
  virtual FunctionContext* CreateFunctionContext() const = 0;

  // Gets the class of the FunctionObject this Function needs.
  // Returns:
  //   the class of the FunctionObject this Function needs.
  virtual const ObjectBase::Class* GetFunctionContextClass() const = 0;

  // Gets a weak pointer to us.
  WeakPointerType GetWeakPointer() const {
    return weak_pointer_manager_.GetWeakPointer();
  }

 protected:
  explicit Function(ServiceLocator* service_locator);

 private:
  // Manager for weak pointers to us.
  WeakPointerType::WeakPointerManager weak_pointer_manager_;

  O3D_DECL_CLASS(Function, NamedObject);
  DISALLOW_COPY_AND_ASSIGN(Function);
};

// A param that holds a weak pointer to a Function.
class ParamFunction : public TypedRefParam<Function> {
 public:
  typedef SmartPointer<ParamFunction> Ref;

  ParamFunction(ServiceLocator* service_locator, bool dynamic, bool read_only)
      : TypedRefParam<Function>(service_locator, dynamic, read_only) {
  }

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(ParamFunction, RefParamBase);
  DISALLOW_COPY_AND_ASSIGN(ParamFunction);
};

// A class that evaluates a Function through parameters.
class FunctionEval : public ParamObject {
 public:
  typedef SmartPointer<FunctionEval> Ref;

  // Names of Params.
  static const char* kInputParamName;
  static const char* kFunctionObjectParamName;
  static const char* kOutputParamName;

  float input() const {
    return input_param_->value();
  }

  void set_input(float value) {
    input_param_->set_value(value);
  }

  Function* function_object() const {
    return function_object_param_->value();
  }

  void set_function_object(Function* function) {
    function_object_param_->set_value(function);
  }

  float output() const {
    return output_param_->value();
  }

  // Updates the output param.
  void UpdateOutputs();

 private:
  typedef SlaveParam<ParamFloat, FunctionEval> SlaveParamFloat;

  explicit FunctionEval(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  ParamFloat::Ref input_param_;
  ParamFunction::Ref function_object_param_;
  SlaveParamFloat::Ref output_param_;
  FunctionContext::Ref function_context_;

  O3D_DECL_CLASS(FunctionEval, ParamObject);
  DISALLOW_COPY_AND_ASSIGN(FunctionEval);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_FUNCTION_H_




