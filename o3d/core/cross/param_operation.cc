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


// This file contains the implementation of various Parameter Operations.

#include "core/cross/precompile.h"
#include "core/cross/param_operation.h"

namespace o3d {

O3D_DEFN_CLASS(ParamOp2FloatsToFloat2, ParamObject);
O3D_DEFN_CLASS(ParamOp3FloatsToFloat3, ParamObject);
O3D_DEFN_CLASS(ParamOp4FloatsToFloat4, ParamObject);
O3D_DEFN_CLASS(ParamOp16FloatsToMatrix4, ParamObject);
O3D_DEFN_CLASS(TRSToMatrix4, ParamObject);

const char* ParamOp2FloatsToFloat2::kInput0ParamName =
    O3D_STRING_CONSTANT("input0");
const char* ParamOp2FloatsToFloat2::kInput1ParamName =
    O3D_STRING_CONSTANT("input1");
const char* ParamOp2FloatsToFloat2::kOutputParamName =
    O3D_STRING_CONSTANT("output");

ParamOp2FloatsToFloat2::ParamOp2FloatsToFloat2(ServiceLocator* service_locator)
    : ParamObject(service_locator) {
  RegisterParamRef(kInput0ParamName, &input_0_param_);
  RegisterParamRef(kInput1ParamName, &input_1_param_);
  SlaveParamFloat2::RegisterParamRef(kOutputParamName,
                                     &output_param_,
                                     this);
}

void ParamOp2FloatsToFloat2::UpdateOutputs() {
  if (output_param_->input_connection() == NULL) {
    output_param_->set_dynamic_value(Float2(input_0_param_->value(),
                                            input_1_param_->value()));
  }
}

ObjectBase::Ref ParamOp2FloatsToFloat2::Create(
    ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamOp2FloatsToFloat2(service_locator));
}

const char* ParamOp3FloatsToFloat3::kInput0ParamName =
    O3D_STRING_CONSTANT("input0");
const char* ParamOp3FloatsToFloat3::kInput1ParamName =
    O3D_STRING_CONSTANT("input1");
const char* ParamOp3FloatsToFloat3::kInput2ParamName =
    O3D_STRING_CONSTANT("input2");
const char* ParamOp3FloatsToFloat3::kOutputParamName =
    O3D_STRING_CONSTANT("output");

ParamOp3FloatsToFloat3::ParamOp3FloatsToFloat3(ServiceLocator* service_locator)
    : ParamObject(service_locator) {
  RegisterParamRef(kInput0ParamName, &input_0_param_);
  RegisterParamRef(kInput1ParamName, &input_1_param_);
  RegisterParamRef(kInput2ParamName, &input_2_param_);
  SlaveParamFloat3::RegisterParamRef(kOutputParamName,
                                     &output_param_,
                                     this);
}

void ParamOp3FloatsToFloat3::UpdateOutputs() {
  if (output_param_->input_connection() == NULL) {
    output_param_->set_dynamic_value(Float3(input_0_param_->value(),
                                            input_1_param_->value(),
                                            input_2_param_->value()));
  }
}

ObjectBase::Ref ParamOp3FloatsToFloat3::Create(
    ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamOp3FloatsToFloat3(service_locator));
}

const char* ParamOp4FloatsToFloat4::kInput0ParamName =
    O3D_STRING_CONSTANT("input0");
const char* ParamOp4FloatsToFloat4::kInput1ParamName =
    O3D_STRING_CONSTANT("input1");
const char* ParamOp4FloatsToFloat4::kInput2ParamName =
    O3D_STRING_CONSTANT("input2");
const char* ParamOp4FloatsToFloat4::kInput3ParamName =
    O3D_STRING_CONSTANT("input3");
const char* ParamOp4FloatsToFloat4::kOutputParamName =
    O3D_STRING_CONSTANT("output");

ParamOp4FloatsToFloat4::ParamOp4FloatsToFloat4(ServiceLocator* service_locator)
    : ParamObject(service_locator) {
  RegisterParamRef(kInput0ParamName, &input_0_param_);
  RegisterParamRef(kInput1ParamName, &input_1_param_);
  RegisterParamRef(kInput2ParamName, &input_2_param_);
  RegisterParamRef(kInput3ParamName, &input_3_param_);
  SlaveParamFloat4::RegisterParamRef(kOutputParamName,
                                     &output_param_,
                                     this);
}

