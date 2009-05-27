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


// This file contains the implementation of some utilities for textures.

#include <stdlib.h>
#include "command_buffer/service/cross/texture_utils.h"

namespace o3d {
namespace command_buffer {

void MakeTransferInfo(TransferInfo *transfer_info,
                      const MipLevelInfo &mip_level,
                      const Volume &volume,
                      unsigned int row_pitch,
                      unsigned int slice_pitch) {
  transfer_info->row_pitch = row_pitch;
  transfer_info->slice_pitch = slice_pitch;
  transfer_info->row_size =
      volume.width / mip_level.block_size_x * mip_level.block_bpp;
  transfer_info->slice_size = transfer_info->row_size +
      (volume.height / mip_level.block_size_y - 1) * row_pitch;
  transfer_info->total_size = transfer_info->slice_size +
      (volume.depth - 1) * slice_pitch;
  transfer_info->packed = (transfer_info->row_size == row_pitch) &&
      (volume.depth == 1 || transfer_info->slice_size == slice_pitch);
}

void MakePackedTransferInfo(TransferInfo *transfer_info,
                            const MipLevelInfo &mip_level,
                            const Volume &volume) {
  transfer_info->row_size =
      volume.width / mip_level.block_size_x * mip_level.block_bpp;
  transfer_info->row_pitch = transfer_info->row_size;
  transfer_info->slice_size =
      volume.height / mip_level.block_size_y * transfer_info->row_pitch;
  transfer_info->slice_pitch = transfer_info->slice_size;
  transfer_info->total_size = volume.depth * transfer_info->slice_pitch;
  transfer_info->packed = true;
}

// Transfers a volume of texels.
void TransferVolume(const Volume &volume,
                    const MipLevelInfo &mip_level,
                    const TransferInfo &dst_transfer_info,
                    void *dst_data,
                    const TransferInfo &src_transfer_info,
                    const void *src_data) {
  DCHECK_EQ(src_transfer_info.row_size, dst_transfer_info.row_size);
  if (src_transfer_info.packed && dst_transfer_info.packed) {
    // fast path
    DCHECK_EQ(src_transfer_info.total_size, dst_transfer_info.total_size);
    DCHECK_EQ(src_transfer_info.row_pitch, dst_transfer_info.row_pitch);
    DCHECK_EQ(src_transfer_info.slice_pitch, dst_transfer_info.slice_pitch);
    memcpy(dst_data, src_data, src_transfer_info.total_size);
  } else {
    const char *src = static_cast<const char *>(src_data);
    char *dst = static_cast<char *>(dst_data);
    for (unsigned int slice = 0; slice < volume.depth; ++slice) {
      const char *row_src = src;
      char *row_dst = dst;
      for (unsigned int row = 0; row < volume.height;
           row += mip_level.block_size_y) {
        memcpy(row_dst, row_src, src_transfer_info.row_size);
        row_src += src_transfer_info.row_pitch;
        row_dst += dst_transfer_info.row_pitch;
      }
      src += src_transfer_info.slice_pitch;
      dst += dst_transfer_info.slice_pitch;
    }
  }
}

}  // namespace command_buffer
}  // namespace o3d
