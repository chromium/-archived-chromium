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


// This file contains the declaration of Bitmap helper class that can load
// raw 24- and 32-bit bitmaps from popular image formats. The Bitmap class
// also interprets the file format to record the correct OpenGL buffer format.
//
// Trying to keep this class independent from the OpenGL API in case they
// need retargeting later on.

#ifndef O3D_CORE_CROSS_BITMAP_H_
#define O3D_CORE_CROSS_BITMAP_H_

#include <stdlib.h>

#include "base/cross/bits.h"
#include "core/cross/types.h"
#include "core/cross/texture.h"

class FilePath;

namespace o3d {

class MemoryReadStream;
class RawData;

class Bitmap {
 public:

  // We will fail to load images that are bigger than 4kx4k to avoid security
  // risks. GPUs don't usually support bigger sizes anyway.
  // The biggest bitmap buffer size with these dimensions is:
  // 4k x 4k x 4xsizeof(float) x6 x4/3 (x6 for cube maps, x4/3 for mipmaps)
  // That makes 2GB, representable in an unsigned int, so we will avoid wraps.
  static const unsigned int kMaxImageDimension = 4096;
  enum ImageFileType {
    UNKNOWN,
    TGA,
    JPEG,
    PNG,
    DDS,
  };

  Bitmap()
      : image_data_(NULL),
        format_(Texture::UNKNOWN_FORMAT),
        width_(0),
        height_(0),
        num_mipmaps_(0),
        is_cubemap_(false) {}
  ~Bitmap() {}

  static bool CheckImageDimensions(unsigned int width, unsigned int height) {
    return width > 0 && height > 0 &&
        width <= kMaxImageDimension && height < kMaxImageDimension;
  }

  // Creates a copy of a bitmap, copying the pixels as well.
  // Parameters:
  //   source: the source bitmap.
  void CopyDeepFrom(const Bitmap &source) {
    Allocate(source.format_, source.width_, source.height_,
             source.num_mipmaps_, source.is_cubemap_);
    memcpy(image_data(), source.image_data(), GetTotalSize());
  }

  // Sets the bitmap parameters from another bitmap, stealing the pixel buffer
  // from the source bitmap.
  // Parameters:
  //   source: the source bitmap.
  void SetFrom(Bitmap *source) {
    image_data_.reset();
    format_ = source->format_;
    width_ = source->width_;
    height_ = source->height_;
    num_mipmaps_ = source->num_mipmaps_;
    is_cubemap_ = source->is_cubemap_;
    image_data_.swap(source->image_data_);
  }

  // Allocates an uninitialized bitmap with specified parameters.
  // Parameters:
  //   format: the format of the pixels.
  //   width: the width of the base image.
  //   height: the height of the base image.
  //   num_mipmaps: the number of mip-maps.
  //   cube_map: true if creating a cube map.
  void Allocate(Texture::Format format,
                unsigned int width,
                unsigned int height,
                unsigned int num_mipmaps,
                bool cube_map);

  // Allocates a bitmap with initialized parameters.
  // data is zero-initialized
  void AllocateData() {
    image_data_.reset(new unsigned char[GetTotalSize()]);
    memset(image_data_.get(), 0, GetTotalSize());
  }

  // Frees the data owned by the bitmap.
  void FreeData() {
    image_data_.reset(NULL);
  }

  // Gets the total size of the bitmap data, counting all faces and mip levels.
  unsigned int GetTotalSize() {
    return (is_cubemap_ ? 6 : 1) *
        GetMipChainSize(width_, height_, format_, num_mipmaps_);
  }

  // Computes the number of bytes of a texture pixel buffer.
  static unsigned int GetBufferSize(unsigned int width,
                                    unsigned int height,
                                    Texture::Format format);

  // Gets the image data for a given mip-map level and cube map face.
  // Parameters:
  //   level: mip level to get.
  //   face: face of cube to get. This parameter is ignored if
  //       this bitmap is not a cube map.
  unsigned char *GetMipData(unsigned int level,
                            TextureCUBE::CubeFace face) const;

  unsigned char *image_data() const { return image_data_.get(); }
  Texture::Format format() const { return format_; }
  unsigned int width() const { return width_; }
  unsigned int height() const { return height_; }
  unsigned int num_mipmaps() const { return num_mipmaps_; }
  bool is_cubemap() const { return is_cubemap_; }

  // Returns whether or not the dimensions of the bitmap are power-of-two.
  bool IsPOT() const {
    return ((width_ & (width_ - 1)) == 0) && ((height_ & (height_ - 1)) == 0);
  }

  void set_format(Texture::Format format) { format_ = format; }
  void set_width(unsigned int n) { width_ = n; }
  void set_height(unsigned int n) { height_ = n; }
  void set_num_mipmaps(unsigned int n) { num_mipmaps_ = n; }
  void set_is_cubemap(bool is_cubemap) { is_cubemap_ = is_cubemap; }

  // Loads a bitmap from a file.
  // Parameters:
  //   filename: the name of the file to load.
  //   file_type: the type of file to load. If UNKNOWN, the file type will be
  //              determined from the filename extension, and if it is not a
  //              known extension, all the loaders will be tried.
  //   generate_mipmaps: whether or not to generate all the mip-map levels.
  bool LoadFromFile(const FilePath &filepath,
                    ImageFileType file_type,
                    bool generate_mipmaps);

  // Loads a bitmap from a RawData object.
  // Parameters:
  //   raw_data: contains the bitmap data in one of the known formats
  //   file_type: the format of the bitmap data. If UNKNOWN, the file type
  //              will be determined from the extension from raw_data's uri
  //              and if it is not a known extension, all the loaders will
  //              be tried.
  //   generate_mipmaps: whether or not to generate all the mip-map levels.
  bool LoadFromRawData(RawData *raw_data,
                       ImageFileType file_type,
                       bool generate_mipmaps);