void ParamOp4FloatsToFloat4::UpdateOutputs() {
  if (output_param_->input_connection() == NULL) {
    output_param_->set_dynamic_value(Float4(input_0_param_->value(),
                                            input_1_param_->value(),
                                            input_2_param_->value(),
                                            input_3_param_->value()));
  }
}

ObjectBase::Ref ParamOp4FloatsToFloat4::Create(
    ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamOp4FloatsToFloat4(service_locator));
}

const char* ParamOp16FloatsToMatrix4::kInput0ParamName =
    O3D_STRING_CONSTANT("input0");
const char* ParamOp16FloatsToMatrix4::kInput1ParamName =
    O3D_STRING_CONSTANT("input1");
const char* ParamOp16FloatsToMatrix4::kInput2ParamName =
    O3D_STRING_CONSTANT("input2");
const char* ParamOp16FloatsToMatrix4::kInput3ParamName =
    O3D_STRING_CONSTANT("input3");
const char* ParamOp16FloatsToMatrix4::kInput4ParamName =
    O3D_STRING_CONSTANT("input4");
const char* ParamOp16FloatsToMatrix4::kInput5ParamName =
    O3D_STRING_CONSTANT("input5");
const char* ParamOp16FloatsToMatrix4::kInput6ParamName =
    O3D_STRING_CONSTANT("input6");
const char* ParamOp16FloatsToMatrix4::kInput7ParamName =
    O3D_STRING_CONSTANT("input7");
const char* ParamOp16FloatsToMatrix4::kInput8ParamName =
    O3D_STRING_CONSTANT("input8");
const char* ParamOp16FloatsToMatrix4::kInput9ParamName =
    O3D_STRING_CONSTANT("input9");
const char* ParamOp16FloatsToMatrix4::kInput10ParamName =
    O3D_STRING_CONSTANT("input10");
const char* ParamOp16FloatsToMatrix4::kInput11ParamName =
    O3D_STRING_CONSTANT("input11");
const char* ParamOp16FloatsToMatrix4::kInput12ParamName =
    O3D_STRING_CONSTANT("input12");
const char* ParamOp16FloatsToMatrix4::kInput13ParamName =
    O3D_STRING_CONSTANT("input13");
const char* ParamOp16FloatsToMatrix4::kInput14ParamName =
    O3D_STRING_CONSTANT("input14");
const char* ParamOp16FloatsToMatrix4::kInput15ParamName =
    O3D_STRING_CONSTANT("input15");
const char* ParamOp16FloatsToMatrix4::kOutputParamName =
    O3D_STRING_CONSTANT("output");

ParamOp16FloatsToMatrix4::ParamOp16FloatsToMatrix4(
    ServiceLocator* service_locator)
    : ParamObject(service_locator) {
  RegisterParamRef(kInput0ParamName, &input_0_param_);
  RegisterParamRef(kInput1ParamName, &input_1_param_);
  RegisterParamRef(kInput2ParamName, &input_2_param_);
  RegisterParamRef(kInput3ParamName, &input_3_param_);
  RegisterParamRef(kInput4ParamName, &input_4_param_);
  RegisterParamRef(kInput5ParamName, &input_5_param_);
  RegisterParamRef(kInput6ParamName, &input_6_param_);
  RegisterParamRef(kInput7ParamName, &input_7_param_);
  RegisterParamRef(kInput8ParamName, &input_8_param_);
  RegisterParamRef(kInput9ParamName, &input_9_param_);
  RegisterParamRef(kInput10ParamName, &input_10_param_);
  RegisterParamRef(kInput11ParamName, &input_11_param_);
  RegisterParamRef(kInput12ParamName, &input_12_param_);
  RegisterParamRef(kInput13ParamName, &input_13_param_);
  RegisterParamRef(kInput14ParamName, &input_14_param_);
  RegisterParamRef(kInput15ParamName, &input_15_param_);
  SlaveParamMatrix4::RegisterParamRef(kOutputParamName,
                                      &output_param_,
                                      this);
  // Make it the identity matrix by default.
  set_input_0(1.0f);
  set_input_5(1.0f);
  set_input_10(1.0f);
  set_input_15(1.0f);
}

