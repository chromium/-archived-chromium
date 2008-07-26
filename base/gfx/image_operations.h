// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef BASE_GFX_IMAGE_OPERATIONS_H__
#define BASE_GFX_IMAGE_OPERATIONS_H__

#include "base/basictypes.h"
#include "base/gfx/rect.h"

class SkBitmap;

namespace gfx {

class ImageOperations {
 public:
  enum ResizeMethod {
    // Box filter. This is a weighted average of all of the pixels touching
    // the destination pixel. For enlargement, this is nearest neighbor.
    //
    // You probably don't want this, it is here for testing since it is easy to
    // compute. Use RESIZE_LANCZOS3 instead.
    RESIZE_BOX,

    // 3-cycle Lanczos filter. This is tall in the middle, goes negative on
    // each side, then oscillates 2 more times. It gives nice sharp edges.
    RESIZE_LANCZOS3,
  };

  // Resizes the given source bitmap using the specified resize method, so that
  // the entire image is (dest_size) big. The dest_subset is the rectangle in
  // this destination image that should actually be returned.
  //
  // The output image will be (dest_subset.width(), dest_subset.height()). This
  // will save work if you do not need the entire bitmap.
  //
  // The destination subset must be smaller than the destination image.
  static SkBitmap Resize(const SkBitmap& source,
                         ResizeMethod method,
                         const Size& dest_size,
                         const Rect& dest_subset);

  // Alternate version for resizing and returning the entire bitmap rather than
  // a subset.
  static SkBitmap Resize(const SkBitmap& source,
                         ResizeMethod method,
                         const Size& dest_size);


  // Create a bitmap that is a blend of two others. The alpha argument
  // specifies the opacity of the second bitmap. The provided bitmaps must
  // use have the kARGB_8888_Config config and be of equal dimensions.
  static SkBitmap CreateBlendedBitmap(const SkBitmap& first,
                                      const SkBitmap& second,
                                      double alpha);
 private:
  ImageOperations();  // Class for scoping only.
};

}  // namespace gfx

#endif  // BASE_GFX_IMAGE_OPERATIONS_H__
