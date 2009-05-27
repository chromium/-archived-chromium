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


// This file contains the declaration of various Parameter Operations.

#ifndef O3D_CORE_CROSS_PARAM_OPERATION_H_
#define O3D_CORE_CROSS_PARAM_OPERATION_H_

#include "core/cross/param_object.h"

namespace o3d {

// A Param operation that takes 2 floats to produce a Float2.
class ParamOp2FloatsToFloat2 : public ParamObject {
 public:
  static const char* kInput0ParamName;
  static const char* kInput1ParamName;
  static const char* kOutputParamName;

  float input_0() const {
    return input_0_param_->value();
  }

  void set_input_0(float value) {
    input_0_param_->set_value(value);
  }

  float input_1() const {
    return input_1_param_->value();
  }

  void set_input_1(float value) {
    input_1_param_->set_value(value);
  }

  Float2 output() const {
    return output_param_->value();
  }

  // Updates the output param.
  void UpdateOutputs();

 private:
  typedef SlaveParam<ParamFloat2, ParamOp2FloatsToFloat2> SlaveParamFloat2;

  explicit ParamOp2FloatsToFloat2(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  ParamFloat::Ref input_0_param_;
  ParamFloat::Ref input_1_param_;
  SlaveParamFloat2::Ref output_param_;

  O3D_DECL_CLASS(ParamOp2FloatsToFloat2, ParamObject);
  DISALLOW_COPY_AND_ASSIGN(ParamOp2FloatsToFloat2);
};

// A Param operation that takes 3 floats to produce a Float3.
class ParamOp3FloatsToFloat3 : public ParamObject {
 public:
  static const char* kInput0ParamName;
  static const char* kInput1ParamName;
  static const char* kInput2ParamName;
  static const char* kOutputParamName;

  float input_0() const {
    return input_0_param_->value();
  }

  void set_input_0(float value) {
    input_0_param_->set_value(value);
  }

  float input_1() const {
    return input_1_param_->value();
  }

  void set_input_1(float value) {
    input_1_param_->set_value(value);
  }

  float input_2() const {
    return input_2_param_->value();
  }

  void set_input_2(float value) {
    input_2_param_->set_value(value);
  }

  Float3 output() const {
    return output_param_->value();
  }

  // Updates the output param.
  void UpdateOutputs();

 private:
  typedef SlaveParam<ParamFloat3, ParamOp3FloatsToFloat3> SlaveParamFloat3;

  explicit ParamOp3FloatsToFloat3(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  ParamFloat::Ref input_0_param_;
  ParamFloat::Ref input_1_param_;
  ParamFloat::Ref input_2_param_;
  SlaveParamFloat3::Ref output_param_;

  O3D_DECL_CLASS(ParamOp3FloatsToFloat3, ParamObject);
  DISALLOW_COPY_AND_ASSIGN(ParamOp3FloatsToFloat3);
};

// A Param operation that takes 4 floats to produce a Float4.
class ParamOp4FloatsToFloat4 : public ParamObject {
 public:
  static const char* kInput0ParamName;
  static const char* kInput1ParamName;
  static const char* kInput2ParamName;
  static const char* kInput3ParamName;
  static const char* kOutputParamName;

  float input_0() const {
    return input_0_param_->value();
  }

  void set_input_0(float value) {
    input_0_param_->set_value(value);
  }

  float input_1() const {
    return input_1_param_->value();
  }

  void set_input_1(float value) {
    input_1_param_->set_value(value);
  }

  float input_2() const {
    return input_2_param_->value();
  }

  void set_input_2(float value) {
    input_2_param_->set_value(value);
  }

  float input_3() const {
    return input_3_param_->value();
  }

  void set_input_3(float value) {
    input_3_param_->set_value(value);
  }

  Float4 output() const {
    return output_param_->value();
  }

  // Updates the output param.
  void UpdateOutputs();

 private:
  typedef SlaveParam<ParamFloat4, ParamOp4FloatsToFloat4> SlaveParamFloat4;

