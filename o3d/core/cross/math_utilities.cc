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


// The file contains defintions of various math related functions.

#include <math.h>
#include "core/cross/precompile.h"
#include "core/cross/math_utilities.h"

static const float kEpsilon = 0.00001f;

namespace Vectormath {
namespace Aos {

// Creates a perspective projection matrix.
Matrix4 CreatePerspectiveMatrix(float vertical_field_of_view_radians,
                                float aspect,
                                float z_near,
                                float z_far) {
  float dz = z_near - z_far;
  if (aspect > kEpsilon && fabsf(dz) > kEpsilon) {
    float vertical_scale = 1.0f / tanf(vertical_field_of_view_radians / 2.0f);
    float horizontal_scale = vertical_scale / aspect;
    return Matrix4(Vector4(horizontal_scale, 0.0f, 0.0f, 0.0f),
                   Vector4(0.0f, vertical_scale, 0.0f, 0.0f),
                   Vector4(0.0f, 0.0f, z_far / dz, -1.0f),
                   Vector4(0.0f, 0.0f, z_near * z_far / dz, 0.0f));
  }
  return Matrix4::identity();
}

// Creates an arbitrary frustum projection matrix.
Matrix4 CreateFrustumMatrix(float left,
                            float right,
                            float bottom,
                            float top,
                            float z_near,
                            float z_far) {
  float dx = (right - left);
  float dy = (top - bottom);
  float dz = (z_near - z_far);

  if (fabsf(dx) > kEpsilon && fabsf(dy) > kEpsilon && fabsf(dz) > kEpsilon) {
    return Matrix4(
        Vector4(2.0f * z_near / dx, 0.0f, 0.0f, 0.0f),
        Vector4(0.0f, 2.0f * z_near / dy, 0.0f, 0.0f),
        Vector4((left + right) / dx, (top + bottom) / dy, z_far / dz, -1.0f),
        Vector4(0.0f, 0.0f, z_near * z_far / dz, 0.0f));
  }
  return Matrix4::identity();
}

// Creates an orthographic projection matrix.
Matrix4 CreateOrthographicMatrix(float left,
                                 float right,
                                 float bottom,
                                 float top,
                                 float z_near,
                                 float z_far) {
  if (fabsf(left - right) > kEpsilon &&
      fabsf(top - bottom) > kEpsilon &&
      fabsf(z_near - z_far) > kEpsilon) {
    return Matrix4(
        Vector4(2.0f / (right - left), 0.0f, 0.0f, 0.0f),
        Vector4(0.0f, 2.0f / (top - bottom), 0.0f, 0.0f),
        Vector4(0.0f, 0.0f, 1.0f / (z_near - z_far), 0.0f),
        Vector4((left + right) / (left - right),
                (top + bottom) / (bottom - top),
                z_near / (z_near - z_far),
                1.0f));
  }
  return Matrix4::identity();
}

uint16 FloatToHalf(float value) {
  union FloatAndUInt {
    float f;
    unsigned u;
  } temp;
  temp.f = value;
  unsigned v = temp.u;
  unsigned sign = static_cast<uint16>(v >> 31);
  unsigned mantissa;
  unsigned exponent;
  uint16 half;

  // -15 stored using a single precision bias of 127
  const unsigned HALF_FLOAT_MIN_BIASED_EXP_AS_SINGLE_FP_EXP =
      0x38000000;
  // max exponent value in single precision that will be converted
  // to Inf or Nan when stored as a half-float
  const unsigned HALF_FLOAT_MAX_BIASED_EXP_AS_SINGLE_FP_EXP = 0x47800000;
  // 255 is the max exponent biased value
  const unsigned FLOAT_MAX_BIASED_EXP = (0xFF << 23);
  const unsigned HALF_FLOAT_MAX_BIASED_EXP = (0x1F << 10);

  // get mantissa
  mantissa = v & ((1 << 23) - 1);
  // get exponent bits
  exponent = v & FLOAT_MAX_BIASED_EXP;
  if (exponent >= HALF_FLOAT_MAX_BIASED_EXP_AS_SINGLE_FP_EXP) {
    // check if the original single precision float number is a NaN
    if (mantissa && (exponent == FLOAT_MAX_BIASED_EXP)) {
      // we have a single precision NaN
      mantissa = (1 << 23) - 1;
    } else {
      // 16-bit half-float representation stores number as Inf
      mantissa = 0;
    }
    half = (static_cast<uint16>(sign) << 15) |
           static_cast<uint16>(HALF_FLOAT_MAX_BIASED_EXP) |
           static_cast<uint16>(mantissa >> 13);
  // check if exponent is <= -15
  } else if (exponent <= HALF_FLOAT_MIN_BIASED_EXP_AS_SINGLE_FP_EXP) {
    // store a denorm half-float value or zero
    exponent = (HALF_FLOAT_MIN_BIASED_EXP_AS_SINGLE_FP_EXP -
                exponent) >> 23;
    mantissa >>= (14 + exponent);

    half = (static_cast<uint16>(sign) << 15) |
           static_cast<uint16>(mantissa);
  } else {
    half =
        (static_cast<uint16>(sign) << 15) |
        static_cast<uint16>(
            (exponent - HALF_FLOAT_MIN_BIASED_EXP_AS_SINGLE_FP_EXP) >> 13) |
        static_cast<uint16>(mantissa >> 13);
  }
  return half;
}
}  // namespace Vectormath
}  // namespace Aos

namespace o3d {
float FrobeniusNorm(const Matrix3& matrix) {
  Matrix3 elementsSquared = mulPerElem(matrix, matrix);
  Vector3 ones(1, 1, 1);
  float sumOfElementsSquared = 0.0f;
  for (int i = 0; i < 3; ++i) {
    sumOfElementsSquared += dot(ones, elementsSquared.getCol(i));
  }
  return sqrtf(sumOfElementsSquared);
}

float FrobeniusNorm(const Matrix4& matrix) {
  Matrix4 elementsSquared = mulPerElem(matrix, matrix);
  Vector4 ones(1, 1, 1, 1);
  float sumOfElementsSquared = 0.0f;
  for (int i = 0; i < 4; ++i) {
    sumOfElementsSquared += dot(ones, elementsSquared.getCol(i));
  }
  return sqrtf(sumOfElementsSquared);
}
}  // namespace o3d
