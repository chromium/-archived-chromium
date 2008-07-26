// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <math.h>

#include "base/gfx/png_encoder.h"
#include "base/gfx/png_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

static void MakeRGBImage(int w, int h, std::vector<unsigned char>* dat) {
  dat->resize(w * h * 3);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      unsigned char* org_px = &(*dat)[(y * w + x) * 3];
      org_px[0] = x * 3;      // r
      org_px[1] = x * 3 + 1;  // g
      org_px[2] = x * 3 + 2;  // b
    }
  }
}

// Set use_transparency to write data into the alpha channel, otherwise it will
// be filled with 0xff. With the alpha channel stripped, this should yield the
// same image as MakeRGBImage above, so the code below can make reference
// images for conversion testing.
static void MakeRGBAImage(int w, int h, bool use_transparency,
                          std::vector<unsigned char>* dat) {
  dat->resize(w * h * 4);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      unsigned char* org_px = &(*dat)[(y * w + x) * 4];
      org_px[0] = x * 3;      // r
      org_px[1] = x * 3 + 1;  // g
      org_px[2] = x * 3 + 2;  // b
      if (use_transparency)
        org_px[3] = x*3 + 3;  // a
      else
        org_px[3] = 0xFF;     // a (opaque)
    }
  }
}

TEST(PNGCodec, EncodeDecodeRGB) {
  const int w = 20, h = 20;

  // create an image with known values
  std::vector<unsigned char> original;
  MakeRGBImage(w, h, &original);

  // encode
  std::vector<unsigned char> encoded;
  EXPECT_TRUE(PNGEncoder::Encode(&original[0], PNGEncoder::FORMAT_RGB, w, h,
                               w * 3, false, &encoded));

  // decode, it should have the same size as the original
  std::vector<unsigned char> decoded;
  int outw, outh;
  EXPECT_TRUE(PNGDecoder::Decode(&encoded[0], encoded.size(),
                               PNGDecoder::FORMAT_RGB, &decoded,
                               &outw, &outh));
  ASSERT_EQ(w, outw);
  ASSERT_EQ(h, outh);
  ASSERT_EQ(original.size(), decoded.size());

  // Images must be equal
  ASSERT_TRUE(original == decoded);
}

TEST(PNGCodec, EncodeDecodeRGBA) {
  const int w = 20, h = 20;

  // create an image with known values, a must be opaque because it will be
  // lost during encoding
  std::vector<unsigned char> original;
  MakeRGBAImage(w, h, true, &original);

  // encode
  std::vector<unsigned char> encoded;
  EXPECT_TRUE(PNGEncoder::Encode(&original[0], PNGEncoder::FORMAT_RGBA, w, h,
                               w * 4, false, &encoded));

  // decode, it should have the same size as the original
  std::vector<unsigned char> decoded;
  int outw, outh;
  EXPECT_TRUE(PNGDecoder::Decode(&encoded[0], encoded.size(),
                               PNGDecoder::FORMAT_RGBA, &decoded,
                               &outw, &outh));
  ASSERT_EQ(w, outw);
  ASSERT_EQ(h, outh);
  ASSERT_EQ(original.size(), decoded.size());

  // Images must be exactly equal
  ASSERT_TRUE(original == decoded);
}

// Test that corrupted data decompression causes failures.
TEST(PNGCodec, DecodeCorrupted) {
  int w = 20, h = 20;

  // Make some random data (an uncompressed image).
  std::vector<unsigned char> original;
  MakeRGBImage(w, h, &original);

  // It should fail when given non-JPEG compressed data.
  std::vector<unsigned char> output;
  int outw, outh;
  EXPECT_FALSE(PNGDecoder::Decode(&original[0], original.size(),
                                PNGDecoder::FORMAT_RGB, &output,
                                &outw, &outh));

  // Make some compressed data.
  std::vector<unsigned char> compressed;
  EXPECT_TRUE(PNGEncoder::Encode(&original[0], PNGEncoder::FORMAT_RGB, w, h,
                               w * 3, false, &compressed));

  // Try decompressing a truncated version.
  EXPECT_FALSE(PNGDecoder::Decode(&compressed[0], compressed.size() / 2,
                                PNGDecoder::FORMAT_RGB, &output,
                                &outw, &outh));

  // Corrupt it and try decompressing that.
  for (int i = 10; i < 30; i++)
    compressed[i] = i;
  EXPECT_FALSE(PNGDecoder::Decode(&compressed[0], compressed.size(),
                                PNGDecoder::FORMAT_RGB, &output,
                                &outw, &outh));
}

TEST(PNGCodec, EncodeDecodeBGRA) {
  const int w = 20, h = 20;

  // Create an image with known values, alpha must be opaque because it will be
  // lost during encoding.
  std::vector<unsigned char> original;
  MakeRGBAImage(w, h, true, &original);

  // Encode.
  std::vector<unsigned char> encoded;
  EXPECT_TRUE(PNGEncoder::Encode(&original[0], PNGEncoder::FORMAT_BGRA, w, h,
                               w * 4, false, &encoded));

  // Decode, it should have the same size as the original.
  std::vector<unsigned char> decoded;
  int outw, outh;
  EXPECT_TRUE(PNGDecoder::Decode(&encoded[0], encoded.size(),
                               PNGDecoder::FORMAT_BGRA, &decoded,
                               &outw, &outh));
  ASSERT_EQ(w, outw);
  ASSERT_EQ(h, outh);
  ASSERT_EQ(original.size(), decoded.size());

  // Images must be exactly equal.
  ASSERT_TRUE(original == decoded);
}

TEST(PNGCodec, StripAddAlpha) {
  const int w = 20, h = 20;

  // These should be the same except one has a 0xff alpha channel.
  std::vector<unsigned char> original_rgb;
  MakeRGBImage(w, h, &original_rgb);
  std::vector<unsigned char> original_rgba;
  MakeRGBAImage(w, h, false, &original_rgba);

  // Encode RGBA data as RGB.
  std::vector<unsigned char> encoded;
  EXPECT_TRUE(PNGEncoder::Encode(&original_rgba[0], PNGEncoder::FORMAT_RGBA, w, h,
                               w * 4, true, &encoded));

  // Decode the RGB to RGBA.
  std::vector<unsigned char> decoded;
  int outw, outh;
  EXPECT_TRUE(PNGDecoder::Decode(&encoded[0], encoded.size(),
                               PNGDecoder::FORMAT_RGBA, &decoded,
                               &outw, &outh));

  // Decoded and reference should be the same (opaque alpha).
  ASSERT_EQ(w, outw);
  ASSERT_EQ(h, outh);
  ASSERT_EQ(original_rgba.size(), decoded.size());
  ASSERT_TRUE(original_rgba == decoded);

  // Encode RGBA to RGBA.
  EXPECT_TRUE(PNGEncoder::Encode(&original_rgba[0], PNGEncoder::FORMAT_RGBA, w, h,
                               w * 4, false, &encoded));

  // Decode the RGBA to RGB.
  EXPECT_TRUE(PNGDecoder::Decode(&encoded[0], encoded.size(),
                               PNGDecoder::FORMAT_RGB, &decoded,
                               &outw, &outh));

  // It should be the same as our non-alpha-channel reference.
  ASSERT_EQ(w, outw);
  ASSERT_EQ(h, outh);
  ASSERT_EQ(original_rgb.size(), decoded.size());
  ASSERT_TRUE(original_rgb == decoded);
}
