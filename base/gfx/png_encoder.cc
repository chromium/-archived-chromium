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

#include "base/basictypes.h"
#include "base/gfx/png_encoder.h"
#include "base/logging.h"

extern "C" {
#include "png.h"
}

namespace {

// Converts BGRA->RGBA and RGBA->BGRA.
void ConvertBetweenBGRAandRGBA(const unsigned char* input, int pixel_width,
                               unsigned char* output) {
  for (int x = 0; x < pixel_width; x++) {
    const unsigned char* pixel_in = &input[x * 4];
    unsigned char* pixel_out = &output[x * 4];
    pixel_out[0] = pixel_in[2];
    pixel_out[1] = pixel_in[1];
    pixel_out[2] = pixel_in[0];
    pixel_out[3] = pixel_in[3];
  }
}

void ConvertRGBAtoRGB(const unsigned char* rgba, int pixel_width,
                      unsigned char* rgb) {
  for (int x = 0; x < pixel_width; x++) {
    const unsigned char* pixel_in = &rgba[x * 4];
    unsigned char* pixel_out = &rgb[x * 3];
    pixel_out[0] = pixel_in[0];
    pixel_out[1] = pixel_in[1];
    pixel_out[2] = pixel_in[2];
  }
}

}  // namespace

// Encoder --------------------------------------------------------------------
//
// This section of the code is based on nsPNGEncoder.cpp in Mozilla
// (Copyright 2005 Google Inc.)

namespace {

// Passed around as the io_ptr in the png structs so our callbacks know where
// to write data.
struct PngEncoderState {
  PngEncoderState(std::vector<unsigned char>* o) : out(o) {}
  std::vector<unsigned char>* out;
};

// Called by libpng to flush its internal buffer to ours.
void EncoderWriteCallback(png_structp png, png_bytep data, png_size_t size) {
  PngEncoderState* state = static_cast<PngEncoderState*>(png_get_io_ptr(png));
  DCHECK(state->out);

  size_t old_size = state->out->size();
  state->out->resize(old_size + size);
  memcpy(&(*state->out)[old_size], data, size);
}

void ConvertBGRAtoRGB(const unsigned char* bgra, int pixel_width,
                      unsigned char* rgb) {
  for (int x = 0; x < pixel_width; x++) {
    const unsigned char* pixel_in = &bgra[x * 4];
    unsigned char* pixel_out = &rgb[x * 3];
    pixel_out[0] = pixel_in[2];
    pixel_out[1] = pixel_in[1];
    pixel_out[2] = pixel_in[0];
  }
}

// Automatically destroys the given write structs on destruction to make
// cleanup and error handling code cleaner.
class PngWriteStructDestroyer {
 public:
  PngWriteStructDestroyer(png_struct** ps, png_info** pi) : ps_(ps), pi_(pi) {
  }
  ~PngWriteStructDestroyer() {
    png_destroy_write_struct(ps_, pi_);
  }
 private:
  png_struct** ps_;
  png_info** pi_;

  DISALLOW_EVIL_CONSTRUCTORS(PngWriteStructDestroyer);
};

}  // namespace

// static
bool PNGEncoder::Encode(const unsigned char* input, ColorFormat format,
                      int w, int h, int row_byte_width,
                      bool discard_transparency,
                      std::vector<unsigned char>* output) {
  // Run to convert an input row into the output row format, NULL means no
  // conversion is necessary.
  void (*converter)(const unsigned char* in, int w, unsigned char* out) = NULL;

  int input_color_components, output_color_components;
  int png_output_color_type;
  switch (format) {
    case FORMAT_RGB:
      input_color_components = 3;
      output_color_components = 3;
      png_output_color_type = PNG_COLOR_TYPE_RGB;
      discard_transparency = false;
      break;

    case FORMAT_RGBA:
      input_color_components = 4;
      if (discard_transparency) {
        output_color_components = 3;
        png_output_color_type = PNG_COLOR_TYPE_RGB;
        converter = ConvertRGBAtoRGB;
      } else {
        output_color_components = 4;
        png_output_color_type = PNG_COLOR_TYPE_RGB_ALPHA;
        converter = NULL;
      }
      break;

    case FORMAT_BGRA:
      input_color_components = 4;
      if (discard_transparency) {
        output_color_components = 3;
        png_output_color_type = PNG_COLOR_TYPE_RGB;
        converter = ConvertBGRAtoRGB;
      } else {
        output_color_components = 4;
        png_output_color_type = PNG_COLOR_TYPE_RGB_ALPHA;
        converter = ConvertBetweenBGRAandRGBA;
      }
      break;

    default:
      NOTREACHED() << "Unknown pixel format";
      return false;
  }

  // Row stride should be at least as long as the length of the data.
  DCHECK(input_color_components * w <= row_byte_width);

  png_struct* png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                                png_voidp_NULL,
                                                png_error_ptr_NULL,
                                                png_error_ptr_NULL);
  if (!png_ptr)
    return false;
  png_info* info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_write_struct(&png_ptr, NULL);
    return false;
  }
  PngWriteStructDestroyer destroyer(&png_ptr, &info_ptr);

  if (setjmp(png_jmpbuf(png_ptr))) {
    // The destroyer will ensure that the structures are cleaned up in this
    // case, even though we may get here as a jump from random parts of the
    // PNG library called below.
    return false;
  }

  // Set our callback for libpng to give us the data.
  PngEncoderState state(output);
  png_set_write_fn(png_ptr, &state, EncoderWriteCallback, NULL);

  png_set_IHDR(png_ptr, info_ptr, w, h, 8, png_output_color_type,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png_ptr, info_ptr);

  if (!converter) {
    // No conversion needed, give the data directly to libpng.
    for (int y = 0; y < h; y ++)
      png_write_row(png_ptr, const_cast<unsigned char*>(&input[y * row_byte_width]));
  } else {
    // Needs conversion using a separate buffer.
    unsigned char* row = new unsigned char[w * output_color_components];
    for (int y = 0; y < h; y ++) {
      converter(&input[y * row_byte_width], w, row);
      png_write_row(png_ptr, row);
    }
    delete[] row;
  }

  png_write_end(png_ptr, info_ptr);
  return true;
}
