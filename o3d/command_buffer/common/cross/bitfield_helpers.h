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


// This file contains a helper template class used to access bit fields in
// unsigned int_ts.

#ifndef O3D_COMMAND_BUFFER_COMMON_CROSS_BITFIELD_HELPERS_H__
#define O3D_COMMAND_BUFFER_COMMON_CROSS_BITFIELD_HELPERS_H__

namespace o3d {
namespace command_buffer {

// Bitfield template class, used to access bit fields in unsigned int_ts.
template<int shift, int length> class BitField {
 public:
  static const unsigned int kShift = shift;
  static const unsigned int kLength = length;
  // the following is really (1<<length)-1 but also work for length == 32
  // without compiler warning.
  static const unsigned int kMask = 1U + ((1U << (length-1)) - 1U) * 2U;

  // Gets the value contained in this field.
  static unsigned int Get(unsigned int container) {
    return (container >> kShift) & kMask;
  }

  // Makes a value that can be or-ed into this field.
  static unsigned int MakeValue(unsigned int value) {
    return (value & kMask) << kShift;
  }

  // Changes the value of this field.
  static void Set(unsigned int *container, unsigned int field_value) {
    *container = (*container & ~(kMask << kShift)) | MakeValue(field_value);
  }
};

}  // namespace command_buffer
}  // namespace o3d


#endif  // O3D_COMMAND_BUFFER_COMMON_CROSS_BITFIELD_HELPERS_H__
