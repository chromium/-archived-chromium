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
// loading. Trying to keep this class as independent from the OpenGL API in
// case they need retargeting later on.

// The precompiled header must appear before anything else.
#include "core/cross/precompile.h"

#include <cstring>
#include <sys/stat.h>
#include "core/cross/bitmap.h"
#include "utils/cross/file_path_utils.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "import/cross/raw_data.h"
#include "import/cross/memory_buffer.h"
#include "import/cross/memory_stream.h"

using file_util::OpenFile;
using file_util::CloseFile;
using file_util::GetFileSize;

namespace o3d {

// Gets the size of the buffer containing a an image, given its width, height
// and format.
unsigned int Bitmap::GetBufferSize(unsigned int width,
                                   unsigned int height,
                                   Texture::Format format) {
  DCHECK(CheckImageDimensions(width, height));
  unsigned int pixels = width * height;
  switch (format) {
    case Texture::XRGB8:
    case Texture::ARGB8:
      return 4 * sizeof(unsigned char) * pixels;  // NOLINT
    case Texture::ABGR16F:
      return 4 * sizeof(unsigned short) * pixels;  // NOLINT
    case Texture::R32F:
      return sizeof(float) * pixels;  // NOLINT
    case Texture::ABGR32F:
      return 4 * sizeof(float) * pixels;  // NOLINT
    case Texture::DXT1:
    case Texture::DXT3:
    case Texture::DXT5: {
      unsigned int blocks = ((width + 3) / 4) * ((height + 3) / 4);
      unsigned int bytes_per_block = format == Texture::DXT1 ? 8 : 16;
      return blocks * bytes_per_block;
    }
    case Texture::UNKNOWN_FORMAT:
      break;
  }
  // failed to find a matching format
  LOG(ERROR) << "Unrecognized Texture format type.";
  return 0;
}

// Gets the size of the buffer containing a mip-map chain, given its base
// width, height, format and number of mip-map levels.
unsigned int Bitmap::GetMipChainSize(unsigned int base_width,
                                     unsigned int base_height,
                                     Texture::Format format,
                                     unsigned int num_mipmaps) {
  DCHECK(CheckImageDimensions(base_width, base_height));
  unsigned int total_size = 0;
  unsigned int mip_width = base_width;
  unsigned int mip_height = base_height;
  for (unsigned int i = 0; i < num_mipmaps; ++i) {
    total_size += GetBufferSize(mip_width, mip_height, format);
    mip_width = std::max(1U, mip_width >> 1);
    mip_height = std::max(1U, mip_height >> 1);
  }
  return total_size;
}

void Bitmap::Allocate(Texture::Format format,
                      unsigned int width,
                      unsigned int height,
                      unsigned int num_mipmaps,
                      bool cube_map) {
  DCHECK(CheckImageDimensions(width, height));
  switch (format) {
    case Texture::XRGB8:
    case Texture::ARGB8:
    case Texture::ABGR16F:
    case Texture::R32F:
    case Texture::ABGR32F:
    case Texture::DXT1:
    case Texture::DXT3:
    case Texture::DXT5:
      break;
    default:
      DLOG(FATAL) << "Trying to allocate a bitmap with invalid format";
      break;
  }
  DCHECK(!cube_map || (width == height));
  DCHECK_LE(num_mipmaps, GetMipMapCount(width, height));
  DCHECK_GT(num_mipmaps, 0u);

  format_ = format;
  width_ = width;
  height_ = height;
  num_mipmaps_ = num_mipmaps;
  is_cubemap_ = cube_map;
  AllocateData();
}

unsigned char *Bitmap::GetMipData(unsigned int level,
                                  TextureCUBE::CubeFace face) const {
  DCHECK(level < num_mipmaps_);
  if (!image_data_.get()) return NULL;
  unsigned char *data = image_data_.get();
  if (is_cubemap_) {
    data += (face - TextureCUBE::FACE_POSITIVE_X) *
        GetMipChainSize(width_, height_, format_, num_mipmaps_);
  }
  return data + GetMipChainSize(width_, height_, format_, level);
}


bool Bitmap::LoadFromStream(MemoryReadStream *stream,
                            const String &filename,
                            ImageFileType file_type,
                            bool generate_mipmaps) {
  // If we don't know what type to load, try to detect it based on the file
  // name.
  if (file_type == UNKNOWN) {
    file_type = GetFileTypeFromFilename(filename.c_str());
  }

  switch (file_type) {
    case TGA:
      if (LoadFromTGAStream(stream, filename, generate_mipmaps)) return true;
      break;
    case DDS:
      if (LoadFromDDSStream(stream, filename, generate_mipmaps)) return true;
      break;
    case PNG:
      if (LoadFromPNGStream(stream, filename, generate_mipmaps)) return true;
      break;
    case JPEG:
      if (LoadFromJPEGStream(stream, filename, generate_mipmaps)) return true;
      break;
    case UNKNOWN:
    default:
      break;
  }

  // At this point we either could not detect the filetype, or possibly
  // the file extension was incorrect (eg. a JPEG image with a .png suffix)
  DLOG(INFO) << "Could not detect file type from filename \""
             << filename << "\". Trying all the loaders.";
  // We will try all the loaders, one by one, starting by the ones that can
  // have an early detect based on magic strings.  We Seek(0) after each try
  // since each attempt changes the stream read position.
  if (LoadFromDDSStream(stream, filename, generate_mipmaps)) return true;

  stream->Seek(0);
  if (LoadFromPNGStream(stream, filename, generate_mipmaps)) return true;

  stream->Seek(0);
  if (LoadFromJPEGStream(stream, filename, generate_mipmaps)) return true;

  stream->Seek(0);
  if (LoadFromTGAStream(stream, filename, generate_mipmaps)) return true;
  DLOG(ERROR) << "Failed to load image \"" << filename
              << "\": unknown file type";
  return false;
}

// Given an arbitrary bitmap file, load it all into memory and then call our
// stream loader
bool Bitmap::LoadFromFile(const FilePath &filepath,
                          ImageFileType file_type,
                          bool generate_mipmaps) {
  // Open the file
  String filename = FilePathToUTF8(filepath);
  FILE *file = OpenFile(filepath, "rb");

  if (!file) {
    DLOG(ERROR) << "bitmap file not found \"" << filename << "\"";
    return false;
  }

  // Determine the file's length
  int64 file_size64;
  if (!GetFileSize(filepath, &file_size64)) {
    DLOG(ERROR) << "error getting bitmap file size \"" << filename << "\"";
    CloseFile(file);
    return false;
  }
  if (file_size64 > 0xffffffffLL) {
    DLOG(ERROR) << "bitmap file is too large \"" << filename << "\"";
    return false;
  }
  size_t file_length = static_cast<size_t>(file_size64);

  // Load the compressed image data into memory
  MemoryBuffer<uint8> file_contents(file_length);
  uint8 *p = file_contents;
  if (fread(p, file_length, 1, file) != 1) {
    DLOG(ERROR) << "error reading bitmap file \"" << filename << "\"";
    return false;
  }

  // And create the bitmap from a memory stream
  MemoryReadStream stream(file_contents, file_length);
  bool result = LoadFromStream(&stream, filename, file_type, generate_mipmaps);
  CloseFile(file);

  return result;
}

// Given a RawData object containing image data in one of our known formats,
// decide which image format it is and call the correct loading function.
bool Bitmap::LoadFromRawData(RawData *raw_data,
                             ImageFileType file_type,
                             bool generate_mipmaps) {
  String filename = raw_data->uri();

  // GetData() returns NULL if it, for example, cannot open the temporary data
  // file. In that case, it invokes the error callback. We just have to be
  // careful not to dereference it.
  const uint8* data = raw_data->GetData();
  if (!data) {
    return false;
  }

  MemoryReadStream stream(data, raw_data->GetLength());

  return LoadFromStream(&stream, filename, file_type, generate_mipmaps);
}


Bitmap::ImageFileType Bitmap::GetFileTypeFromFilename(const char *filename) {
  // Convert the filename to lower case for matching.
  // NOTE: Surprisingly, "tolower" is not in the std namespace.
  String name(filename);
  std::transform(name.begin(), name.end(), name.begin(), ::tolower);

  // Dispatch loading functions based on filename extensions.
  String::size_type i = name.rfind(".");
  if (i == String::npos) {
    DLOG(INFO) << "Could not detect file type for image \""
               << filename << "\": no extension.";
    return UNKNOWN;
  }

  String extension = name.substr(i);
  if (extension == ".tga") {
    DLOG(INFO) << "Bitmap Found a TGA file : " << filename;
    return TGA;
  } else if (extension == ".dds") {
    DLOG(INFO) << "Bitmap Found a DDS file : " << filename;
    return DDS;
  } else if (extension == ".png") {
    DLOG(INFO) << "Bitmap Found a PNG file : " << filename;
    return PNG;
  } else if (extension == ".jpg" ||
             extension == ".jpeg" ||
             extension == ".jpe") {
    DLOG(INFO) << "Bitmap Found a JPEG file : " << filename;
    return JPEG;
  } else {
    return UNKNOWN;
  }
}

struct MimeTypeToFileType {
  const char *mime_type;
  Bitmap::ImageFileType file_type;
};

static const MimeTypeToFileType mime_type_map[] = {
  {"image/png", Bitmap::PNG},
  {"image/jpeg", Bitmap::JPEG},
  // No official MIME type for TGA or DDS.
};

Bitmap::ImageFileType Bitmap::GetFileTypeFromMimeType(const char *mime_type) {
  for (unsigned int i = 0; i < arraysize(mime_type_map); ++i) {
    if (!strcmp(mime_type, mime_type_map[i].mime_type))
      return mime_type_map[i].file_type;
  }
  return Bitmap::UNKNOWN;
}

void Bitmap::XYZToXYZA(unsigned char *image_data, int pixel_count) {
  // We do this pixel by pixel, starting from the end to avoid overlapping
  // problems.
  for (int i = pixel_count - 1; i >= 0; --i) {
    image_data[i*4+3] = 0xff;
    image_data[i*4+2] = image_data[i*3+2];
    image_data[i*4+1] = image_data[i*3+1];
    image_data[i*4+0] = image_data[i*3+0];
  }
}

void Bitmap::RGBAToBGRA(unsigned char *image_data, int pixel_count) {
  for (int i = 0; i < pixel_count; ++i) {
    unsigned char c = image_data[i*4+0];
    image_data[i*4+0] = image_data[i*4+2];
    image_data[i*4+2] = c;
  }
}

// Compute a texel, filtered from several source texels. This function assumes
// minification.
// Parameters:
//   x: x-coordinate of the destination texel in the destination image
//   y: y-coordinate of the destination texel in the destination image
//   dst_width: width of the destination image
//   dst_height: height of the destination image
//   dst_data: address of the destination image data
//   src_width: width of the source image
//   src_height: height of the source image
//   src_data: address of the source image data
//   components: number of components in the image.
static void FilterTexel(unsigned int x,
                        unsigned int y,
                        unsigned int dst_width,
                        unsigned int dst_height,
                        unsigned char *dst_data,
                        unsigned int src_width,
                        unsigned int src_height,
                        const unsigned char *src_data,
                        unsigned int components) {
  DCHECK(Bitmap::CheckImageDimensions(src_width, src_height));
  DCHECK(Bitmap::CheckImageDimensions(dst_width, dst_height));
  DCHECK_LE(dst_width, src_width);
  DCHECK_LE(dst_height, src_height);
  DCHECK_LE(x, dst_width);
  DCHECK_LE(y, dst_height);
  // the texel at (x, y) represents the square of texture coordinates
  // [x/dst_w, (x+1)/dst_w) x [y/dst_h, (y+1)/dst_h).
  // This takes contributions from the texels:
  // [floor(x*src_w/dst_w), ceil((x+1)*src_w/dst_w)-1]
  // x
  // [floor(y*src_h/dst_h), ceil((y+1)*src_h/dst_h)-1]
  // from the previous level.
  unsigned int src_min_x = (x*src_width)/dst_width;
  unsigned int src_max_x = ((x+1)*src_width+dst_width-1)/dst_width - 1;
  unsigned int src_min_y = (y*src_height)/dst_height;
  unsigned int src_max_y = ((y+1)*src_height+dst_height-1)/dst_height - 1;

  // Find the contribution of source each texel, by computing the coverage of
  // the destination texel on the source texel. We do all the computations in
  // fixed point, at a src_height*src_width factor to be able to use ints,
  // but keep all the precision.
  // Accumulators need to be 64 bits though, because src_height*src_width can
  // be 24 bits for a 4kx4k base, to which we need to multiply the component
  // value which is another 8 bits (and we need to accumulate several of them).

  // NOTE: all of our formats use at most 4 components per pixel.
  // Instead of dynamically allocating a buffer for each pixel on the heap,
  // just allocate the worst case on the stack.
  DCHECK_LE(components, 4u);
  uint64 accum[4] = {0};
  for (unsigned int src_x = src_min_x; src_x <= src_max_x; ++src_x) {
    for (unsigned int src_y = src_min_y; src_y <= src_max_y; ++src_y) {
      // The contribution of a fully covered texel is 1/(m_x*m_y) where m_x is
      // the x-dimension minification factor (src_width/dst_width) and m_y is
      // the y-dimenstion minification factor (src_height/dst_height).
      // If the texel is partially covered (on a border), the contribution is
      // proportional to the covered area. We compute it as the product of the
      // covered x-length by the covered y-length.

      unsigned int x_contrib = dst_width;
      if (src_x * dst_width < x * src_width) {
        // source texel is across the left border of the footprint of the
        // destination texel.
        x_contrib = (src_x + 1) * dst_width - x * src_width;
      } else if ((src_x + 1) * dst_width > (x+1) * src_width) {
        // source texel is across the right border of the footprint of the
        // destination texel.
        x_contrib = (x+1) * src_width - src_x * dst_width;
      }
      DCHECK(x_contrib > 0);
      DCHECK(x_contrib <= dst_width);
      unsigned int y_contrib = dst_height;
      if (src_y * dst_height < y * src_height) {
        // source texel is across the top border of the footprint of the
        // destination texel.
        y_contrib = (src_y + 1) * dst_height - y * src_height;
      } else if ((src_y + 1) * dst_height > (y+1) * src_height) {
        // source texel is across the bottom border of the footprint of the
        // destination texel.
        y_contrib = (y+1) * src_height - src_y * dst_height;
      }
      DCHECK(y_contrib > 0);
      DCHECK(y_contrib <= dst_height);
      unsigned int contrib = x_contrib * y_contrib;
      for (unsigned int c = 0; c < components; ++c) {
        accum[c] +=
            contrib * src_data[(src_y * src_width + src_x) * components + c];
      }
    }
  }
  for (unsigned int c = 0; c < components; ++c) {
    uint64 value = accum[c] / (src_height * src_width);
    DCHECK_LE(value, 255);
    dst_data[(y * dst_width + x) * components + c] =
        static_cast<unsigned char>(value);
  }
}

bool Bitmap::GenerateMipmaps(unsigned int base_width,
                             unsigned int base_height,
                             Texture::Format format,
                             unsigned int num_mipmaps,
                             unsigned char *data) {
  DCHECK(CheckImageDimensions(base_width, base_height));
  unsigned int components = 0;
  switch (format) {
    case Texture::XRGB8:
    case Texture::ARGB8:
      components = 4;
      break;
    case Texture::ABGR16F:
    case Texture::R32F:
    case Texture::ABGR32F:
    case Texture::DXT1:
    case Texture::DXT3:
    case Texture::DXT5:
    case Texture::UNKNOWN_FORMAT:
      DLOG(ERROR) << "Mip-map generation not supported for format: " << format;
      return false;
  }
  DCHECK_GE(std::max(base_width, base_height) >> (num_mipmaps-1), 1u);
  unsigned char *mip_data = data;
  unsigned int mip_width = base_width;
  unsigned int mip_height = base_height;
  for (unsigned int level = 1; level < num_mipmaps; ++level) {
    unsigned int prev_width = mip_width;
    unsigned int prev_height = mip_height;
    unsigned char *prev_data = mip_data;
    mip_data += components * mip_width * mip_height;
    DCHECK_EQ(mip_data, data + GetMipChainSize(base_width, base_height, format,
                                               level));
    mip_width = std::max(1U, mip_width >> 1);
    mip_height = std::max(1U, mip_height >> 1);

    if (mip_width * 2 == prev_width && mip_height * 2 == prev_height) {
      // Easy case: every texel maps to exactly 4 texels in the previous level.
      for (unsigned int y = 0; y < mip_height; ++y) {
        for (unsigned int x = 0; x < mip_width; ++x) {
          for (unsigned int c = 0; c < components; ++c) {
            // Average the 4 texels.
            unsigned int offset = (y*2*prev_width + x*2) * components + c;
            unsigned int value = prev_data[offset];            // (2x, 2y)
            value += prev_data[offset+components];             // (2x+1, 2y)
            value += prev_data[offset+prev_width*components];  // (2x, 2y+1)
            value +=
                prev_data[offset+(prev_width+1)*components];   // (2x+1, 2y+1)
            mip_data[(y*mip_width+x) * components + c] = value/4;
          }
        }
      }
    } else {
      for (unsigned int y = 0; y < mip_height; ++y) {
        for (unsigned int x = 0; x < mip_width; ++x) {
          FilterTexel(x, y, mip_width, mip_height, mip_data, prev_width,
                      prev_height, prev_data, components);
        }
      }
    }
  }

  return true;
}

// Scales the image using basic point filtering.
bool Bitmap::ScaleUpToPOT(unsigned int width,
                          unsigned int height,
                          Texture::Format format,
                          const unsigned char *src,
                          unsigned char *dst) {
  DCHECK(CheckImageDimensions(width, height));
  unsigned int components = 0;
  switch (format) {
    case Texture::XRGB8:
    case Texture::ARGB8:
      components = 4;
      break;
    case Texture::ABGR16F:
    case Texture::R32F:
    case Texture::ABGR32F:
    case Texture::DXT1:
    case Texture::DXT3:
    case Texture::DXT5:
    case Texture::UNKNOWN_FORMAT:
      DLOG(ERROR) << "Up-scaling is not supported for format: " << format;
      return false;
  }
  unsigned int pot_width = GetPOTSize(width);
  unsigned int pot_height = GetPOTSize(height);
  if (pot_width == width && pot_height == height && src == dst)
    return true;
  return Scale(width, height, format, src, pot_width, pot_height, dst);
}

// Scales the image using basic point filtering.
bool Bitmap::Scale(unsigned int src_width,
                   unsigned int src_height,
                   Texture::Format format,
                   const unsigned char *src,
                   unsigned int dst_width,
                   unsigned int dst_height,
                   unsigned char *dst) {
  DCHECK(CheckImageDimensions(src_width, src_height));
  DCHECK(CheckImageDimensions(dst_width, dst_height));
  unsigned int components = 0;
  switch (format) {
    case Texture::XRGB8:
    case Texture::ARGB8:
      components = 4;
      break;
    case Texture::ABGR16F:
    case Texture::R32F:
    case Texture::ABGR32F:
    case Texture::DXT1:
    case Texture::DXT3:
    case Texture::DXT5:
    case Texture::UNKNOWN_FORMAT:
      DLOG(ERROR) << "Up-scaling is not supported for format: " << format;
      return false;
  }
  // Start from the end to be able to do it in place.
  for (unsigned int y = dst_height - 1; y < dst_height; --y) {
    // max value for y is dst_height - 1, which makes :
    // base_y = (2*dst_height - 1) * src_height / (2 * dst_height)
    // which is < src_height.
    unsigned int base_y = ((y * 2 + 1) * src_height) / (dst_height * 2);
    DCHECK_LT(base_y, src_height);
    for (unsigned int x = dst_width - 1; x < dst_width; --x) {
      unsigned int base_x = ((x * 2 + 1) * src_width) / (dst_width * 2);
      DCHECK_LT(base_x, src_width);
      for (unsigned int c = 0; c < components; ++c) {
        dst[(y * dst_width + x) * components + c] =
            src[(base_y * src_width + base_x) * components + c];
      }
    }
  }
  return true;
}

// Checks that all the alpha values are 1.0
bool Bitmap::CheckAlphaIsOne() const {
  if (!image_data())
    return false;

  switch (format()) {
    case Texture::XRGB8:
      return true;
    case Texture::ARGB8: {
      int faces = is_cubemap() ? 6 : 1;
      for (int face = 0; face < faces; ++face) {
        for (unsigned int level = 0; level < num_mipmaps(); ++level) {
          const unsigned char *data = GetMipData(
              level,
              static_cast<TextureCUBE::CubeFace>(face)) + 3;
          const unsigned char* end = data + Bitmap::GetBufferSize(
              std::max(1U, width() >> level),
              std::max(1U, height() >> level),
              format());
          while (data < end) {
            if (*data != 255) {
              return false;
            }
            data += 4;
          }
        }
      }
      break;
    }
    case Texture::DXT1: {
      int faces = is_cubemap() ? 6 : 1;
      for (int face = 0; face < faces; ++face) {
        for (unsigned int level = 0; level < num_mipmaps(); ++level) {
          const unsigned char *data = GetMipData(
              level,
              static_cast<TextureCUBE::CubeFace>(face));
          const unsigned char* end = data + Bitmap::GetBufferSize(
              std::max(1U, width() >> level),
              std::max(1U, height() >> level),
              format());
          DCHECK((end - data) % 8 == 0);
          while (data < end) {
            int color0 = static_cast<int>(data[0]) |
                         static_cast<int>(data[1]) << 8;
            int color1 = static_cast<int>(data[2]) |
                         static_cast<int>(data[3]) << 8;
            if (color0 < color1) {
              return false;
            }
            data += 8;
          }
        }
      }
      break;
    }
    case Texture::DXT3:
    case Texture::DXT5:
      return false;
    case Texture::ABGR16F: {
      int faces = is_cubemap() ? 6 : 1;
      for (int face = 0; face < faces; ++face) {
        for (unsigned int level = 0; level < num_mipmaps(); ++level) {
          const unsigned char *data = GetMipData(
              level,
              static_cast<TextureCUBE::CubeFace>(face)) + 6;
          const unsigned char* end = data + Bitmap::GetBufferSize(
              std::max(1U, width() >> level),
              std::max(1U, height() >> level),
              format());
          while (data < end) {
            if (data[0] != 0x00 || data[1] != 0x3C) {
              return false;
            }
            data += 8;
          }
        }
      }
      break;
    }
    case Texture::R32F:
      return true;
    case Texture::ABGR32F: {
      int faces = is_cubemap() ? 6 : 1;
      for (int face = 0; face < faces; ++face) {
        for (unsigned int level = 0; level < num_mipmaps(); ++level) {
          const unsigned char* data = GetMipData(
              level,
              static_cast<TextureCUBE::CubeFace>(face)) + 12;
          const unsigned char* end = data + Bitmap::GetBufferSize(
              std::max(1U, width() >> level),
              std::max(1U, height() >> level),
              format());
          while (data < end) {
            if (*(reinterpret_cast<const float*>(data)) != 1.0f) {
              return false;
            }
            data += 16;
          }
        }
      }
      break;
    }
    case Texture::UNKNOWN_FORMAT:
      return false;
  }
  return true;
}

}  // namespace o3d
