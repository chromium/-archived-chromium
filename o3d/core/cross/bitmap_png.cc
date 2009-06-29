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


// This file contains the image codec operations for PNG files.

// precompiled header must appear before anything else.
#include "core/cross/precompile.h"

#include <fstream>
#include "core/cross/bitmap.h"
#include "core/cross/types.h"
#include "utils/cross/file_path_utils.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "import/cross/memory_buffer.h"
#include "import/cross/memory_stream.h"
#include "png.h"

using file_util::OpenFile;
using file_util::CloseFile;

namespace o3d {

// Helper function for LoadFromPNGFile that converts a stream into the
// necessary abstract byte reading function.
static void stream_read_data(png_structp png_ptr,
                             png_bytep data,
                             png_size_t length) {
  MemoryReadStream *stream =
    static_cast<MemoryReadStream*>(png_get_io_ptr(png_ptr));
  stream->Read(data, length);
}


// Loads the raw RGB data from a compressed PNG file.
bool Bitmap::LoadFromPNGStream(MemoryReadStream *stream,
                               const String &filename,
                               bool generate_mipmaps) {
  // Read the magic header.
  char magic[4];
  size_t bytes_read = stream->Read(magic, sizeof(magic));
  if (bytes_read != sizeof(magic)) {
    DLOG(ERROR) << "PNG file magic header not loaded \"" << filename << "\"";
    return false;
  }

  // Match the magic header to check that this is a PNG file.
  if (png_sig_cmp(reinterpret_cast<png_bytep>(magic), 0, sizeof(magic)) != 0) {
    DLOG(ERROR) << "File is not a PNG file \"" << filename << "\"";
    return false;
  }

  // Load the rest of the PNG file ----------------
  png_structp png_ptr = NULL;
  png_infop info_ptr = NULL;

  // create the PNG structure (not providing user error functions).
  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                   NULL,   // user_error_ptr
                                   NULL,   // user_error_fn
                                   NULL);  // user_warning_fn
  if (png_ptr == NULL)
    return 0;

