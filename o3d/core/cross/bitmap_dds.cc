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


// This file contains the image codec operations for DDS files.

// precompiled header must appear before anything else.
#include "core/cross/precompile.h"

#include <stdio.h>
#include "core/cross/bitmap.h"
#include "core/cross/ddsurfacedesc.h"
#include "utils/cross/file_path_utils.h"
#include "base/file_util.h"
#include "import/cross/memory_buffer.h"
#include "import/cross/memory_stream.h"

using file_util::OpenFile;
using file_util::CloseFile;

namespace o3d {

// LoadFromDDSFile -------------------------------------------------------------

// A function that flips a DXTC block.
typedef void (* FlipBlockFunction)(unsigned char *block);

// Flips a full DXT1 block in the y direction.
static void FlipDXT1BlockFull(unsigned char *block) {
  // A DXT1 block layout is:
  // [0-1] color0.
  // [2-3] color1.
  // [4-7] color bitmap, 2 bits per pixel.
  // So each of the 4-7 bytes represents one line, flipping a block is just
  // flipping those bytes.
  unsigned char tmp = block[4];
  block[4] = block[7];
  block[7] = tmp;
  tmp = block[5];
  block[5] = block[6];
  block[6] = block[5];
}

// Flips the first 2 lines of a DXT1 block in the y direction.
static void FlipDXT1BlockHalf(unsigned char *block) {
  // See layout above.
  unsigned char tmp = block[4];
  block[4] = block[5];
  block[5] = tmp;
}

// Flips a full DXT3 block in the y direction.
static void FlipDXT3BlockFull(unsigned char *block) {
  // A DXT3 block layout is:
  // [0-7]  alpha bitmap, 4 bits per pixel.
  // [8-15] a DXT1 block.

  // We can flip the alpha bits at the byte level (2 bytes per line).
  unsigned char tmp = block[0];
  block[0] = block[6];
  block[6] = tmp;
  tmp = block[1];
  block[1] = block[7];
  block[7] = tmp;
  tmp = block[2];
  block[2] = block[4];
  block[4] = tmp;
  tmp = block[3];
  block[3] = block[5];
  block[5] = tmp;

  // And flip the DXT1 block using the above function.
  FlipDXT1BlockFull(block + 8);
}

// Flips the first 2 lines of a DXT3 block in the y direction.
static void FlipDXT3BlockHalf(unsigned char *block) {
  // See layout above.
  unsigned char tmp = block[0];
  block[0] = block[2];
  block[2] = tmp;
  tmp = block[1];
  block[1] = block[3];
  block[3] = tmp;
  FlipDXT1BlockHalf(block + 8);
}

// Flips a full DXT5 block in the y direction.
static void FlipDXT5BlockFull(unsigned char *block) {
  // A DXT5 block layout is:
  // [0]    alpha0.
  // [1]    alpha1.
  // [2-7]  alpha bitmap, 3 bits per pixel.
  // [8-15] a DXT1 block.

  // The alpha bitmap doesn't easily map lines to bytes, so we have to
  // interpret it correctly.  Extracted from
  // http://www.opengl.org/registry/specs/EXT/texture_compression_s3tc.txt :
  //
  //   The 6 "bits" bytes of the block are decoded into one 48-bit integer:
  //
  //     bits = bits_0 + 256 * (bits_1 + 256 * (bits_2 + 256 * (bits_3 +
  //                   256 * (bits_4 + 256 * bits_5))))
  //
  //   bits is a 48-bit unsigned integer, from which a three-bit control code
  //   is extracted for a texel at location (x,y) in the block using:
  //
  //       code(x,y) = bits[3*(4*y+x)+1..3*(4*y+x)+0]
  //
  //   where bit 47 is the most significant and bit 0 is the least
  //   significant bit.
  unsigned int line_0_1 = block[2] + 256 * (block[3] + 256 * block[4]);
  unsigned int line_2_3 = block[5] + 256 * (block[6] + 256 * block[7]);
  // swap lines 0 and 1 in line_0_1.
  unsigned int line_1_0 = ((line_0_1 & 0x000fff) << 12) |
      ((line_0_1 & 0xfff000) >> 12);
  // swap lines 2 and 3 in line_2_3.
  unsigned int line_3_2 = ((line_2_3 & 0x000fff) << 12) |
      ((line_2_3 & 0xfff000) >> 12);
  block[2] = line_3_2 & 0xff;
  block[3] = (line_3_2 & 0xff00) >> 8;
  block[4] = (line_3_2 & 0xff0000) >> 8;
  block[5] = line_1_0 & 0xff;
  block[6] = (line_1_0 & 0xff00) >> 8;
  block[7] = (line_1_0 & 0xff0000) >> 8;

  // And flip the DXT1 block using the above function.
  FlipDXT1BlockFull(block + 8);
}

// Flips the first 2 lines of a DXT5 block in the y direction.
static void FlipDXT5BlockHalf(unsigned char *block) {
  // See layout above.
  unsigned int line_0_1 = block[2] + 256 * (block[3] + 256 * block[4]);
  unsigned int line_1_0 = ((line_0_1 & 0x000fff) << 12) |
      ((line_0_1 & 0xfff000) >> 12);
  block[2] = line_1_0 & 0xff;
  block[3] = (line_1_0 & 0xff00) >> 8;
  block[4] = (line_1_0 & 0xff0000) >> 8;
  FlipDXT1BlockHalf(block + 8);
}

// Flips a DXTC image, by flipping and swapping DXTC blocks as appropriate.
static void FlipDXTCImage(unsigned int width,
                          unsigned int height,
                          unsigned int levels,
                          Texture::Format format,
                          unsigned char *data) {
  DCHECK(Bitmap::CheckImageDimensions(width, height));
  // Height must be a power-of-two.
  DCHECK_EQ(height & (height - 1), 0);
  FlipBlockFunction full_block_function = NULL;
  FlipBlockFunction half_block_function = NULL;
  unsigned int block_bytes = 0;
  switch (format) {
    case Texture::DXT1:
      full_block_function = FlipDXT1BlockFull;
      half_block_function = FlipDXT1BlockHalf;
      block_bytes = 8;
      break;
    case Texture::DXT3:
      full_block_function = FlipDXT3BlockFull;
      half_block_function = FlipDXT3BlockHalf;
      block_bytes = 16;
      break;
    case Texture::DXT5:
      full_block_function = FlipDXT5BlockFull;
      half_block_function = FlipDXT5BlockHalf;
      block_bytes = 16;
      break;
    default:
      DLOG(FATAL) << "Not Reached";
      return;
  }
  unsigned int mip_width = width;
  unsigned int mip_height = height;
  for (unsigned int i = 0; i < levels; ++i) {
    unsigned int blocks_per_row = (mip_width + 3) / 4;
    unsigned int blocks_per_col = (mip_height + 3) / 4;
    unsigned int blocks = blocks_per_row * blocks_per_col;
    if (mip_height == 1) {
      // no flip to do, and we're done.
      break;
    } else if (mip_height == 2) {
      // flip the first 2 lines in each block.
      for (unsigned int i = 0; i < blocks_per_row; ++i) {
        half_block_function(data + i * block_bytes);
      }
    } else {
      // flip each block.
      for (unsigned int i = 0; i < blocks; ++i) {
        full_block_function(data + i * block_bytes);
      }
      // swap each block line in the first half of the image with the
      // corresponding one in the second half.
      // note that this is a no-op if mip_height is 4.
      unsigned int row_bytes = block_bytes * blocks_per_row;
      scoped_array<unsigned char> temp_line(new unsigned char[row_bytes]);
      for (unsigned int y = 0; y < blocks_per_col / 2; ++y) {
        unsigned char *line1 = data + y * row_bytes;
        unsigned char *line2 = data + (blocks_per_col - y - 1) * row_bytes;
        memcpy(temp_line.get(), line1, row_bytes);
        memcpy(line1, line2, row_bytes);
        memcpy(line2, temp_line.get(), row_bytes);
      }
    }
    // mip levels are contiguous.
    data += block_bytes * blocks;
    mip_width = std::max(1U, mip_width >> 1);
    mip_height = std::max(1U, mip_height >> 1);
  }
}

// Flips a BGRA image, by simply swapping pixel rows.
static void FlipBGRAImage(unsigned int width,
                          unsigned int height,
                          unsigned int levels,
                          Texture::Format format,
                          unsigned char *data) {
  DCHECK(Bitmap::CheckImageDimensions(width, height));
  DCHECK(format == Texture::XRGB8 || format == Texture::ARGB8);
  unsigned int pixel_bytes = 4;
  unsigned int mip_width = width;
  unsigned int mip_height = height;
  // rows are at most as big as the first one.
  scoped_array<unsigned char> temp_line(
      new unsigned char[mip_width * pixel_bytes]);
  for (unsigned int i = 0; i < levels; ++i) {
    unsigned int row_bytes = pixel_bytes * mip_width;
    for (unsigned int y = 0; y < mip_height / 2; ++y) {
      unsigned char *line1 = data + y * row_bytes;
      unsigned char *line2 = data + (mip_height - y - 1) * row_bytes;
      memcpy(temp_line.get(), line1, row_bytes);
      memcpy(line1, line2, row_bytes);
      memcpy(line2, temp_line.get(), row_bytes);
    }
    // mip levels are contiguous.
    data += row_bytes * mip_height;
    mip_width = std::max(1U, mip_width >> 1);
    mip_height = std::max(1U, mip_height >> 1);
  }
}

// Load the bitmap data as DXTC compressed data from a DDS stream into the
// Bitmap object. This routine only supports compressed DDS formats DXT1,
// DXT3 and DXT5.
bool Bitmap::LoadFromDDSStream(MemoryReadStream *stream,
                               const String &filename,
                               bool generate_mipmaps) {
  // Verify the file is a true .dds file
  char magic[4];
  size_t bytes_read = stream->Read(magic, sizeof(magic));
  if (bytes_read != sizeof(magic)) {
    DLOG(ERROR) << "DDS magic header not read \"" << filename << "\"";
    return false;
  }
  if (std::strncmp(magic, "DDS ", 4) != 0) {
    DLOG(ERROR) << "DDS magic header not rcognised \"" << filename << "\"";
    return false;
  }
  // Get the DirectDraw Surface Descriptor
  DDSURFACEDESC2 dd_surface_descriptor;
  if (!stream->ReadAs<DDSURFACEDESC2>(&dd_surface_descriptor)) {
    DLOG(ERROR) << "DDS header not read \"" << filename << "\"";
    return false;
  }
  const unsigned int kRequiredFlags =
      DDSD_CAPS |
      DDSD_HEIGHT |
      DDSD_WIDTH |
      DDSD_PIXELFORMAT;
  if ((dd_surface_descriptor.dwFlags & kRequiredFlags) != kRequiredFlags) {
    DLOG(ERROR) << "Required DDS flags are absent in \"" << filename << "\".";
    return false;
  }
  // NOTE: Add permissible flags as appropriate here when supporting new
  // formats.
  const unsigned int kValidFlags = kRequiredFlags |
      DDSD_MIPMAPCOUNT |
      DDSD_LINEARSIZE;
  if (dd_surface_descriptor.dwFlags & ~kValidFlags) {
    DLOG(ERROR) << "Invalid DDS flags combination \"" << filename << "\".";
    return false;
  }
  unsigned int mip_count = (dd_surface_descriptor.dwFlags & DDSD_MIPMAPCOUNT) ?
      dd_surface_descriptor.dwMipMapCount : 1;
  unsigned int dds_width = dd_surface_descriptor.dwWidth;
  unsigned int dds_height = dd_surface_descriptor.dwHeight;
  if (!CheckImageDimensions(dds_width, dds_height)) {
    DLOG(ERROR) << "Failed to load " << filename
                << ": dimensions are too large (" << dds_width
                << ", " << dds_height << ").";
    return false;
  }
  // Check for cube maps
  bool is_cubemap =
      (dd_surface_descriptor.ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP) != 0;
  // Cube maps should have all the face flags set - otherwise the cube map is
  // incomplete.
  if (is_cubemap) {
    if ((dd_surface_descriptor.ddsCaps.dwCaps2 & DDSCAPS2_CUBEMAP_ALLFACES) !=
        DDSCAPS2_CUBEMAP_ALLFACES) {
      DLOG(ERROR) << "DDS file \"" << filename
                  << "\" is a cube map but doesn't have all the faces.";
      return false;
    }
    if (dds_width != dds_height) {
      DLOG(ERROR) << "DDS file \"" << filename
                  << "\" is a cube map but doesn't have square dimensions.";
      return false;
    }
  }

  // The size of the buffer needed to hold four-component per pixel
  // image data, including MIPMaps
  int components_per_pixel = 0;
  bool add_filler_alpha = false;
  bool rgb_to_bgr = false;

  Texture::Format format = Texture::UNKNOWN_FORMAT;
  bool is_dxtc = false;

  DDPIXELFORMAT &pixel_format = dd_surface_descriptor.ddpfPixelFormat;

  if (pixel_format.dwFlags & DDPF_FOURCC) {
    switch (pixel_format.dwFourCC) {
      case FOURCC_DXT1 : {
        format = Texture::DXT1;
        is_dxtc = true;
        break;
      }
      case FOURCC_DXT3 : {
        format = Texture::DXT3;
        is_dxtc = true;
        break;
      }
      case FOURCC_DXT5 : {
        format = Texture::DXT5;
        is_dxtc = true;
        break;
      }
      default : {
        DLOG(ERROR) << "DDS format not DXT1, DXT3 or DXT5. \""
                    << filename << "\"";
        return false;
      }
    }

    // Check that the advertised size is correct.
    if (dd_surface_descriptor.dwFlags & DDSD_LINEARSIZE) {
      unsigned int expected_size = GetBufferSize(dds_width, dds_height, format);
      if (expected_size != dd_surface_descriptor.dwLinearSize) {
        DLOG(ERROR) << "Advertised buffer size in \"" << filename
                    << "\" differs from expected size.";
        return false;
      }
    }
    if (is_dxtc) {
      // DirectX says the only valid DXT format base sizes are multiple-of-4.
      // OpenGL doesn't care, but we actually do because we need to flip them.
      // (and we can't flip them if they are not multiple-of-4).
      // This restriction actually exists for mip-map levels as well, so in
      // practice we need power-of-two restriction.
      if ((dds_width & (dds_width - 1)) != 0 ||
          (dds_height & (dds_height - 1)) != 0) {
        DLOG(ERROR) << "Invalid dimensions in DXTC file \""
                    << filename << "\": must be power-of-two.";
        return false;
      }
    }
  } else if (pixel_format.dwFlags & DDPF_RGB) {
    if (pixel_format.dwFlags & DDPF_ALPHAPIXELS) {
      // Pixel format with alpha. Check that the alpha bits are at the expected
      // place.
      if (pixel_format.dwRGBAlphaBitMask != 0xff000000) {
        DLOG(ERROR) << "unexpected alpha mask in DDS image format \""
                    << filename << "\"";
        return false;
      }
    } else {
      add_filler_alpha = true;
    }
    // uncompressed bitmap
    // try to determine the format
    if (pixel_format.dwRBitMask == 0x00ff0000 &&
        pixel_format.dwGBitMask == 0x0000ff00 &&
        pixel_format.dwBBitMask == 0x000000ff) {
      // BGR(A) format.
    } else if (pixel_format.dwRBitMask == 0x000000ff &&
               pixel_format.dwGBitMask == 0x0000ff00 &&
               pixel_format.dwBBitMask == 0x00ff0000) {
      // RGB(A) format. Convert to BGR(A).
      rgb_to_bgr = true;
    } else {
      DLOG(ERROR) << "unknown uncompressed DDS image format \""
                  << filename << "\"";
      return false;
    }
    // components per pixel in the file.
    components_per_pixel = add_filler_alpha ? 3 : 4;
    if (components_per_pixel * 8 != pixel_format.dwRGBBitCount) {
      DLOG(ERROR) << "unexpected bit count in DDS image format \""
                  << filename << "\"";
      return false;
    }
    format = add_filler_alpha ? Texture::XRGB8 : Texture::ARGB8;
  }

  if (is_dxtc && generate_mipmaps) {
    DLOG(WARNING) << "Disabling mip-map generation for DXTC image.";
    generate_mipmaps = false;
  }

  // compute buffer size
  unsigned int num_faces = is_cubemap ? 6 : 1;
  // power-of-two dimensions.
  unsigned int final_mip_count =
      generate_mipmaps ? GetMipMapCount(dds_width, dds_height) : mip_count;
  unsigned int face_size = GetMipChainSize(dds_width, dds_height, format,
                                           final_mip_count);
  unsigned int buffer_size = num_faces * face_size;


  // Allocate and load bitmap data.
  scoped_array<unsigned char> image_data(new unsigned char[buffer_size]);

  unsigned int disk_face_size = GetMipChainSize(dds_width, dds_height, format,
                                                mip_count);
  if (!is_dxtc) {
    // if reading uncompressed RGB, for example, we shouldn't read alpha channel
    // NOTE: here we assume that RGB data is packed - it may not be true
    // for non-multiple-of-4 widths.
    disk_face_size = components_per_pixel * disk_face_size / 4;
  }

  for (unsigned int face = 0; face < num_faces; ++face) {
    char *data = reinterpret_cast<char*>(image_data.get()) + face_size * face;
    bytes_read = stream->Read(data, disk_face_size);
    if (bytes_read != disk_face_size) {
      DLOG(ERROR) << "DDS failed to read image data \"" << filename << "\"";
      return false;
    }
  }

  // Do pixel conversions on non-DXT images.
  if (!is_dxtc) {
    DCHECK(components_per_pixel == 3 || components_per_pixel == 4);
    unsigned int pixel_count = disk_face_size / components_per_pixel;
    for (unsigned int face = 0; face < num_faces; ++face) {
      unsigned char *data = image_data.get() + face_size * face;
      // convert to four components per pixel if necessary
      if (add_filler_alpha) {
        DCHECK_EQ(components_per_pixel, 3);
        XYZToXYZA(data, pixel_count);
      } else {
        DCHECK_EQ(components_per_pixel, 4);
      }
      if (rgb_to_bgr)
        RGBAToBGRA(data, pixel_count);
    }
  }

  if (!is_cubemap) {
    // NOTE: we flip the images to respect max/maya's UV orientation.
    if (is_dxtc) {
      FlipDXTCImage(dds_width, dds_height, mip_count, format, image_data.get());
    } else {
      FlipBGRAImage(dds_width, dds_height, mip_count, format, image_data.get());
    }
  }

  if (final_mip_count > mip_count) {
    // Generate the full mip-map chain using the last mip-map read, for each
    // face.
    unsigned int base_mip_width = dds_width >> (mip_count - 1);
    unsigned int base_mip_height = dds_height >> (mip_count - 1);
    unsigned int base_mip_offset = GetMipChainSize(dds_width, dds_height,
                                                   format, mip_count - 1);
    for (unsigned int face = 0; face < num_faces; ++face) {
      unsigned char *data =
          image_data.get() + face_size * face + base_mip_offset;
      if (!GenerateMipmaps(base_mip_width, base_mip_height, format,
                           final_mip_count - mip_count, data)) {
        DLOG(ERROR) << "mip-map generation failed for \"" << filename << "\"";
        return false;
      }
    }
  }

  // Update the Bitmap member variables.
  image_data_.swap(image_data);
  format_ = format;
  width_  = dds_width;
  height_ = dds_height;
  num_mipmaps_ = final_mip_count;
  is_cubemap_ = is_cubemap;
  return true;
}

}  // namespace o3d
