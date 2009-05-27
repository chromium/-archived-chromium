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


// This file defines some bit utilities.

#ifndef O3D_BASE_CROSS_BITS_H_
#define O3D_BASE_CROSS_BITS_H_

#include "base/logging.h"

namespace o3d {
namespace base {
namespace bits {

// Returns the integer i such as 2^i <= n < 2^(i+1)
static inline int Log2Floor(unsigned int n) {
  if (n == 0)
    return -1;
  int log = 0;
  unsigned int value = n;
  for (int i = 4; i >= 0; --i) {
    int shift = (1 << i);
    unsigned int x = value >> shift;
    if (x != 0) {
      value = x;
      log += shift;
    }
  }
  DCHECK_EQ(value, 1);
  return log;
}

// Returns the integer i such as 2^(i-1) < n <= 2^i
static inline int Log2Ceiling(unsigned int n) {
  if (n == 0) {
    return -1;
  } else {
    // Log2Floor returns -1 for 0, so the following works correctly for n=1.
    return 1 + Log2Floor(n - 1);
  }
}

}  // namespace bits
}  // namespace base
}  // namespace o3d

#endif  // O3D_BASE_CROSS_BITS_H_
