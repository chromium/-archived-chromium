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

#ifndef BASE_GFX_PNG_DECODER_H__
#define BASE_GFX_PNG_DECODER_H__

#include <vector>

#include "base/basictypes.h"

class SkBitmap;

// Interface for decoding PNG data. This is a wrapper around libpng,
// which has an inconvenient interface for callers. This is currently designed
// for use in tests only (where we control the files), so the handling isn't as
// robust as would be required for a browser (see Decode() for more).  WebKit
// has its own more complicated PNG decoder which handles, among other things,
// partially downloaded data.
class PNGDecoder {
 public:
  enum ColorFormat {
    // 3 bytes per pixel (packed), in RGB order regardless of endianness.
    // This is the native JPEG format.
    FORMAT_RGB,

    // 4 bytes per pixel, in RGBA order in memory regardless of endianness.
    FORMAT_RGBA,

    // 4 bytes per pixel, in BGRA order in memory regardless of endianness.
    // This is the default Windows DIB order.
    FORMAT_BGRA
  };

  // Decodes the PNG data contained in input of length input_size. The
  // decoded data will be placed in *output with the dimensions in *w and *h
  // on success (returns true). This data will be written in the 'format'
  // format. On failure, the values of these output variables are undefined.
  //
  // This function may not support all PNG types, and it hasn't been tested
  // with a large number of images, so assume a new format may not work. It's
  // really designed to be able to read in something written by Encode() above.
  static bool Decode(const unsigned char* input, size_t input_size,
                     ColorFormat format, std::vector<unsigned char>* output,
                     int* w, int* h);

  // A convenience function for decoding PNGs as previously encoded by the PNG
  // encoder. Chrome encodes png in the format PNGDecoder::FORMAT_BGRA.
  //
  // Returns true if data is non-null and can be decoded as a png, false
  // otherwise.
  static bool Decode(const std::vector<unsigned char>* data, SkBitmap* icon);

  // Create a SkBitmap from a decoded BGRA DIB. The caller owns the returned
  // SkBitmap.
  static SkBitmap* CreateSkBitmapFromBGRAFormat(
      std::vector<unsigned char>& bgra, int width, int height);

 private:
  DISALLOW_EVIL_CONSTRUCTORS(PNGDecoder);
};

#endif  // BASE_GFX_PNG_DECODER_H__
