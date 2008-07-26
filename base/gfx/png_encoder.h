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

#ifndef BASE_GFX_PNG_ENCODER_H__
#define BASE_GFX_PNG_ENCODER_H__

#include <vector>

#include "base/basictypes.h"

// Interface for encoding PNG data. This is a wrapper around libpng,
// which has an inconvenient interface for callers. This is currently designed
// for use in tests only (where we control the files), so the handling isn't as
// robust as would be required for a browser (see Decode() for more).  WebKit
// has its own more complicated PNG decoder which handles, among other things,
// partially downloaded data.
class PNGEncoder {
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

  // Encodes the given raw 'input' data, with each pixel being represented as
  // given in 'format'. The encoded PNG data will be written into the supplied
  // vector and true will be returned on success. On failure (false), the
  // contents of the output buffer are undefined.
  //
  // When writing alpha values, the input colors are assumed to be post
  // multiplied.
  //
  // w, h: dimensions of the image
  // row_byte_width: the width in bytes of each row. This may be greater than
  //   w * bytes_per_pixel if there is extra padding at the end of each row
  //   (often, each row is padded to the next machine word).
  // discard_transparency: when true, and when the input data format includes
  //   alpha values, these alpha values will be discarded and only RGB will be
  //   written to the resulting file. Otherwise, alpha values in the input
  //   will be preserved.
  static bool Encode(const unsigned char* input, ColorFormat format,
                     int w, int h, int row_byte_width,
                     bool discard_transparency,
                     std::vector<unsigned char>* output);

 private:
  DISALLOW_EVIL_CONSTRUCTORS(PNGEncoder);
};

#endif  // BASE_GFX_PNG_ENCODER_H__
