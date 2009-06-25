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


// This file contains the implementation of the helper functions for resources.

#include "command_buffer/common/cross/resource.h"

namespace o3d {
namespace command_buffer {

namespace texture {

// Gets the number of bytes per block for a given format.
unsigned int GetBytesPerBlock(Format format) {
  switch (format) {
    case XRGB8:
    case ARGB8:
      return 4;
    case ABGR16F:
      return 8;
    case DXT1:
      return 8;
    default:
      LOG(FATAL) << "Invalid format";
      return 1;
  }
}

// Gets the width of a block for a given format.
unsigned int GetBlockSizeX(Format format) {
  switch (format) {
    case XRGB8:
    case ARGB8:
    case ABGR16F:
      return 1;
    case DXT1:
      return 4;
    default:
      LOG(FATAL) << "Invalid format";
      return 1;
  }
}

// Gets the height of a block for a given format.
unsigned int GetBlockSizeY(Format format) {
  // NOTE: currently only supported formats use square blocks.
  return GetBlockSizeX(format);
}

}  // namespace texture

namespace effect_param {

// Gets the size of the data of a given parameter type.
unsigned int GetDataSize(DataType type) {
  switch (type) {
    case UNKNOWN:
      return 0;
    case FLOAT1:
      return sizeof(float);  // NOLINT
    case FLOAT2:
      return sizeof(float)*2;  // NOLINT
    case FLOAT3:
      return sizeof(float)*3;  // NOLINT
    case FLOAT4:
      return sizeof(float)*4;  // NOLINT
    case MATRIX4:
      return sizeof(float)*16;  // NOLINT
    case INT:
      return sizeof(int);  // NOLINT
    case BOOL:
      return sizeof(bool);  // NOLINT
    case SAMPLER:
      return sizeof(ResourceID);  // NOLINT
    case TEXTURE:
      return sizeof(ResourceID);  // NOLINT
    default:
      LOG(FATAL) << "Invalid type.";
      return 0;
  }
}

}  // namespace effect_param

}  // namespace command_buffer
}  // namespace o3d
