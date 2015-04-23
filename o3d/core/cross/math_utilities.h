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


// This file contains declarations of various math functions.
//

#ifndef O3D_CORE_CROSS_MATH_UTILITIES_H_
#define O3D_CORE_CROSS_MATH_UTILITIES_H_

#include "core/cross/types.h"

namespace Vectormath {
namespace Aos {

// Creates a perspective projection matrix.
Matrix4 CreatePerspectiveMatrix(float vertical_field_of_view_radians,
                                float aspect,
                                float z_near,
                                float z_far);

// Creates a frustum projection matrix.
Matrix4 CreateFrustumMatrix(float left,
                            float right,
                            float bottom,
                            float top,
                            float z_near,
                            float z_far);

// Creates an orthographic projection matrix.
Matrix4 CreateOrthographicMatrix(float left,
                                 float right,
                                 float bottom,
                                 float top,
                                 float z_near,
                                 float z_far);

// Converts a 32 bit float to a 16 bit float.
uint16 FloatToHalf(float value);
}  // namespace Vectormath
}  // namespace Aos

namespace o3d {
// Calculates the Frobenius norm of a 3x3 matrix.
// See http://en.wikipedia.org/wiki/Matrix_norm
float FrobeniusNorm(const Matrix3& matrix);

// Calculates the Frobenius norm of a 4x4 matrix.
// See http://en.wikipedia.org/wiki/Matrix_norm
float FrobeniusNorm(const Matrix4& matrix);

const float kPi = acosf(-1.0f);
}  // namespace o3d

#endif  // O3D_CORE_CROSS_MATH_UTILITIES_H_




