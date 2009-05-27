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


// This file contains the implementation of the Perlin noise generator.

#include <stdlib.h>
#include "command_buffer/common/cross/logging.h"
#include "command_buffer/samples/bubble/perlin_noise.h"
#include "command_buffer/samples/bubble/utils.h"

namespace o3d {
namespace command_buffer {

PerlinNoise2D::PerlinNoise2D(unsigned int frequency)
    : frequency_(frequency),
      permutation_(new unsigned int[2*frequency]),
      gradients_(new Float2[frequency]) {
}

void PerlinNoise2D::Initialize(unsigned int *seed) {
  // Initialize the gradients table with a random unit direction.
  // Initialize the permutation table to be the identity.
  for (unsigned int i = 0; i < frequency_; ++i) {
    float theta = Randf(0.f, 2.f * kPi, seed);
    gradients_[i] = Float2(cosf(theta), sinf(theta));
    permutation_[i] = i;
  }
  // Generate the permutation table by switching each element with a further
  // element. Also duplicate the permutation table so that constructs like
  // permutation[x + permutation[y]] work without additional modulo.
  for (unsigned int i = 0; i < frequency_; ++i) {
    unsigned int j = i + (rand_r(seed) % (frequency_ - i));
    unsigned int tmp = permutation_[j];
    permutation_[j] = permutation_[i];
    permutation_[i] = tmp;
    permutation_[i + frequency_] = tmp;
  }
}

void PerlinNoise2D::Generate(unsigned int width,
                             unsigned int height,
                             float *texture) {
  for (unsigned int y = 0; y < height; ++y) {
    for (unsigned int x = 0; x < width; ++x) {
      // The texture is decomposed into a lattice of frequency_ points in each
      // direction. A (x, y) point falls between 4 lattice vertices.
      // (xx, yy) are the coordinate of the bottom left lattice vertex
      // corresponding to an (x, y) image point.
      unsigned int xx = x * frequency_ / width;
      unsigned int yy = y * frequency_ / height;
      // xt and yt are the barycentric coordinates of the (x, y) point relative
      // to the vertices (between 0 and 1).
      float xt = 1.f/width * ((x * frequency_) % width);
      float yt = 1.f/height * ((y * frequency_) % height);
      // contribution of each lattice vertex to the point value.
      float contrib[4];
      for (unsigned int y_offset = 0; y_offset < 2; ++y_offset) {
        for (unsigned int x_offset = 0; x_offset < 2; ++x_offset) {
          unsigned int index = permutation_[xx + x_offset] + y_offset + yy;
          DCHECK_LT(index, frequency_ * 2);
          Float2 gradient = gradients_[permutation_[index]];
          contrib[y_offset*2+x_offset] = gradient.first * (xt - x_offset) +
                                         gradient.second * (yt - y_offset);
        }
      }
      // We interpolate between the vertex contributions using a smooth step
      // function of the barycentric coordinates.
      float xt_smooth = SmoothStep(xt);
      float yt_smooth = SmoothStep(yt);
      float contrib_bottom = Lerp(xt_smooth, contrib[0], contrib[1]);
      float contrib_top = Lerp(xt_smooth, contrib[2], contrib[3]);
      *texture++ = Lerp(yt_smooth, contrib_bottom, contrib_top);
    }
  }
}

}  // namespace command_buffer
}  // namespace o3d
