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


// This file defines some utility functions.

#ifndef O3D_COMMAND_BUFFER_SAMPLES_BUBBLE_UTILS_H_
#define O3D_COMMAND_BUFFER_SAMPLES_BUBBLE_UTILS_H_

#include <math.h>
#include <stdlib.h>
#include <algorithm>

namespace o3d {
namespace command_buffer {

const float kPi = 3.14159265359f;

// Returns a random value between min and max.
inline float Randf(float min, float max, unsigned int *seed) {
  return min + (max - min) / RAND_MAX * rand_r(seed);
}

// Converts a [0..1] float to a [0..255] color value.
inline unsigned char ToChar(float x) {
  x = std::min(1.f, std::max(0.f, x));
  return static_cast<unsigned char>(x * 255.f);
}

// Maps [0..1] into [0..1], using a smooth (C^2) function,
// with f(0)=0, f(1)=1, f'(0)=f'(1)=0.
// This version is simply a cubic interpolation between 0 and 1.
inline float SmoothStep(float x) {
  return (3.f - 2.f * x) * x * x;
}

// Interpolates between a and b, with a ratio of t.
inline float Lerp(float t, float a, float b) {
  return a + (b-a)*t;
}

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SAMPLES_BUBBLE_UTILS_H_