  explicit ParamOp4FloatsToFloat4(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  ParamFloat::Ref input_0_param_;
  ParamFloat::Ref input_1_param_;
  ParamFloat::Ref input_2_param_;
  ParamFloat::Ref input_3_param_;
  SlaveParamFloat4::Ref output_param_;

  O3D_DECL_CLASS(ParamOp4FloatsToFloat4, ParamObject);
  DISALLOW_COPY_AND_ASSIGN(ParamOp4FloatsToFloat4);
};

// A Param operation that takes 16 floats to produce a Matrix4.
class ParamOp16FloatsToMatrix4 : public ParamObject {
 public:
  static const char* kInput0ParamName;
  static const char* kInput1ParamName;
  static const char* kInput2ParamName;
  static const char* kInput3ParamName;
  static const char* kInput4ParamName;
  static const char* kInput5ParamName;
  static const char* kInput6ParamName;
  static const char* kInput7ParamName;
  static const char* kInput8ParamName;
  static const char* kInput9ParamName;
  static const char* kInput10ParamName;
  static const char* kInput11ParamName;
  static const char* kInput12ParamName;
  static const char* kInput13ParamName;
  static const char* kInput14ParamName;
  static const char* kInput15ParamName;
  static const char* kOutputParamName;

  float input_0() const {
    return input_0_param_->value();
  }

  void set_input_0(float value) {
    input_0_param_->set_value(value);
  }

  float input_1() const {
    return input_1_param_->value();
  }

  void set_input_1(float value) {
    input_1_param_->set_value(value);
  }

  float input_2() const {
    return input_2_param_->value();
  }

  void set_input_2(float value) {
    input_2_param_->set_value(value);
  }

  float input_3() const {
    return input_3_param_->value();
  }

  void set_input_3(float value) {
    input_3_param_->set_value(value);
  }

  float input_4() const {
    return input_4_param_->value();
  }

  void set_input_4(float value) {
    input_4_param_->set_value(value);
  }

  float input_5() const {
    return input_5_param_->value();
  }

  void set_input_5(float value) {
    input_5_param_->set_value(value);
  }

  float input_6() const {
    return input_6_param_->value();
  }

  void set_input_6(float value) {
    input_6_param_->set_value(value);
  }

  float input_7() const {
    return input_7_param_->value();
  }

  void set_input_7(float value) {
    input_7_param_->set_value(value);
  }

  float input_8() const {
    return input_8_param_->value();
  }

  void set_input_8(float value) {
    input_8_param_->set_value(value);
  }

  float input_9() const {
    return input_9_param_->value();
  }

  void set_input_9(float value) {
    input_9_param_->set_value(value);
  }

  float input_10() const {
    return input_10_param_->value();
  }

  void set_input_10(float value) {
    input_10_param_->set_value(value);
  }

  float input_11() const {
    return input_11_param_->value();
  }

  void set_input_11(float value) {
    input_11_param_->set_value(value);
  }

  float input_12() const {
    return input_12_param_->value();
  }

  void set_input_12(float value) {
    input_12_param_->set_value(value);
  }

  float input_13() const {
    return input_13_param_->value();
  }

  void set_input_13(float value) {
    input_13_param_->set_value(value);
  }

  float input_14() const {
    return input_14_param_->value();
  }

  void set_input_14(float value) {
    input_14_param_->set_value(value);
  }

  float input_15() const {
    return input_15_param_->value();
  }

  void set_input_15(float value) {
    input_15_param_->set_value(value);
  }

  Matrix4 output() const {
    return output_param_->value();
  }

  // Updates the output param.
  void UpdateOutputs();

 private:
  typedef SlaveParam<ParamMatrix4, ParamOp16FloatsToMatrix4> SlaveParamMatrix4;

