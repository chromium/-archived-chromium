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


// This file implements some optics utility functions and the thin-layer
// iridescence texture generation function.

#include <math.h>
#include <utility>
#include "command_buffer/samples/bubble/iridescence_texture.h"
#include "command_buffer/samples/bubble/utils.h"

namespace o3d {
namespace command_buffer {

// Computes the fresnel coefficients for amplitude.
// http://physics.tamuk.edu/~suson/html/4323/prop-em.html has them.
FresnelCoefficients ComputeFresnel(float n, float cos_i, float cos_t) {
  FresnelCoefficients coeff;
  coeff.reflected_perp = (cos_i - n*cos_t)/(cos_i + n*cos_t);
  coeff.transmitted_perp = 2.f*cos_i/(cos_i + n*cos_t);
  coeff.reflected_para = (n*cos_i - cos_t)/(n*cos_i + cos_t);
  coeff.transmitted_para = 2.f*cos_i/(n*cos_i + cos_t);
  return coeff;
}

// Snell-Descartes' Law: sin_i = n * sin_t.
float RefractedRay(float n, float cos_i) {
  float sin2_i = 1.f - cos_i * cos_i;
  float sin2_t = sin2_i / (n*n);
  float cos2_t = 1.f - sin2_t;
  float cos_t = sqrtf(std::max(0.f, cos2_t));
  return cos_t;
}

// Understanding the notations in the following two functions.
//
//              \ A       \ A'          / B         i = incident angle.
//   incident ray \         \         /             t = transmitted angle.
//                  \ i|      \ i|i /
//                    \|        \|/      air (n = 1) outside the bubble
// -outer-interface----P---------R---------------------------------------
//                     |\       /|      ^
//                     |t\     /t|      |thin layer (e.g. water, n > 1)
//        transmitted ray \t|t/         |thickness
//                         \|/          v
// -inner-interface---------Q--------------------------------------------
//                          |\           air (n = 1) inside the bubble
//                          |i \
//                               \ C
//
// Incident ray A gets refracted by the outer interface at point P, then
// reflected by the inner interface at point Q, then refracted into B at point R
// ("trt" ray). At the same time, incident ray A', coherent with A, gets
// directly reflected into B ("r" ray) at point R, so it interferes with the
// "trt" ray.
// At point Q the ray also gets refracted inside the bubble, leading to the
// "tt" ray C.


// Computes the interference between the reflected ray ('r' - one reflection
// one the outer interface) and the reflection of the transmitted ray ('trt' -
// transmitted through the outer interface, reflected on the inner interface,
// then transmitted again through the outer interface).
//
// Parameters:
//   thickness: the thickness of the medium between the interfaces.
//   wavelength: the wavelength of the incident light.
//   n: the refraction index of the medium (relative to the outer medium).
//   r_perp: the amplitude coefficient for the r ray for perpendicular
//     polarization.
//   r_para: the amplitude coefficient for the r ray for parallel polarization.
//   trt_perp: the amplitude coefficient for the trt ray for perpendicular
//     polarization.
//   trt_para: the amplitude coefficient for the trt ray for parallel
//     polarization.
//   cos_t: the cosine of the refracted angle.
// Returns:
//   The reflected power after interference.
float Interference(float thickness,
                   float wavelength,
                   float n,
                   float r_perp,
                   float r_para,
                   float trt_perp,
                   float trt_para,
                   float cos_t) {
  // Difference in wave propagation between the trt ray and the r ray.
  float delta_phase = 2.f * thickness/wavelength * n * cos_t;
  // For a given polarization, power = ||r + trt * e^(i*2*pi*delta_phase)||^2
  float cos_delta = cosf(2.f * kPi * delta_phase);
  float power_perp =
      r_perp*r_perp + trt_perp*trt_perp + 2.f * r_perp*trt_perp*cos_delta;
  float power_para =
      r_para*r_para + trt_para*trt_para + 2.f * r_para*trt_para*cos_delta;
  // Total power is the average between 2 polarization modes (for non-polarized
  // light).
  return (power_perp+power_para)/2.f;
}

void MakeIridescenceTexture(unsigned int width,
                            unsigned int height,
                            float n,
                            float max_thickness,
                            unsigned char *texture) {
  for (unsigned int y = 0; y < height; ++y) {
    float thickness = (y + .5f) * max_thickness / height;
    for (unsigned int x = 0; x < width; ++x) {
      float cos_i = (x + .5f) * 1.f / width;
      float cos_t = RefractedRay(n, cos_i);
      // Fresnel coefficient for the "outer" interface (outside->inside).
      FresnelCoefficients outer = ComputeFresnel(n, cos_i, cos_t);
      // Fresnel coefficient for the "inner" interface (inside->outside).
      FresnelCoefficients inner = ComputeFresnel(1.f/n, cos_t, cos_i);
      float r_perp = outer.reflected_perp;
      float r_para = outer.reflected_para;
      float trt_perp = outer.transmitted_perp * inner.reflected_perp *
                       inner.transmitted_perp;
      float trt_para = outer.transmitted_para * inner.reflected_para *
                       inner.transmitted_para;
      float red = Interference(thickness, kRedWavelength, n, r_perp, r_para,
                               trt_perp, trt_para, cos_t);
      float green = Interference(thickness, kGreenWavelength, n, r_perp, r_para,
                                 trt_perp, trt_para, cos_t);
      float blue = Interference(thickness, kBlueWavelength, n, r_perp, r_para,
                                trt_perp, trt_para, cos_t);
      float tt_perp = outer.transmitted_perp * inner.transmitted_perp;
      float tt_para = outer.transmitted_para * inner.transmitted_para;
      float alpha = (tt_perp*tt_perp + tt_para*tt_para)/2.f;
      *texture++ = ToChar(blue);
      *texture++ = ToChar(green);
      *texture++ = ToChar(red);
      *texture++ = ToChar(alpha);
    }
  }
}

}  // namespace command_buffer
}  // namespace o3d