void ParamOp16FloatsToMatrix4::UpdateOutputs() {
  if (output_param_->input_connection() == NULL) {
    output_param_->set_dynamic_value(
        Matrix4(Vector4(input_0_param_->value(),
                        input_1_param_->value(),
                        input_2_param_->value(),
                        input_3_param_->value()),
                Vector4(input_4_param_->value(),
                        input_5_param_->value(),
                        input_6_param_->value(),
                        input_7_param_->value()),
                Vector4(input_8_param_->value(),
                        input_9_param_->value(),
                        input_10_param_->value(),
                        input_11_param_->value()),
                Vector4(input_12_param_->value(),
                        input_13_param_->value(),
                        input_14_param_->value(),
                        input_15_param_->value())));
  }
}

ObjectBase::Ref ParamOp16FloatsToMatrix4::Create(
    ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamOp16FloatsToMatrix4(service_locator));
}

const char* TRSToMatrix4::kTranslateXParamName =
    O3D_STRING_CONSTANT("translateX");
const char* TRSToMatrix4::kTranslateYParamName =
    O3D_STRING_CONSTANT("translateY");
const char* TRSToMatrix4::kTranslateZParamName =
    O3D_STRING_CONSTANT("translateZ");
const char* TRSToMatrix4::kRotateXParamName =
    O3D_STRING_CONSTANT("rotateX");
const char* TRSToMatrix4::kRotateYParamName =
    O3D_STRING_CONSTANT("rotateY");
const char* TRSToMatrix4::kRotateZParamName =
    O3D_STRING_CONSTANT("rotateZ");
const char* TRSToMatrix4::kScaleXParamName =
    O3D_STRING_CONSTANT("scaleX");
const char* TRSToMatrix4::kScaleYParamName =
    O3D_STRING_CONSTANT("scaleY");
const char* TRSToMatrix4::kScaleZParamName =
    O3D_STRING_CONSTANT("scaleZ");
const char* TRSToMatrix4::kOutputParamName =
    O3D_STRING_CONSTANT("output");

TRSToMatrix4::TRSToMatrix4(ServiceLocator* service_locator)
    : ParamObject(service_locator) {
  RegisterParamRef(kTranslateXParamName, &translate_x_param_);
  RegisterParamRef(kTranslateYParamName, &translate_y_param_);
  RegisterParamRef(kTranslateZParamName, &translate_z_param_);
  RegisterParamRef(kRotateXParamName, &rotate_x_param_);
  RegisterParamRef(kRotateYParamName, &rotate_y_param_);
  RegisterParamRef(kRotateZParamName, &rotate_z_param_);
  RegisterParamRef(kScaleXParamName, &scale_x_param_);
  RegisterParamRef(kScaleYParamName, &scale_y_param_);
  RegisterParamRef(kScaleZParamName, &scale_z_param_);
  SlaveParamMatrix4::RegisterParamRef(kOutputParamName,
                                      &output_param_,
                                      this);
  // Make it the identity matrix by default.
  set_scale_x(1.0f);
  set_scale_y(1.0f);
  set_scale_z(1.0f);
}

void TRSToMatrix4::UpdateOutputs() {
  if (output_param_->input_connection() == NULL) {
    output_param_->set_dynamic_value(
        Matrix4::translation(Vector3(translate_x_param_->value(),
                                     translate_y_param_->value(),
                                     translate_z_param_->value())) *
        Matrix4::rotationZYX(Vector3(rotate_x_param_->value(),
                                     rotate_y_param_->value(),
                                     rotate_z_param_->value())) *
        Matrix4::scale(Vector3(scale_x_param_->value(),
                               scale_y_param_->value(),
                               scale_z_param_->value())));
  }
}

ObjectBase::Ref TRSToMatrix4::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new TRSToMatrix4(service_locator));
}

}  // namespace o3d


