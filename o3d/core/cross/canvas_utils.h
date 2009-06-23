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


#ifndef O3D_CORE_CROSS_CANVAS_UTILS_H_
#define O3D_CORE_CROSS_CANVAS_UTILS_H_

#include "core/cross/float_n.h"
#include "third_party/skia/include/core/SkColor.h"

// Helper function to convert from Float4 to an SkColor
static SkColor Float4ToSkColor(const o3d::Float4& color) {
  unsigned char red = static_cast<unsigned char>(color[0] * 255.0f);
  unsigned char green = static_cast<unsigned char>(color[1] * 255.0f);
  unsigned char blue = static_cast<unsigned char>(color[2] * 255.0f);
  unsigned char alpha = static_cast<unsigned char>(color[3] * 255.0f);
  return SkColorSetARGB(static_cast<U8CPU>(alpha),
                        static_cast<U8CPU>(red),
                        static_cast<U8CPU>(green),
                        static_cast<U8CPU>(blue));
}

#endif  // O3D_CORE_CROSS_CANVAS_UTILS_H_