  explicit ParamOp16FloatsToMatrix4(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  ParamFloat::Ref input_0_param_;
  ParamFloat::Ref input_1_param_;
  ParamFloat::Ref input_2_param_;
  ParamFloat::Ref input_3_param_;
  ParamFloat::Ref input_4_param_;
  ParamFloat::Ref input_5_param_;
  ParamFloat::Ref input_6_param_;
  ParamFloat::Ref input_7_param_;
  ParamFloat::Ref input_8_param_;
  ParamFloat::Ref input_9_param_;
  ParamFloat::Ref input_10_param_;
  ParamFloat::Ref input_11_param_;
  ParamFloat::Ref input_12_param_;
  ParamFloat::Ref input_13_param_;
  ParamFloat::Ref input_14_param_;
  ParamFloat::Ref input_15_param_;
  SlaveParamMatrix4::Ref output_param_;

  O3D_DECL_CLASS(ParamOp16FloatsToMatrix4, ParamObject);
  DISALLOW_COPY_AND_ASSIGN(ParamOp16FloatsToMatrix4);
};

// A Param operation that takes 9 floats to produce a Matrix4.
// It is effectively this:
//
//  output = Matrix4::translation(translateX, translateY, translateZ) *
//           Matrix4::rotationXYZ(rotateX, rotateY, rotateZ) *
//           Matrix4::scale(scaleX, scaleY, scaleZ);
class TRSToMatrix4 : public ParamObject {
 public:
  static const char* kTranslateXParamName;
  static const char* kTranslateYParamName;
  static const char* kTranslateZParamName;
  static const char* kRotateXParamName;
  static const char* kRotateYParamName;
  static const char* kRotateZParamName;
  static const char* kScaleXParamName;
  static const char* kScaleYParamName;
  static const char* kScaleZParamName;
  static const char* kOutputParamName;

  float translate_x() const {
    return translate_x_param_->value();
  }

  void set_translate_x(float value) {
    translate_x_param_->set_value(value);
  }

  float translate_y() const {
    return translate_y_param_->value();
  }

  void set_translate_y(float value) {
    translate_y_param_->set_value(value);
  }

  float translate_z() const {
    return translate_z_param_->value();
  }

  void set_translate_z(float value) {
    translate_z_param_->set_value(value);
  }

  float rotate_x() const {
    return rotate_x_param_->value();
  }

  void set_rotate_x(float value) {
    rotate_x_param_->set_value(value);
  }

  float rotate_y() const {
    return rotate_y_param_->value();
  }

  void set_rotate_y(float value) {
    rotate_y_param_->set_value(value);
  }

  float rotate_z() const {
    return rotate_z_param_->value();
  }

  void set_rotate_z(float value) {
    rotate_z_param_->set_value(value);
  }

  float scale_x() const {
    return scale_x_param_->value();
  }

  void set_scale_x(float value) {
    scale_x_param_->set_value(value);
  }

  float scale_y() const {
    return scale_y_param_->value();
  }

  void set_scale_y(float value) {
    scale_y_param_->set_value(value);
  }

  float scale_z() const {
    return scale_z_param_->value();
  }

  void set_scale_z(float value) {
    scale_z_param_->set_value(value);
  }

  Matrix4 output() const {
    return output_param_->value();
  }

  // Updates the output param.
  void UpdateOutputs();

 private:
  typedef SlaveParam<ParamMatrix4, TRSToMatrix4> SlaveParamMatrix4;

  explicit TRSToMatrix4(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  ParamFloat::Ref translate_x_param_;
  ParamFloat::Ref translate_y_param_;
  ParamFloat::Ref translate_z_param_;
  ParamFloat::Ref rotate_x_param_;
  ParamFloat::Ref rotate_y_param_;
  ParamFloat::Ref rotate_z_param_;
  ParamFloat::Ref scale_x_param_;
  ParamFloat::Ref scale_y_param_;
  ParamFloat::Ref scale_z_param_;
  SlaveParamMatrix4::Ref output_param_;

  O3D_DECL_CLASS(TRSToMatrix4, ParamObject);
  DISALLOW_COPY_AND_ASSIGN(TRSToMatrix4);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_PARAM_OPERATION_H_




