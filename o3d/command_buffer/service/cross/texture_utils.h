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


// This file declares some utilities for textures, in particular to deal with
// in-memory texture data and layout.

#ifndef O3D_COMMAND_BUFFER_SERVICE_CROSS_TEXTURE_UTILS_H_
#define O3D_COMMAND_BUFFER_SERVICE_CROSS_TEXTURE_UTILS_H_

#include "command_buffer/common/cross/logging.h"
#include "command_buffer/common/cross/resource.h"

namespace o3d {
namespace command_buffer {

// Structure describing a volume of pixels.
struct Volume {
  unsigned int x;
  unsigned int y;
  unsigned int z;
  unsigned int width;
  unsigned int height;
  unsigned int depth;
};

// Structure describing the dimensions and structure of a mip level.
struct MipLevelInfo {
  unsigned int block_bpp;
  unsigned int block_size_x;
  unsigned int block_size_y;
  unsigned int width;
  unsigned int height;
  unsigned int depth;
};

// Structure describing a memory layout for transfers.
struct TransferInfo {
  unsigned int row_size;     // size in bytes of a row of blocks.
  unsigned int row_pitch;    // number of bytes between 2 successive rows.
  unsigned int slice_size;   // size in bytes of a slice of data.
  unsigned int slice_pitch;  // number of bytes between 2 successive slices.
  unsigned int total_size;   // total size of the data.
  bool packed;               // indicates whether the data is tightly packed.
};

// Round a value up, so that it is divisible by the block size.
static inline unsigned int RoundToBlockSize(unsigned int base,
                                            unsigned int block) {
  DCHECK_GT(base, 0);
  DCHECK_GT(block, 0);
  return block + base - 1 - (base - 1) % block;
}

// Fills a MipLevelInfo structure from the base texture dimensions.
static inline void MakeMipLevelInfo(MipLevelInfo *mip_info,
                                    texture::Format format,
                                    unsigned int base_width,
                                    unsigned int base_height,
                                    unsigned int base_depth,
                                    unsigned int level) {
  mip_info->block_bpp = texture::GetBytesPerBlock(format);
  mip_info->block_size_x = texture::GetBlockSizeX(format);
  mip_info->block_size_y = texture::GetBlockSizeY(format);
  mip_info->width = RoundToBlockSize(
      texture::GetMipMapDimension(base_width, level), mip_info->block_size_x);
  mip_info->height = RoundToBlockSize(
      texture::GetMipMapDimension(base_height, level), mip_info->block_size_y);
  mip_info->depth = texture::GetMipMapDimension(base_depth, level);
}

// Gets the size in bytes of a mip level.
static inline unsigned int GetMipLevelSize(const MipLevelInfo &mip_info) {
  return mip_info.block_bpp * mip_info.width / mip_info.block_size_x *
      mip_info.height / mip_info.block_size_y * mip_info.depth;
}

// Checks that [x .. x+width] is contained in [0 .. mip_width], and that both x
// and width are divisible by block_size, and that width is positive.
static inline bool CheckDimension(unsigned int x,
                                  unsigned int width,
                                  unsigned int mip_width,
                                  unsigned int block_size) {
  return x < mip_width && x+width <= mip_width && x % block_size == 0 &&
      width % block_size == 0 && width > 0;
}

// Checks that given volume fits into a mip level.
static inline bool CheckVolume(const MipLevelInfo &mip_info,
                               const Volume &volume) {
  return CheckDimension(volume.x, volume.width, mip_info.width,
                        mip_info.block_size_x) &&
      CheckDimension(volume.y, volume.height, mip_info.height,
                     mip_info.block_size_y) &&
      CheckDimension(volume.z, volume.depth, mip_info.depth, 1);
}

// Checks whether a volume fully maps a mip level.
static inline bool IsFullVolume(const MipLevelInfo &mip_info,
                                const Volume &volume) {
  return (volume.x == 0) && (volume.y == 0) && (volume.z == 0) &&
      (volume.width == mip_info.width) &&
      (volume.height == mip_info.height) &&
      (volume.depth == mip_info.depth);
}

// Makes a transfer info from a mip level, a volume and row/slice pitches.
void MakeTransferInfo(TransferInfo *transfer_info,
                      const MipLevelInfo &mip_level,
                      const Volume &volume,
                      unsigned int row_pitch,
                      unsigned int slice_pitch);

// Makes a transfer info from a mip level and a volume, considering packed data.
void MakePackedTransferInfo(TransferInfo *transfer_info,
                            const MipLevelInfo &mip_level,
                            const Volume &volume);

// Transfers a volume of texels.
void TransferVolume(const Volume &volume,
                    const MipLevelInfo &mip_level,
                    const TransferInfo &dst_transfer_info,
                    void *dst_data,
                    const TransferInfo &src_transfer_info,
                    const void *src_data);

}  // namespace command_buffer
}  // namespace o3d

#endif  // O3D_COMMAND_BUFFER_SERVICE_CROSS_TEXTURE_UTILS_H_