  // Loads a bitmap from a MemoryReadStream.
  // Parameters:
  //   stream: a stream for the bitmap data in one of the known formats
  //   filename: a filename (or uri) of the original bitmap data
  //             (may be an empty string)
  //   file_type: the format of the bitmap data. If UNKNOWN, the file type
  //              will be determined from the extension of |filename|
  //              and if it is not a known extension, all the loaders
  //              will be tried.
  //   generate_mipmaps: whether or not to generate all the mip-map levels.
  bool LoadFromStream(MemoryReadStream *stream,
                      const String &filename,
                      ImageFileType file_type,
                      bool generate_mipmaps);

  bool LoadFromPNGStream(MemoryReadStream *stream,
                         const String &filename,
                         bool generate_mipmaps);

  bool LoadFromTGAStream(MemoryReadStream *stream,
                         const String &filename,
                         bool generate_mipmaps);

  bool LoadFromDDSStream(MemoryReadStream *stream,
                         const String &filename,
                         bool generate_mipmaps);

  bool LoadFromJPEGStream(MemoryReadStream *stream,
                          const String &filename,
                          bool generate_mipmaps);

  // Saves to a PNG file. The image must be of the ARGB8 format, be a 2D image
  // with no mip-maps (only the base level).
  // Parameters:
  //   filename: the name of the file to into.
  // Returns:
  //   true if successful.
  bool SaveToPNGFile(const char* filename);

  // Checks that the alpha channel for the entire bitmap is 1.0
  bool CheckAlphaIsOne() const;

  // Detects the type of image file based on the filename.
  static ImageFileType GetFileTypeFromFilename(const char *filename);
  // Detects the type of image file based on the mime-type.
  static ImageFileType GetFileTypeFromMimeType(const char *mime_type);

  // Adds filler alpha byte (0xff) after every pixel. Assumes buffer was
  // allocated with enough storage)
  // can convert RGB -> RGBA, BGR -> BGRA, etc.
  static void XYZToXYZA(unsigned char *image_data, int pixel_count);

  // Swaps Red and Blue components in the image.
  static void RGBAToBGRA(unsigned char *image_data, int pixel_count);

  // Gets the number of mip-maps required for a full chain starting at
  // width x height.
  static unsigned int GetMipMapCount(unsigned int width, unsigned int height) {
    return 1 + base::bits::Log2Floor(std::max(width, height));
  }

  // Gets the smallest power-of-two value that is at least as high as
  // dimension. This is the POT dimension used in ScaleUpToPOT.
  static unsigned int GetPOTSize(unsigned int dimension) {
    return 1 << base::bits::Log2Ceiling(dimension);
  }

  // Gets the size of the buffer containing a mip-map chain, given its base
  // width, height, format and number of mip-map levels.
  static unsigned int GetMipChainSize(unsigned int base_width,
                                      unsigned int base_height,
                                      Texture::Format format,
                                      unsigned int num_mipmaps);

  // Generates mip-map levels for a single image, using the data from the base
  // level.
  // NOTE: this doesn't work for DXTC, or floating-point images.
  //
  // Parameters:
  //   base_width: the width of the base image.
  //   base_height: the height of the base image.
  //   format: the format of the data.
  //   num_mipmaps: the number of mipmaps to generate.
  //   data: the data containing the base image, and enough space for the
  //   mip-maps.
  static bool GenerateMipmaps(unsigned int base_width,
                              unsigned int base_height,
                              Texture::Format format,
                              unsigned int num_mipmaps,
                              unsigned char *data);

  // Scales an image up to power-of-two textures, using point filtering.
  // NOTE: this doesn't work for DXTC, or floating-point images.
  //
  // Parameters:
  //   width: the non-power-of-two width of the original image.
  //   height: the non-power-of-two height of the original image.
  //   format: the format of the data.
  //   src: the data containing the source data of the original image.
  //   dst: a buffer with enough space for the power-of-two version. Pixels are
  //   written from the end to the beginning so dst can be the same buffer as
  //   src.
  static bool ScaleUpToPOT(unsigned int width,
                           unsigned int height,
                           Texture::Format format,
                           const unsigned char *src,
                           unsigned char *dst);

  // Scales an image to an arbitrary size, using point filtering.
  // NOTE: this doesn't work for DXTC, or floating-point images.
  //
  // Parameters:
  //   src_width: the width of the original image.
  //   src_height: the height of the original image.
  //   format: the format of the data.
  //   src: the data containing the source data of the original image.
  //   dst_width: the width of the target image.
  //   dst_height: the height of the target image.
  //   dst: a buffer with enough space for the target version. Pixels are
  //   written from the end to the beginning so dst can be the same buffer as
  //   src if the transformation is an upscaling.
  static bool Scale(unsigned int src_width,
                    unsigned int src_height,
                    Texture::Format format,
                    const unsigned char *src,
                    unsigned int dst_width,
                    unsigned int dst_height,
                    unsigned char *dst);
 private:
  // pointer to the raw bitmap data
  scoped_array<uint8> image_data_;
  // format of the texture this is meant to represent.
  Texture::Format format_;
  // width of the bitmap in pixels.
  unsigned int width_;
  // height of the bitmap in pixels.
  unsigned int height_;
  // number of mipmap levels in this texture.
  unsigned int num_mipmaps_;
  // is this cube-map data
  bool is_cubemap_;

  DISALLOW_COPY_AND_ASSIGN(Bitmap);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_BITMAP_H_
