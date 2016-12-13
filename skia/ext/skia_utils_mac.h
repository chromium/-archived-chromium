// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SKIA_UTILS_MAC_H_
#define SKIA_EXT_SKIA_UTILS_MAC_H_

#include <CoreGraphics/CGColor.h>

#include "third_party/skia/include/core/SkColor.h"

struct SkMatrix;
struct SkIRect;
struct SkPoint;
struct SkRect;
class SkBitmap;
typedef struct _NSSize NSSize;

#ifdef __OBJC__
@class NSImage;
#endif

namespace gfx {

// Converts a Skia point to a CoreGraphics CGPoint.
// Both use same in-memory format.
inline const CGPoint& SkPointToCGPoint(const SkPoint& point) {
  return reinterpret_cast<const CGPoint&>(point);
}

// Converts a CoreGraphics point to a Skia CGPoint.
// Both use same in-memory format.
inline const SkPoint& CGPointToSkPoint(const CGPoint& point) {
  return reinterpret_cast<const SkPoint&>(point);
}

// Matrix converters.
CGAffineTransform SkMatrixToCGAffineTransform(const SkMatrix& matrix);

// Rectangle converters.
SkRect CGRectToSkRect(const CGRect& rect);
SkIRect CGRectToSkIRect(const CGRect& rect);

// Converts a Skia rect to a CoreGraphics CGRect.
CGRect SkIRectToCGRect(const SkIRect& rect);
CGRect SkRectToCGRect(const SkRect& rect);

// Converts CGColorRef to the ARGB layout Skia expects.
SkColor CGColorRefToSkColor(CGColorRef color);

// Converts ARGB to CGColorRef.
CGColorRef SkColorToCGColorRef(SkColor color);

// Converts a CGImage to a SkBitmap.
SkBitmap CGImageToSkBitmap(CGImageRef image);

#ifdef __OBJC__
// Draws an NSImage with a given size into a SkBitmap.
SkBitmap NSImageToSkBitmap(NSImage* image, NSSize size, bool is_opaque);

// Given an SkBitmap, return an autoreleased NSImage.
NSImage* SkBitmapToNSImage(const SkBitmap& icon);
#endif

}  // namespace gfx

#endif  // SKIA_EXT_SKIA_UTILS_MAC_H_
