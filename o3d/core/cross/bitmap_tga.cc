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


// This file contains the image file codec operations for OpenGL texture
// loading.

// precompiled header must appear before anything else.
#include "core/cross/precompile.h"

#include <stdio.h>
#include "core/cross/bitmap.h"
#include "utils/cross/file_path_utils.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "import/cross/memory_buffer.h"
#include "import/cross/memory_stream.h"

using file_util::OpenFile;
using file_util::CloseFile;

namespace o3d {

// Loads the header information and raw RGB{A} data from an uncompressed
// 24-bit or 32-bit TGA stream into the Bitmap object.
bool Bitmap::LoadFromTGAStream(MemoryReadStream *stream,
                               const String &filename,
                               bool generate_mipmaps) {
  // Read the magic header.
  unsigned char file_magic[12];
  if (stream->Read(file_magic, sizeof(file_magic)) != sizeof(file_magic)) {
    DLOG(ERROR) << "Targa file magic not loaded \"" << filename << "\"";
    return false;
  }
  // Match the first few bytes of the TGA header to confirm we can read this
  // format. Multibyte values are stored little endian.
  const unsigned char kTargaMagic[12] = {
    0,     // ID Length (0 = no ID string present)
    0,     // Color Map Type ( 0 = no color map)
    2,     // Image Type (2 = Uncompressed True Color)
    0, 0,  // Color Map: First Entry Index (2 bytes)
    0, 0,  // Color Map: Table Length (2 bytes)
    0,     // Color Map: Entry Size
    0, 0,  // X-origin of image
    0, 0,  // Y-origin of image
           // MATCHED LATER: Image Width  (2 bytes)
           // MATCHED LATER: Image Height (2 bytes)
           // MATCHED LATER: Pixel Depth (1 byte)
           // MATCHED LATER: Image Descriptor (1 byte, alpha:4bit, origin:2bit)
  };
  if (memcmp(kTargaMagic, file_magic, sizeof(kTargaMagic)) != 0) {
    DLOG(ERROR) << "Targa file subtype not recognized \"" << filename << "\"";
    return false;
  }
  // Read the image header.
  unsigned char header[6];
  if (stream->Read(header, sizeof(header)) != sizeof(header)) {
    DLOG(ERROR) << "Targa file header not read \"" << filename << "\"";
    return false;
  }
  // Calculate image width and height, stored as little endian.
  unsigned int tga_width  = header[1] * 256 + header[0];
  unsigned int tga_height = header[3] * 256 + header[2];
  if (!CheckImageDimensions(tga_width, tga_height)) {
    DLOG(ERROR) << "Failed to load " << filename
                << ": dimensions are too large (" << tga_width
                << ", " << tga_height << ").";
    return false;
  }
  unsigned int components = header[4] >> 3;
  // NOTE: Image Descriptor byte is skipped.
  if (components != 3 && components != 4) {
    DLOG(ERROR) << "Targa file  \"" << filename
                << "\"has an unsupported number of components";
    return false;
  }
  // pixels contained in the file.
  unsigned int pixel_count = tga_width * tga_height;
  // Allocate storage for the pixels.
  unsigned int num_mipmaps =
      generate_mipmaps ? GetMipMapCount(tga_width, tga_height) : 1;
  Texture::Format format = components == 3 ? Texture::XRGB8 : Texture::ARGB8;
  unsigned int image_size = GetMipChainSize(tga_width, tga_height, format,
                                            num_mipmaps);
  scoped_array<unsigned char> image_data(new unsigned char[image_size]);
  if (image_data.get() == NULL) {
    DLOG(ERROR) << "Targa file memory allocation error \"" << filename << "\"";
    return false;
  }
  // Read in the bitmap data.
  size_t bytes_to_read = pixel_count * components;
  if (stream->Read(image_data.get(), bytes_to_read) != bytes_to_read) {
    DLOG(ERROR) << "Targa file read failed \"" << filename << "\"";
    return false;
  }

  if (components == 3) {
    // Fixup the image by inserting an alpha value of 1 (BGR->BGRX).
    XYZToXYZA(image_data.get(), pixel_count);
  }

  if (generate_mipmaps) {
    if (!GenerateMipmaps(tga_width, tga_height, format, num_mipmaps,
                         image_data.get())) {
      DLOG(ERROR) << "mip-map generation failed for \"" << filename << "\"";
      return false;
    }
  }

  // Success.
  image_data_.swap(image_data);
  width_ = tga_width;
  height_ = tga_height;
  format_ = format;
  num_mipmaps_ = num_mipmaps;
  return true;
}

}  // namespace o3d
