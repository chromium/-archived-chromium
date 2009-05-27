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


// This file contains the definition of class Matrix4AxisRotation.

#include "core/cross/precompile.h"
#include "core/cross/matrix4_axis_rotation.h"

namespace o3d {

O3D_DEFN_CLASS(Matrix4AxisRotation, ParamObject);

const char* Matrix4AxisRotation::kInputMatrixParamName =
    O3D_STRING_CONSTANT("inputMatrix");
const char* Matrix4AxisRotation::kAxisParamName =
    O3D_STRING_CONSTANT("axis");
const char* Matrix4AxisRotation::kAngleParamName =
    O3D_STRING_CONSTANT("angle");
const char* Matrix4AxisRotation::kOutputMatrixParamName =
    O3D_STRING_CONSTANT("outputMatrix");

Matrix4AxisRotation::Matrix4AxisRotation(ServiceLocator* service_locator)
    : ParamObject(service_locator) {
  RegisterParamRef(kInputMatrixParamName, &input_matrix_param_);
  RegisterParamRef(kAxisParamName, &axis_param_);
  RegisterParamRef(kAngleParamName, &angle_param_);
  SlaveParamMatrix4::RegisterParamRef(kOutputMatrixParamName,
                                      &output_matrix_param_,
                                      this);
}

void Matrix4AxisRotation::UpdateOutputs() {
  Matrix4 matrix = input_matrix() * Matrix4::rotation(
      angle_param_->value(),
      Float3ToVector3(axis_param_->value()));
  output_matrix_param_->set_dynamic_value(matrix);
}

ObjectBase::Ref Matrix4AxisRotation::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new Matrix4AxisRotation(service_locator));
}
}  // namespace o3d
