// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include <time.h>
#include <vector>

#include "skia/ext/convolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skia {

namespace {

// Fills the given filter with impulse functions for the range 0->num_entries.
void FillImpulseFilter(int num_entries, ConvolusionFilter1D* filter) {
  float one = 1.0f;
  for (int i = 0; i < num_entries; i++)
    filter->AddFilter(i, &one, 1);
}

// Filters the given input with the impulse function, and verifies that it
// does not change.
void TestImpulseConvolusion(const unsigned char* data, int width, int height) {
  int byte_count = width * height * 4;

  ConvolusionFilter1D filter_x;
  FillImpulseFilter(width, &filter_x);

  ConvolusionFilter1D filter_y;
  FillImpulseFilter(height, &filter_y);

  std::vector<unsigned char> output;
  output.resize(byte_count);
  BGRAConvolve2D(data, width * 4, true, filter_x, filter_y, &output[0]);

  // Output should exactly match input.
  EXPECT_EQ(0, memcmp(data, &output[0], byte_count));
}

// Fills the destination filter with a box filter averaging every two pixels
// to produce the output.
void FillBoxFilter(int size, ConvolusionFilter1D* filter) {
  const float box[2] = { 0.5, 0.5 };
  for (int i = 0; i < size; i++)
    filter->AddFilter(i * 2, box, 2);
}

}  // namespace

// Tests that each pixel, when set and run through the impulse filter, does
// not change.
TEST(Convolver, Impulse) {
  // We pick an "odd" size that is not likely to fit on any boundaries so that
  // we can see if all the widths and paddings are handled properly.
  int width = 15;
  int height = 31;
  int byte_count = width * height * 4;
  std::vector<unsigned char> input;
  input.resize(byte_count);

  unsigned char* input_ptr = &input[0];
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      for (int channel = 0; channel < 3; channel++) {
        memset(input_ptr, 0, byte_count);
        input_ptr[(y * width + x) * 4 + channel] = 0xff;
        // Always set the alpha channel or it will attempt to "fix" it for us.
        input_ptr[(y * width + x) * 4 + 3] = 0xff;
        TestImpulseConvolusion(input_ptr, width, height);
      }
    }
  }
}

// Tests that using a box filter to halve an image results in every square of 4
// pixels in the original get averaged to a pixel in the output.
TEST(Convolver, Halve) {
  static const int kSize = 16;

  int src_width = kSize;
  int src_height = kSize;
  int src_row_stride = src_width * 4;
  int src_byte_count = src_row_stride * src_height;
  std::vector<unsigned char> input;
  input.resize(src_byte_count);

  int dest_width = src_width / 2;
  int dest_height = src_height / 2;
  int dest_byte_count = dest_width * dest_height * 4;
  std::vector<unsigned char> output;
  output.resize(dest_byte_count);

  // First fill the array with a bunch of random data.
  srand(static_cast<unsigned>(time(NULL)));
  for (int i = 0; i < src_byte_count; i++)
    input[i] = rand() * 255 / RAND_MAX;

  // Compute the filters.
  ConvolusionFilter1D filter_x, filter_y;
  FillBoxFilter(dest_width, &filter_x);
  FillBoxFilter(dest_height, &filter_y);

  // Do the convolusion.
  BGRAConvolve2D(&input[0], src_width, true, filter_x, filter_y, &output[0]);

  // Compute the expected results and check, allowing for a small difference
  // to account for rounding errors.
  for (int y = 0; y < dest_height; y++) {
    for (int x = 0; x < dest_width; x++) {
      for (int channel = 0; channel < 4; channel++) {
        int src_offset = (y * 2 * src_row_stride + x * 2 * 4) + channel;
        int value = input[src_offset] +  // Top left source pixel.
                    input[src_offset + 4] +  // Top right source pixel.
                    input[src_offset + src_row_stride] +  // Lower left.
                    input[src_offset + src_row_stride + 4];  // Lower right.
        value /= 4;  // Average.
        int difference = value - output[(y * dest_width + x) * 4 + channel];
        EXPECT_TRUE(difference >= -1 || difference <= 1);
      }
    }
  }
}

}  // namespace skia

