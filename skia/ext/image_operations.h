// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_IMAGE_OPERATIONS_H_
#define SKIA_EXT_IMAGE_OPERATIONS_H_

#include "base/basictypes.h"
#include "base/gfx/rect.h"

class SkBitmap;

namespace skia {

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
                         int dest_width, int dest_height,
                         const gfx::Rect& dest_subset);

  // Alternate version for resizing and returning the entire bitmap rather than
  // a subset.
  static SkBitmap Resize(const SkBitmap& source,
                         ResizeMethod method,
                         int dest_width, int dest_height);


  // Create a bitmap that is a blend of two others. The alpha argument
  // specifies the opacity of the second bitmap. The provided bitmaps must
  // use have the kARGB_8888_Config config and be of equal dimensions.
  static SkBitmap CreateBlendedBitmap(const SkBitmap& first,
                                      const SkBitmap& second,
                                      double alpha);
 private:
  ImageOperations();  // Class for scoping only.
};

}  // namespace skia

#endif  // SKIA_EXT_IMAGE_OPERATIONS_H_