  // Allocate memory for image information
  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL) {
    png_destroy_read_struct(&png_ptr, png_infopp_NULL, png_infopp_NULL);
    DLOG(ERROR) << "Cannot allocate working memory for PNG load.";
    return false;
  }

  // NOTE: The following smart pointer needs to be declared before the
  // setjmp so that it is properly destroyed if we jump back.
  scoped_array<unsigned char> image_data;
  png_bytepp row_pointers = NULL;

  // Set error handling if you are using the setjmp/longjmp method. If any
  // error happens in the following code, we will return here to handle the
  // error.
  if (setjmp(png_jmpbuf(png_ptr))) {
    // If we reach here, a fatal error occurred so free memory and exit.
    DLOG(ERROR) << "Fatal error reading PNG file \"" << filename << "\"";
    if (row_pointers)
      png_free(png_ptr, row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, png_infopp_NULL);
    return false;
  }

  // Set up our STL stream input control
  png_set_read_fn(png_ptr, stream, &stream_read_data);

  // We have already read some of the signature, advance the pointer.
  png_set_sig_bytes(png_ptr, sizeof(magic));

  // Read the PNG header information.
  png_uint_32 png_width = 0;
  png_uint_32 png_height = 0;
  int png_color_type = 0;
  int png_interlace_type = 0;
  int png_bits_per_channel = 0;
  png_read_info(png_ptr, info_ptr);
  png_get_IHDR(png_ptr,
               info_ptr,
               &png_width,
               &png_height,
               &png_bits_per_channel,
               &png_color_type,
               &png_interlace_type,
               NULL,
               NULL);

  if (!CheckImageDimensions(png_width, png_height)) {
    DLOG(ERROR) << "Failed to load " << filename
                << ": dimensions are too large (" << png_width
                << ", " << png_height << ").";
    // Use the png error system to clean up and exit.
    png_error(png_ptr, "PNG image too large");
  }

  // Extract multiple pixels with bit depths of 1, 2, and 4 from a single
  // byte into separate bytes (useful for paletted and grayscale images)
  //
  // png_set_packing(png_ptr);

  // Change the order of packed pixels to least significant bit first
  // (not useful if you are using png_set_packing)
  //
  // png_set_packswap(png_ptr);

  // Number of components in destination data (going to image_data).
  unsigned int dst_components = 0;
  Texture::Format format = Texture::UNKNOWN_FORMAT;
  // Palette vs non-palette.
  if (png_color_type == PNG_COLOR_TYPE_PALETTE) {
    // Expand paletted colors into RGB{A} triplets
    png_set_palette_to_rgb(png_ptr);
  // Gray vs RGB.
  } else if ((png_color_type & PNG_COLOR_MASK_COLOR) == PNG_COLOR_TYPE_RGB) {
    if (png_bits_per_channel != 8) {
      png_error(png_ptr, "PNG image type not recognized");
    }
  } else {
    if (png_bits_per_channel <= 1 ||
        png_bits_per_channel >= 8 ) {
      png_error(png_ptr, "PNG image type not recognized");
    }
    // Expand grayscale images to the full 8 bits from 2, or 4 bits/pixel
    // TODO(o3d): Do we want to expose L/A/LA texture formats ?
    png_set_gray_1_2_4_to_8(png_ptr);
    png_set_gray_to_rgb(png_ptr);
  }
  // Expand paletted or RGB images with transparency to full alpha channels
  // so the data will be available as RGBA quartets.
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
    png_set_tRNS_to_alpha(png_ptr);
    png_color_type |= PNG_COLOR_MASK_ALPHA;
  }
  // 24-bit RGB image or 32-bit RGBA image.
  if (png_color_type & PNG_COLOR_MASK_ALPHA) {
    format = Texture::ARGB8;
  } else {
    format = Texture::XRGB8;
    // Add alpha byte after each RGB triplet
    png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);
  }
  png_set_bgr(png_ptr);
  dst_components = 4;

  // Turn on interlace handling.  REQURIED if you are not using
  // png_read_image().  To see how to handle interlacing passes,
  // see the png_read_row() method below:
  int png_number_passes = png_set_interlace_handling(png_ptr);

  // Execute any setup steps for each Transform, i.e. to gamma correct and
  // add the background to the palette and update info structure.  REQUIRED
  // if you are expecting libpng to update the palette for you (ie you
  // selected such a transform above).
  png_read_update_info(png_ptr, info_ptr);

  // Allocate storage for the pixels.
  unsigned int num_mipmaps =
      generate_mipmaps ? GetMipMapCount(png_width, png_height) : 1;
  // Allocate storage for the pixels.
  unsigned int png_image_size = GetMipChainSize(png_width, png_height, format,
                                                num_mipmaps);
  image_data.reset(new unsigned char[png_image_size]);
  if (image_data.get() == NULL) {
    DLOG(ERROR) << "PNG image memory allocation error \"" << filename << "\"";
    png_error(png_ptr, "Cannot allocate memory for bitmap");
  }

  // Create an array of pointers to the beginning of each row. For some
  // hideous reason the PNG library requires this. At least we don't malloc
  // memory for each row as the examples do.
  row_pointers = static_cast<png_bytep *>(
      png_malloc(png_ptr, png_height * sizeof(png_bytep)));  // NOLINT
  if (row_pointers == NULL) {
    DLOG(ERROR) << "PNG row memory allocation error \"" << filename << "\"";
    png_error(png_ptr, "Cannot allocate memory for row pointers");
  }

  // Fill the row pointer array.
  DCHECK_LE(png_get_rowbytes(png_ptr, info_ptr), png_width * dst_components);
  // NOTE: we load images bottom to up to respect max/maya's UV
  // orientation.
  png_bytep row_ptr = reinterpret_cast<png_bytep>(image_data.get())
      + png_width * dst_components * (png_height - 1);
  for (unsigned int i = 0; i < png_height; ++i) {
    row_pointers[i] = row_ptr;
    row_ptr -= png_width * dst_components;
  }

  // Read the image, applying format transforms and de-interlacing as we go.
  png_read_image(png_ptr, row_pointers);

  // Do not reading any additional chunks using png_read_end()

  // Clean up after the read, and free any memory allocated - REQUIRED
  png_free(png_ptr, row_pointers);
  png_destroy_read_struct(&png_ptr, &info_ptr, png_infopp_NULL);

  if (generate_mipmaps) {
    if (!GenerateMipmaps(png_width, png_height, format, num_mipmaps,
                         image_data.get())) {
      DLOG(ERROR) << "Mip-map generation failed for \"" << filename << "\"";
      return false;
    }
  }

  // Success.
  image_data_.swap(image_data);
  format_ = format;
  width_ = png_width;
  height_ = png_height;
  num_mipmaps_ = num_mipmaps;
  return true;
}

// Saves the BGRA data from a compressed PNG file.
bool Bitmap::SaveToPNGFile(const char* filename) {
  if (format_ != Texture::ARGB8) {
    DLOG(ERROR) << "Can only save ARGB8 images.";
    return false;
  }
  if (num_mipmaps_ != 1 || is_cubemap_) {
    DLOG(ERROR) << "Only 2D images with only the base level can be saved.";
    return false;
  }
  FILE *fp = fopen(filename, "wb");
  if (!fp) {
    DLOG(ERROR) << "Could not open file " << filename << " for writing.";
    return false;
  }

  png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL,
                                                NULL, NULL);
  if (!png_ptr) {
    DLOG(ERROR) << "Could not create PNG structure.";
    fclose(fp);
    return false;
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    DLOG(ERROR) << "Could not create PNG info structure.";
    png_destroy_write_struct(&png_ptr,  png_infopp_NULL);
    fclose(fp);
    return false;
  }

  scoped_array<png_bytep> row_pointers(new png_bytep[height_]);
  for (unsigned int i = 0; i < height_; ++i) {
    row_pointers[height_-1-i] = image_data_.get() + i * width_ * 4;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    // If we get here, we had a problem reading the file.
    DLOG(ERROR) << "Error while writing file " << filename << ".";
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    return false;
  }

  png_init_io(png_ptr, fp);

  png_set_IHDR(png_ptr, info_ptr, width_, height_, 8,
               PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_set_bgr(png_ptr);
  png_set_rows(png_ptr, info_ptr, row_pointers.get());
  png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, png_voidp_NULL);

  png_destroy_write_struct(&png_ptr, &info_ptr);
  fclose(fp);
  return true;
}

}  // namespace o3d
