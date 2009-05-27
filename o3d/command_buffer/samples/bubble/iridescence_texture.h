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


// This file describes the interface to some optics utility functions and the
// thin-layer iridescence texture generation function.

#ifndef O3D_COMMAND_BUFFER_SAMPLES_BUBBLE_IRIDESCENCE_TEXTURE_H_
#define O3D_COMMAND_BUFFER_SAMPLES_BUBBLE_IRIDESCENCE_TEXTURE_H_

namespace o3d {
namespace command_buffer {

// Wavelengths for different colors used in MakeIridescenceTexture (in
// nanometers).
const float kRedWavelength = 680.f;
const float kGreenWavelength = 530.f;
const float kBlueWavelength = 440.f;

// Fresnel coefficients (amplitude) for 2 polarisations of the incident light
// (either perpendicular or parallel to the plane of incidence).
struct FresnelCoefficients {
  float reflected_perp;
  float reflected_para;
  float transmitted_perp;
  float transmitted_para;
};

// Computes the cosine of the angle of the refracted ray with the interface
// normal.
// Parameters:
//   n: the index of refraction of the transmission medium relative to the
//     incidence one.
//   cos_i: the cosine of the angle of the incident ray with the interface
//     normal
// Returns:
//   cos_t, the cosine of the refracted angle.
float RefractedRay(float n, float cos_i);

// Computes the Fresnel coefficients (amplitude).
// Parameters:
//   n: the index of refraction of the transmission medium relative to the
//     incidence one.
//   cos_i: the cosine of the angle of the incident ray with the interface
//     normal
//   cos_t: the cosine of the angle of the transmitted ray with the interface
//     normal
// Returns:
//   The amplitude Fresnel coefficients.
FresnelCoefficients ComputeFresnel(float n, float cos_i, float cos_t);

// Computes a BGRA texture used for thin-layer iridescence simulation. It maps
// a wavelength-dependent reflected power and total transmitted power as a
// function of the cosine of the incidence angle (x coordinate from 0 to 1),
// and the thin-layer thickness (y coordinate from 0 to max_thickness) for a
// given refraction index.
// The B,G and R component receive the relative reflected power for the
// wavelengths corresponding to blue, green and red respectively.
// The A component receives the transmitted power (wavelength-independent).
// Parameters:
//   width: the width of the texture.
//   height: the height of the texture.
//   n: the refraction index of the thin-layer medium relative to the incident
//     medium.
//   max_thickness: the maximum thickness of the layer (in nanometers).
//   texture: the buffer receiving the iridescence texture. It should be large
//     enough to contain width*height*4 bytes.
void MakeIridescenceTexture(unsigned int width,
                            unsigned int height,
                            float n,
                            float max_thickness,
                            unsigned char *texture);

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SAMPLES_BUBBLE_IRIDESCENCE_TEXTURE_H_
