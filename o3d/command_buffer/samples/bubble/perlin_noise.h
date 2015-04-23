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


// This file contains the declaration of the Perlin noise generator.

#ifndef O3D_COMMAND_BUFFER_SAMPLES_BUBBLE_PERLIN_NOISE_H_
#define O3D_COMMAND_BUFFER_SAMPLES_BUBBLE_PERLIN_NOISE_H_

#include <utility>
#include "base/scoped_ptr.h"

namespace o3d {
namespace command_buffer {

// 2D Perlin perlin noise generator.
class PerlinNoise2D {
 public:
  typedef std::pair<float, float> Float2;
  explicit PerlinNoise2D(unsigned int frequency);

  // Initializes the permutation and gradients arrays. Note that all randomness
  // happens here.
  // Parameters:
  //   seed: pointer to the seed value for randomness. Note: this value is
  //     modified like it would in the rand_r function.
  void Initialize(unsigned int *seed);

  // Generates the noise texture.
  // Parameters:
  //   width: the width of the texture
  //   height: the height of the texture
  //   texture: the texture values, to be filled by the function. The buffer
  //     must have enough storage for width*height floats.
  void Generate(unsigned int width, unsigned int height, float *texture);

 private:
  unsigned int frequency_;
  scoped_array<unsigned int> permutation_;
  scoped_array<Float2> gradients_;
};

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SAMPLES_BUBBLE_PERLIN_NOISE_H_
